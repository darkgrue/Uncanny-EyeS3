/**
 * @file main.cpp
 * @brief Main entry point for the Uncanny Eyes ESP32 firmware.
 *
 * Initializes display, power management (SY6970), input devices
 * (WiiChuck, LightSensor, GestureFaceInput), and network (ESP-NOW). Runs the eye
 * animation at ~120 FPS on a dedicated FreeRTOS task pinned to
 * Core 1, with a lightweight status loop on loop().
 *
 * Supported boards:
 * - LilyGo T-Display S3 AMOLED (CO5300 via QSPI)
 * - LilyGo T-RGB (ST7701S via DPI)
 */
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include "common/EyeState.h"
#include "eye/EyeConfig.h"
#include "common/DisplayGeometry.h"
#include "common/DisplayHAL.h"
#include "display/AMOLEDDisplay.h"
#include "display/TRGBDisplay.h"
#include "eye/EyeAnimator.h"
#include "input/WiiChuck.h"
#include "input/LightSensor.h"
#include "input/GestureFaceInput.h"
#include "network/EyeSync.h"
#include "debug/DebugOverlay.h"
#include "eyes.h"
#include "EyeLibrary.h"
#include "BoardPins.h"
#include <Wire.h>
#include <esp_wifi.h>
#include "driver/i2c_master.h"

#define CURRENT_EYE default_eye::eye

// #define DEBUG_FPS_ENABLED           // Comment out to suppress FPS diagnostic messages on serial
// #define PCF85063_DIAGNOSTIC_ENABLED // Comment out to suppress PCF85063 diagnostic status output on serial
// #define SY6970_DIAGNOSTIC_ENABLED   // Comment out to suppress SY6970 diagnostic status output on serial

#ifdef DEBUG_FPS_ENABLED
static uint32_t s_frameCount = 0;
static uint32_t s_fpsTimer = 0;
static uint32_t s_currentFps = 0;
#endif

static EyeProjectConfig s_config;
static EyeAnimator *s_animator = nullptr;
static DisplayHAL *s_display = nullptr;
static WiiChuckInput *s_wiiChuck = nullptr;
static GestureFaceInput *s_gestureFace = nullptr;
static EyeSyncManager *s_syncManager = nullptr;
static EyeInterpolator *s_interpolator = nullptr;
static LightSensor *s_lightSensor = nullptr;

#ifdef DEBUG_OVERLAY_ENABLED
static DebugOverlay s_debugOverlay;
#endif

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

std::unique_ptr<Arduino_IIC> SY6970(new Arduino_SY6970(IIC_Bus, SY6970_DEVICE_ADDRESS,
                                                       DRIVEBUS_DEFAULT_VALUE, DRIVEBUS_DEFAULT_VALUE));

std::unique_ptr<Arduino_IIC> PCF85063(new Arduino_PCF85063(IIC_Bus, PCF85063_DEVICE_ADDRESS,
                                                           DRIVEBUS_DEFAULT_VALUE, DRIVEBUS_DEFAULT_VALUE));

void renderLoopTask(void *param);

/**
 * @brief Scan for I2C devices connected to a given I2C bus.
 *
 * @param wire Pointer to the TwoWire instance (e.g., &Wire or &Wire1).
 * @param name String identifier for the bus (e.g., "Wire", "Wire1").
 * @return void
 */
