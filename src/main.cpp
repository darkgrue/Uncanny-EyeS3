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
#include "network/EyeSync.h"
#include "debug/DebugOverlay.h"
#include "eyes.h"
#include "EyeLibrary.h"
#include "pin_config.h"
#include <Wire.h>

#define CURRENT_EYE default_eye::eye

static EyeProjectConfig s_config;
static EyeAnimator *s_animator = nullptr;
static DisplayHAL *s_display = nullptr;
static WiiChuckInput *s_wiiChuck = nullptr;
static EyeSyncManager *s_syncManager = nullptr;
static EyeInterpolator *s_interpolator = nullptr;
static LightSensor *s_lightSensor = nullptr;

#ifdef DEBUG_OVERLAY_ENABLED
static DebugOverlay s_debugOverlay;
#endif

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

std::unique_ptr<Arduino_IIC> SY6970(new Arduino_SY6970(IIC_Bus, SY6970_DEVICE_ADDRESS,
                                                       DRIVEBUS_DEFAULT_VALUE, DRIVEBUS_DEFAULT_VALUE));

std::unique_ptr<Arduino_IIC> PCF8563(new Arduino_PCF8563(IIC_Bus, PCF8563_DEVICE_ADDRESS,
                                                         DRIVEBUS_DEFAULT_VALUE, DRIVEBUS_DEFAULT_VALUE));

#if defined(ARDUINO_LILYGO_T_DISPLAY_S3_AMOLED)
#define IS_AMOLED 1
#define IS_TRGB 0
#elif defined(ARDUINO_LILYGO_T_RGB)
#define IS_AMOLED 0
#define IS_TRGB 1
#else
#define IS_AMOLED 0
#define IS_TRGB 0
#endif

void renderLoopTask(void *param);

void switchEye(int index)
{
  if (s_animator && s_animator->setEyeIndex(index))
  {
    Serial.printf("Switched to eye: %s\n", getEyeName(index));
  }
}

void setupSY6970()
{
  if (SY6970->begin() == false)
  {
    Serial.println("SY6970 initialization fail!");
  }
  else
  {
    Serial.println("SY6970 initialization successful.");
  }

  // Enable ADC measurement function.
  if (SY6970->IIC_Write_Device_State(SY6970->Arduino_IIC_Power::Device::POWER_DEVICE_ADC_MEASURE, SY6970->Arduino_IIC_Power::Device_State::POWER_DEVICE_ON) == false)
  {
    Serial.println("Failure to set SY6970 ADC Measure ON!");
  }
  else
  {
    Serial.println("Set SY6970 ADC Measure ON successfully.");
  }

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

  Serial.println("SY6970 configuration complete.");
}

void setupDisplay()
{
#if IS_AMOLED
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

  Serial.printf("Display: %dx%d\n", s_config.displayWidth, s_config.displayHeight);
}

void setupInput()
{
  static WiiChuckInput chuck;
  if (chuck.begin())
  {
    s_wiiChuck = &chuck;
    Serial.println("WiiChuck initialized.");
  }
  else
  {
    Serial.println("WiiChuck not found (this is normal if not connected).");
  }

  static LightSensor lightSensor(LIGHT_PIN);
  if (lightSensor.begin())
  {
    s_lightSensor = &lightSensor;
  }
}

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

void setupNetwork()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  static EyeSyncManager sync;
  if (sync.begin(1))
  {
    s_syncManager = &sync;

    sync.onDataReceived([](const EyeSyncMessage &msg, const uint8_t *mac)
                        {
            Serial.printf("Received from %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            sync.setControllerMac(mac); });

    Serial.println("ESP-NOW initialized.");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
  }
  else
  {
    Serial.println("ESP-NOW init failed!");
  }

  static EyeInterpolator interp;
  s_interpolator = &interp;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("===========================================");
  Serial.println("Uncanny Eyes for ESP32");
  Serial.print("Board: ");
#if IS_AMOLED
  Serial.println("T-Display S3 AMOLED");
#elif IS_TRGB
  Serial.println("T-RGB");
#else
  Serial.println("Unknown");
#endif
  Serial.println("===========================================");

  setupSY6970();

  setupDisplay();

#ifdef DEBUG_OVERLAY_ENABLED
  s_debugOverlay.begin(s_display);
  s_debugOverlay.setEnabled(true);
  s_debugOverlay.setBatteryPin(4, 0, 4095);
#endif

  setupInput();

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
  if (s_syncManager)
  {
    s_animator->setSyncManager(s_syncManager);
  }

  s_animator->setPupilRange(0.45f, 0.8f);

  Serial.println("\nInitialization complete!\n");

  xTaskCreatePinnedToCore(renderLoopTask, "EyeTask", 8192, NULL, 1, NULL, 1);
}

void renderLoopTask(void *param)
{
  (void)param;

  uint32_t lastFrame = 0;
  const uint32_t frameInterval = 8333;  // ~120 FPS for smoother animation

  // FPS measurement
  uint32_t frameCount = 0;
  uint32_t fpsTimer = millis();
  uint32_t currentFps = 0;

  while (true)
  {
    uint32_t now = micros();

    if (now - lastFrame >= frameInterval)
    {
      lastFrame = now;

      // Measure actual FPS
      frameCount++;
      if (millis() - fpsTimer >= 1000) {
        currentFps = frameCount;
        frameCount = 0;
        fpsTimer = millis();
        Serial.printf("[FPS] %u\n", currentFps);
      }

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

        EyeRenderer* renderer = s_animator->getRenderer();
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