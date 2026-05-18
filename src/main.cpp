#include <Arduino.h>
#include "common/EyeState.h"
#include "common/EyeConfig.h"
#include "common/DisplayGeometry.h"
#include "display/DisplayHAL.h"
#include "display/TDisplayS3AMOLED.h"
#include "display/TRGBDisplay.h"
#include "eye/EyeAnimator.h"
#include "input/WiiChuck.h"
#include "network/EyeSync.h"

// Configuration
static EyeProjectConfig s_config;
static EyeAnimator* s_animator = nullptr;
static DisplayHAL* s_display = nullptr;
static WiiChuckInput* s_wiiChuck = nullptr;
static EyeSyncManager* s_syncManager = nullptr;
static EyeInterpolator* s_interpolator = nullptr;

// Detect which board we are running on
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

void renderLoopTask(void* param);

void setupDisplay() {
    #if IS_AMOLED
        static TDisplayS3AMOLED display;
        if (!display.begin()) {
            Serial.println("T-Display S3 AMOLED init failed!");
            return;
        }
        s_display = &display;
        s_config.displayWidth = 466;
        s_config.displayHeight = 466;
    #elif IS_TRGB
        static TRGBDisplay display;
        if (!display.begin()) {
            Serial.println("T-RGB init failed!");
            return;
        }
        s_display = &display;
        s_config.displayWidth = 480;
        s_config.displayHeight = 480;
    #else
        Serial.println("No display configured!");
        return;
    #endif

    Serial.printf("Display: %dx%d\n", s_config.displayWidth, s_config.displayHeight);
}

void setupInput() {
    // Initialize WiiChuck on I2C
    static WiiChuckInput chuck;
    if (chuck.begin()) {
        s_wiiChuck = &chuck;
        Serial.println("WiiChuck initialized");
    } else {
        Serial.println("WiiChuck not found (this is normal if not connected)");
    }
}

void setupNetwork() {
    // Initialize WiFi in station mode for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    static EyeSyncManager sync;
    if (sync.begin(1)) {  // Channel 1
        s_syncManager = &sync;

        // Set up callback for received data
        sync.onDataReceived([](const EyeSyncMessage& msg, const uint8_t* mac) {
            Serial.printf("Received from %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            // Mark sender as controller
            sync.setControllerMac(mac);
        });

        Serial.println("ESP-NOW initialized");
        Serial.print("MAC: ");
        Serial.println(WiFi.macAddress());
    } else {
        Serial.println("ESP-NOW init failed!");
    }

    static EyeInterpolator interp;
    s_interpolator = &interp;
}

void setup() {
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

    // Initialize display
    setupDisplay();

    // Initialize input (WiiChuck)
    setupInput();

    // Initialize network (ESP-NOW)
    setupNetwork();

    // Initialize animator
    s_animator = new EyeAnimator();
    if (!s_animator->begin(s_display)) {
        Serial.println("EyeAnimator init failed!");
        return;
    }

    // Connect input and network to animator
    if (s_wiiChuck) {
        s_animator->setInput(s_wiiChuck);
    }
    if (s_syncManager) {
        s_animator->setSyncManager(s_syncManager);
    }

    // Configure light sensor (using ambient light from display)
    // TODO: Add external light sensor support
    s_animator->setLightSensor(-1, 0, 1023, 1.0f);
    s_animator->setPupilRange(0.45f, 0.8f);

    Serial.println("Initialization complete!");
    Serial.println();

// Start render task on second core
    xTaskCreatePinnedToCore(renderLoopTask, "EyeTask", 8192, NULL, 1, NULL, 1);
}

void renderLoopTask(void* param) {
    (void)param;

    uint32_t lastFrame = 0;
    const uint32_t frameInterval = 16667;  // ~60 FPS

    while (true) {
        uint32_t now = micros();

        // Rate-limit rendering
        if (now - lastFrame >= frameInterval) {
            lastFrame = now;

            // Update animator
            s_animator->update(millis());

            // Broadcast our state to other eyes (if controller or has peers)
            s_animator->broadcastState();

            // Render if needed
            if (s_animator->needsRender()) {
                // Get current eye state
                float eyeX = s_animator->getEyeX();
                float eyeY = s_animator->getEyeY();
                float pupilFactor = s_animator->getPupilFactor();

                // Render the eye (full frame)
                s_animator->getRenderer()->renderFrame(
                    eyeX, eyeY,
                    pupilFactor,
                    1.0f, 1.0f,  // eyelid factors
                    0.0f,  // blinkFactor
                    0, 0    // iris/sclera angles
                );
            }
        }

        // Yield to allow other tasks
        vTaskDelay(1);
    }
}

void loop() {
    // Main loop handles:
    // 1. USB serial (debug)
    // 2. Network keepalive
    // 3. Watchdog feeding

    static uint32_t lastStatus = 0;
    uint32_t now = millis();

    if (now - lastStatus > 5000) {
        lastStatus = now;

        // Print status every 5 seconds
        Serial.printf("[%lu] Running...", now / 1000);
        if (s_wiiChuck) Serial.print(" WiiChuck");
        if (s_syncManager) Serial.printf(" ESP-NOW(%d peers)", s_syncManager->getPeerCount());
        Serial.println();
    }

    // Feed watchdog
    delay(10);
}

// User-callable functions (for external control)
void eyesBlink() { if (s_animator) s_animator->eyesBlink(); }
void eyesBoop() { if (s_animator) s_animator->eyesBoop(); }
void eyesClose() { if (s_animator) s_animator->eyesClose(); }
void eyesWide() { if (s_animator) s_animator->eyesWide(); }
void eyesNormal() { if (s_animator) s_animator->eyesNormal(); }