void debug_I2Cscan(TwoWire *wire, const char *name)
{
  Serial.printf("Scanning %s bus...\n", name);

  uint8_t nDevices = 0;
  for (uint8_t address = 8; address < 127; address++)
  {
    wire->beginTransmission(address);
    uint8_t error = wire->endTransmission();

    if (error == 0)
    {
      Serial.print("  I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("  Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found.");
  else
    Serial.printf("Done. Found %u device(s).\n", nDevices);
}

/**
 * @brief Initialize the display based on the detected board.
 *
 * Instantiates either AMOLEDDisplay or TRGBDisplay and stores the
 * pointer in s_display. Configures dimensions in s_config.
 */
void setupDisplay()
{
#if ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED
  static AMOLEDDisplay display;
  if (!display.begin())
  {
    Serial.println("T-Display S3 AMOLED init failed!");
    return;
  }
  s_display = &display;
  s_config.displayWidth = display.getWidth();
  s_config.displayHeight = display.getHeight();
#elif IS_TRGB
  static TRGBDisplay display;
  if (!display.begin())
  {
    Serial.println("T-RGB init failed!");
    return;
  }
  s_display = &display;
  s_config.displayWidth = display.getWidth();
  s_config.displayHeight = display.getHeight();
#else
  Serial.println("No display configured!");
  return;
#endif

  Serial.printf("Display (%dx%d) configured successfully.\n", s_config.displayWidth, s_config.displayHeight);
}

/**
 * @brief Initialize and configure the PCF85063 RTC.
 *
 * Enables ADC measurement, configures charging thresholds, voltage
 * limits, and current limits for safe Li-Po operation.
 */
void setupPCF85063()
{
  if (PCF85063->begin())
  {
    Serial.println("PCF85063 initialized successfully.");
  }
  else
  {
    Serial.println("ERROR: PCF85063 failed to initialize!");
  }

  // Enable RTC.
  PCF85063->IIC_Write_Device_State(PCF85063->Arduino_IIC_RTC::Device::RTC_CLOCK_RTC,
                                   PCF85063->Arduino_IIC_RTC::Device_State::RTC_DEVICE_ON);
  // Disable RTC.
  PCF85063->IIC_Write_Device_State(PCF85063->Arduino_IIC_RTC::Device::RTC_CLOCK_RTC,
                                   PCF85063->Arduino_IIC_RTC::Device_State::RTC_DEVICE_OFF);

#if defined(PCF85063_DIAGNOSTIC_ENABLED)
  Serial.printf("\n--------------------PCF85063--------------------\n");
  Serial.printf("IIC_Bus.use_count(): %" PRId32 "\n", (int32_t)IIC_Bus.use_count());
  Serial.printf("ID: 0x%" PRIx32 "\n", (int32_t)PCF85063->IIC_Device_ID());

  Serial.printf("\nPCF85063  Weekday: %s\n",
                PCF85063->IIC_Read_Device_State(PCF85063->Arduino_IIC_RTC::Status_Information::RTC_WEEKDAYS_DATA).c_str());
  Serial.printf("PCF85063  Year: %" PRId32 "\n",
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_YEARS_DATA));
  Serial.printf("PCF85063 Date: %" PRId32 ".%" PRId32 "\n",
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_MONTHS_DATA),
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_DAYS_DATA));
  Serial.printf("PCF85063 Time: %" PRId32 ":%" PRId32 ":%" PRId32 "\n",
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_HOURS_DATA),
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_MINUTES_DATA),
                (int32_t)PCF85063->IIC_Read_Device_Value(PCF85063->Arduino_IIC_RTC::Value_Information::RTC_SECONDS_DATA));
  Serial.printf("--------------------PCF85063--------------------\n\n");
#endif
}

/**
 * @brief Initialize and configure the SY6970 battery fuel gauge.
 *
 * Enables ADC measurement, configures charging thresholds, voltage
 * limits, and current limits for safe Li-Po operation.
 */
void setupSY6970()
{
  if (SY6970->begin())
  {
    Serial.println("SY6970 initialized successfully.");
  }
  else
  {
    Serial.println("ERROR: SY6970 failed to initialize!");
  }

  // Enable ADC measurement function.
  SY6970->IIC_Write_Device_State(SY6970->Arduino_IIC_Power::Device::POWER_DEVICE_ADC_MEASURE, SY6970->Arduino_IIC_Power::Device_State::POWER_DEVICE_ON);
  // Disable watchdog timer feeding function.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_WATCHDOG_TIMER, 0);
  // Set thermal regulation threshold to 60 degrees.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_THERMAL_REGULATION_THRESHOLD, 60);
  // Set charging target voltage to 4224mV.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_CHARGING_TARGET_VOLTAGE_LIMIT, 4224);
  // Set minimum system voltage limit to 3600mV.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_MINIMUM_SYSTEM_VOLTAGE_LIMIT, 3600);
  // Set OTG voltage to 5062mV.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_OTG_VOLTAGE_LIMIT, 5062);
  // Set input current limit to 2100mA.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_INPUT_CURRENT_LIMIT, 2100);
  // Set fast charging current limit to 2112mA.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_FAST_CHARGING_CURRENT_LIMIT, 2112);
  // Set pre-charge current limit to 192mA.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_PRECHARGE_CHARGING_CURRENT_LIMIT, 192);
  // Set termination charging current limit to 320mA.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_TERMINATION_CHARGING_CURRENT_LIMIT, 320);
  // Set OTG current limit to 500mA.
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_OTG_CHARGING_LIMIT, 500);

#if defined(SY6970_DIAGNOSTIC_ENABLED)
  Serial.printf("\n--------------------SY6970--------------------\n");
  Serial.printf("IIC_Bus.use_count(): %" PRId32 "\n", (int32_t)IIC_Bus.use_count());
  Serial.printf("IIC device ID: 0x%" PRIx32 "\n", (int32_t)SY6970->IIC_Device_ID());

  Serial.printf("\nBUS Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_BUS_STATUS)).c_str());
  Serial.printf("BUS Connection Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_BUS_CONNECTION_STATUS)).c_str());
  Serial.printf("Charging Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_CHARGING_STATUS)).c_str());
  Serial.printf("Input Source Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_INPUT_SOURCE_STATUS)).c_str());
  Serial.printf("Input USB Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_INPUT_USB_STATUS)).c_str());
  Serial.printf("System Voltage Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_SYSTEM_VOLTAGE_STATUS)).c_str());
  Serial.printf("Thermal Regulation Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_THERMAL_REGULATION_STATUS)).c_str());

  Serial.printf("\nWatchdog Fault Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_WATCHDOG_FAULT_STATUS)).c_str());
  Serial.printf("OTG Fault Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_OTG_FAULT_STATUS)).c_str());
  Serial.printf("Charging Fault Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_CHARGING_FAULT_STATUS)).c_str());
  Serial.printf("Battery Fault Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_BATTERY_FAULT_STATUS)).c_str());
  Serial.printf("NTC Fault Status: %s\n",
                (SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_NTC_FAULT_STATUS)).c_str());

  Serial.printf("\nInput Voltage: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_INPUT_VOLTAGE));
  Serial.printf("Battery Voltage: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_BATTERY_VOLTAGE));
  Serial.printf("System Voltage: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_SYSTEM_VOLTAGE));
  Serial.printf("NTC Voltage Percentage: %.03f %%\n",
                (float)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_NTC_VOLTAGE_PERCENTAGE) / 1000.0);
  Serial.printf("Charging Current: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_CHARGING_CURRENT));
  Serial.printf("Thermal Regulation Threshold: %" PRId32 " °C\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_THERMAL_REGULATION_THRESHOLD));

  Serial.printf("\nCharging Voltage Limit: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_CHARGING_TARGET_VOLTAGE_LIMIT));
  Serial.printf("Minimum System Voltage Limit: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_MINIMUM_SYSTEM_VOLTAGE_LIMIT));
  Serial.printf("OTG Voltage Limit: %" PRId32 " mV\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_OTG_VOLTAGE_LIMIT));
  Serial.printf("Input Current Limit: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_INPUT_CURRENT_LIMIT));
  Serial.printf("Fast Charge Current Limit: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_FAST_CHARGING_CURRENT_LIMIT));
  Serial.printf("Precharge Charge Current Limit: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_PRECHARGE_CHARGING_CURRENT_LIMIT));
  Serial.printf("Termination Charge Current Limit: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_TERMINATION_CHARGING_CURRENT_LIMIT));
  Serial.printf("OTG Current Limit: %" PRId32 " mA\n",
                (int32_t)SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_OTG_CURRENT_LIMIT));
  Serial.printf("--------------------SY6970--------------------\n\n");
#endif

  Serial.println("SY6970 configuration complete.");
}

/**
 * @brief Initialize WiiChuck, LightSensor, and DFRobot Gesture & Face Detection sensor input devices.
 *
 * Attempts to initialize a Wii Nunchuck and DFRobot Gesture
 * & Face Detection sensor on the I2C bus and a photoresistor
 * on LIGHT_PIN. Both are optional (sensor works without a
 * controller, and vice versa).
 */
void setupInput()
{
#if defined(QWIIC_SDA) && defined(QWIIC_SCL)
  static WiiChuckInput chuck(0x52, Wire1);
  static GestureFaceInput gfd(0x72, 320, 240, &Wire1);
#else
  static WiiChuckInput chuck;
  static GestureFaceInput gfd;
#endif

  static LightSensor lightSensor(LIGHT_PIN);
  if (lightSensor.begin())
  {
    s_lightSensor = &lightSensor;
  }

  if (chuck.begin())
  {
    s_wiiChuck = &chuck;
  }

  if (gfd.begin())
  {
    s_gestureFace = &gfd;
  }
}

/**
 * @brief Configure the EyeAnimator with the detected light sensor.
 *
 * Reads the sensor's calibrated min/max and curve values and passes
 * them to EyeAnimator. When no sensor is connected, passes -1 to
 * enable autonomous iris animation instead.
 */
void setupLightSensor()
{
  if (s_lightSensor && s_lightSensor->isConnected())
  {
    s_animator->setLightSensor(
        s_lightSensor->getPin(),
        s_lightSensor->getMinValue(),
        s_lightSensor->getMaxValue(),
        s_lightSensor->getCurve());
    Serial.println("Light sensor configured.");
  }
  else
  {
    s_animator->setLightSensor(-1, 0, 1023, 1.0f);
    Serial.println("Light sensor not connected - using autonomous iris animation.");
  }
}

/**
 * @brief Initialize ESP-NOW networking.
 *
 * Sets WiFi to STA mode, initializes the EyeSyncManager, and registers
 * callbacks for peer events and incoming state data. Sends the local
 * MAC address to the serial console for pairing.
 */
void setupNetwork()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  static EyeSyncManager sync;
  if (sync.begin(1))
  {
    s_syncManager = &sync;

    sync.onDataReceived([](const EyeSyncMessage &msg, const uint8_t *mac)
                        { sync.setControllerMac(mac); });

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    Serial.printf("ESP-NOW initialized (MAC: %02X:%02X:%02X:%02X:%02X:%02X).\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  else
  {
    Serial.println("ERROR: ESP-NOW init failed!");
  }

  static EyeInterpolator interp;
  s_interpolator = &interp;
}

/**
 * @brief Firmware initialization: peripherals, display, inputs, and animation.
 *
 * Initializes Serial, SY6970, display, debug overlay, WiiChuck, light sensor,
 * gesture/face sensor, network, and the EyeAnimator. Creates the renderLoopTask
 * on Core 1 at priority 1. This function does not return until the render task
 * takes over.
 */
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("===========================================");
  Serial.println("Uncanny Eyes for ESP32");
  Serial.print("Board: ");
#if ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED
  Serial.println("T-Display S3 AMOLED");
#elif ARDUINO_LILYGO_T_RGB
  Serial.println("T-RGB");
#else
  Serial.println("Unknown");
#endif
  Serial.println("===========================================");

  // Use 100 kHz as a safe middle ground that works for all devices
  // on the shared I2C bus.
  if (Wire.begin(IIC_SDA, IIC_SCL, 100000))
  {
    Serial.println("Wire (I2C) initialized successfully.");
    debug_I2Cscan(&Wire, "Wire");
  }
  else
  {
    Serial.println("ERROR: Wire (I2C) failed to initialize!");
  }

#if defined(QWIIC_SDA) && defined(QWIIC_SCL)
  if (Wire1.begin(QWIIC_SDA, QWIIC_SCL, 100000))
  {
    Serial.println("Wire1 (QWIIC) initialized successfully.");
    debug_I2Cscan(&Wire1, "Wire1");
  }
  else
  {
    Serial.println("ERROR: Wire1 (QWIIC) failed to initialize!");
  }
#endif

  setupPCF85063();

  setupSY6970();

  setupInput();

  setupDisplay();

  setupNetwork();

  s_animator = new EyeAnimator();
  if (!s_animator->begin(s_display, CURRENT_EYE))
  {
    Serial.println("EyeAnimator init failed!");
    return;
  }

  setupLightSensor();

  if (s_wiiChuck)
  {
    s_animator->setInput(s_wiiChuck);
  }
  if (s_gestureFace)
  {
    s_animator->setFaceInput(s_gestureFace);
  }
  if (s_syncManager)
  {
    s_animator->setSyncManager(s_syncManager);
  }

  s_animator->setPupilRange(0.45f, 0.8f);

#ifdef DEBUG_OVERLAY_ENABLED
  s_debugOverlay.begin(s_display);
  s_debugOverlay.setEnabled(true);
  s_debugOverlay.setBatteryPin(4, 0, 4095);
#endif

  Serial.println("\nInitialization complete!\n");

  xTaskCreatePinnedToCore(renderLoopTask, "EyeTask", 8192, NULL, 1, NULL, 1);
}

/**
 * @brief Eye rendering task running on Core 1 at ~120 FPS.
 *
 * Updates the animation state machine, polls the light sensor, broadcasts
 * to ESP-NOW peers, and triggers rendering when needed. Handles both
 * continuous animation and dirty-region-based updates via EyeRenderer.
 */
void renderLoopTask(void *param)
{
  (void)param;

  uint32_t lastFrame = 0;
  const uint32_t frameInterval = 8333; // ~120 FPS for smoother animation

#ifdef DEBUG_FPS_ENABLED
  s_frameCount = 0;
  s_fpsTimer = millis();
  s_currentFps = 0;
#endif

  while (true)
  {
    uint32_t now = micros();

    if (now - lastFrame >= frameInterval)
    {
      lastFrame = now;

#ifdef DEBUG_FPS_ENABLED
      s_frameCount++;
      if (millis() - s_fpsTimer >= 1000)
      {
        s_currentFps = s_frameCount;
        s_frameCount = 0;
        s_fpsTimer = millis();
        Serial.printf("[FPS] %" PRIu32 "\n", s_currentFps);
      }
#endif

#ifdef DEBUG_OVERLAY_ENABLED
      s_debugOverlay.update();
#endif

      s_animator->update(millis());

      if (s_lightSensor)
      {
        s_lightSensor->update();
      }

      s_animator->broadcastState();

      if (s_animator->needsRender())
      {
        float eyeX = s_animator->getEyeX();
        float eyeY = s_animator->getEyeY();
        float pupilFactor = s_animator->getPupilFactor();
        float blinkFactor = s_animator->getBlinkFactor();

        EyeRenderer *renderer = s_animator->getRenderer();
        float upperLidFactor = renderer->getUpperLidFactor();
        float lowerLidFactor = renderer->getLowerLidFactor();

        renderer->renderFrame(
            eyeX, eyeY,
            pupilFactor,
            upperLidFactor, lowerLidFactor,
            blinkFactor,
            0, 0);
      }
    }

    vTaskDelay(1);
  }
}

/**
 * @brief Switch the active eye definition by index.
 *
 * Registered as a command handler for the 'E' serial command.
 */
void switchEye(int index)
{
  if (s_animator && s_animator->setEyeIndex(index))
  {
    Serial.printf("Switched to eye: %s\n", getEyeName(index));
  }
}

/**
 * @brief Status report loop running on Core 0 (Arduino loop).
 *
 * Prints a running status line every 5 seconds and handles serial
 * commands ('En' to switch eyes). Kept lightweight to avoid
 * interfering with the render task.
 */
void loop()
{
  static uint32_t lastStatus = 0;
  uint32_t now = millis();

  if (now - lastStatus > 5000)
  {
    lastStatus = now;

    Serial.printf("[%" PRIu32 "] Running...", now / 1000);
    if (s_wiiChuck)
      Serial.print(" WiiChuck");
    if (s_gestureFace)
      Serial.print(" GestureFace");
    if (s_syncManager)
      Serial.printf(" ESP-NOW(%d peers)", s_syncManager->getPeerCount());
    Serial.println();
  }

  delay(10);

  while (Serial.available())
  {
    char c = Serial.read();
    if (c == 'E')
    {
      int eyeIndex = Serial.parseInt();
      if (eyeIndex >= 0 && eyeIndex < s_eyeCount)
      {
        switchEye(eyeIndex);
      }
      else
      {
        Serial.printf("Invalid eye index. Available: 0-%d\n", s_eyeCount - 1);
      }
    }
  }
}

void eyesBlink()
{
  if (s_animator)
    s_animator->eyesBlink();
}
void eyesBoop()
{
  if (s_animator)
    s_animator->eyesBoop();
}
void eyesClose()
{
  if (s_animator)
    s_animator->eyesClose();
}
void eyesWide()
{
  if (s_animator)
    s_animator->eyesWide();
}
void eyesNormal()
{
  if (s_animator)
    s_animator->eyesNormal();
}