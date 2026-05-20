#include "EyeAnimator.h"
#include "EyeLibrary.h"
#include <Arduino.h>
#include <WiFi.h>

EyeAnimator::EyeAnimator() 
    : m_display(nullptr)
    , m_input(nullptr)
    , m_sync(nullptr)
    , m_eyeDef(nullptr)
    , m_lightSensorPin(-1)
    , m_lastLightRead(0)
    , m_booped(false)
    , m_needsRender(true)
    , m_initialized(false) {
}

bool EyeAnimator::begin(DisplayHAL* display, const EyeDefinition& eyeDef) {
    m_display = display;
    m_eyeDef = &eyeDef;
    
    if (!m_renderer.begin(display, eyeDef)) {
        return false;
    }
    
// Initialize movement system
    m_movement.setBounds(0.6f);
    m_movement.setRandomDuration(83, 166);
    m_movement.setSaccadeDelay(2000);  // 2 second delay before resuming idle movement after losing target
    m_movement.startRandomMove();
    
    // Initialize autonomous iris
    for (int i = 0; i < IRIS_LEVELS; i++) {
        m_irisPrev[i] = 0;
        m_irisNext[i] = -0.5f + random(0, 1000) / 1000.0f;
    }
    m_irisFrame = 0;
    m_currentIris = 0.5f;
    
    m_initialized = true;
    return true;
}

void EyeAnimator::setLightSensor(int pin, uint16_t minVal, uint16_t maxVal, float curve) {
    m_lightSensorPin = pin;
    m_lightMin = minVal;
    m_lightMax = maxVal;
    m_lightCurve = curve;
    if (m_lightSensorPin >= 0) {
        pinMode(pin, INPUT);
    }
}

void EyeAnimator::setPupilRange(float minPupil, float maxPupil) {
    m_irisMin = minPupil;
    m_irisRange = maxPupil - minPupil;
}

bool EyeAnimator::setEyeIndex(int index) {
    if (index < 0 || index >= s_eyeCount) {
        return false;
    }

    m_eyeIndex = index;
    m_eyeDef = s_eyeRegistry[index];

    if (!m_renderer.begin(m_display, *m_eyeDef)) {
        return false;
    }

    m_needsRender = true;
    return true;
}

void EyeAnimator::update(uint32_t now) {
    if (!m_initialized) return;
    
    // Update input
    if (m_input) {
        m_input->update();
        
        // Get input position if available
        if (m_input->hasExclusiveControl()) {
            float inputX = m_input->getTargetX();
            float inputY = m_input->getTargetY();
            m_movement.setTarget(inputX, inputY);
            m_movement.setRandomMode(false);
            
            // Handle edge-triggered input buttons
            if (m_input->wantsBlink()) {
                eyesBlink();
                m_input->clearBlinkFlag();
            }
            if (m_input->wantsBoop()) {
                eyesBoop();
                m_input->clearBoopFlag();
            }
            
            // Handle hold commands (C = wide, Z = close)
            if (m_input->wantsWide()) {
                eyesWide();
            } else if (m_input->wantsClose()) {
                eyesClose();
            } else {
                eyesNormal();
            }
        } else if (!m_movement.isMoving() && m_movement.getTargetX() == 0 && m_movement.getTargetY() == 0) {
            // No input, resume random movement with saccade delay
            m_movement.setTargetLost();
            m_movement.setRandomMode(true);
        }
    }
    
    // Process network input
    processNetworkInput();
    
    // Update movement
    uint32_t dt = millis() - m_lastLightRead;  // Approximate
    m_movement.update(dt);
    
    // Update blink state machine
    m_blink.update(micros());
    
    // Update light sensor / autonomous iris
    if (m_lightSensorPin >= 0) {
        updateLightSensor(now);
    } else {
        updateIrisAutonomous(now);
    }
    
    // Mark for rendering
    m_needsRender = true;
}

bool EyeAnimator::broadcastState() {
    // If we have no network, nothing to do
    if (!m_sync) return false;
    
    // Only broadcast if we have a WiiChuck with active control
    // OR if we are the controller (someone is listening to us)
    // The key insight: controller status is determined by having active input
    bool shouldBroadcast = isController();
    
    // If we have a sync manager and peers exist, always broadcast
    // (followers will ignore if we are not their controller)
    if (m_sync && m_sync->getPeerCount() > 0) {
        shouldBroadcast = true;
    }
    
    if (!shouldBroadcast) return false;
    
    // Build sync message
    EyeSyncMessage msg;
    memset(&msg, 0, sizeof(msg));
    
    // Get our MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    memcpy(msg.macAddress, mac, 6);
    
    // Current eye position
    msg.eyeX = getEyeX();
    msg.eyeY = getEyeY();
    
    // Pupil dilation
    msg.pupilFactor = m_currentIris;
    
    // Blink state
    msg.blinkState = (uint8_t)m_blink.getState();
    
    // Timestamp
    msg.timestamp = millis();
    
    // Commands from input - priority order: close > wide > blink > boop
    msg.command = CMD_NONE;
    if (m_input && m_input->wantsClose()) {
        msg.command = CMD_CLOSE;
    } else if (m_input && m_input->wantsWide()) {
        msg.command = CMD_WIDE;
    } else if (m_input && m_input->wantsBlink()) {
        msg.command = CMD_BLINK;
    } else if (m_input && m_input->wantsBoop()) {
        msg.command = CMD_BOOP;
    }
    
    // Broadcast to all peers
    m_sync->broadcast(msg);
    
    return true;
}

void EyeAnimator::updateLightSensor(uint32_t now) {
    constexpr uint32_t LIGHT_INTERVAL = 100000;  // 10 Hz max polling
    
    if (now - m_lastLightRead < LIGHT_INTERVAL) return;
    
    uint16_t raw = analogRead(m_lightSensorPin);
    if (raw > 1023) raw = 1023;
    
    raw = constrain(raw, m_lightMin, m_lightMax);
    float normalized = (float)(raw - m_lightMin) / (float)(m_lightMax - m_lightMin);
    normalized = pow(normalized, m_lightCurve);
    
    m_currentIris = m_irisMin + normalized * m_irisRange;
    m_lastLightRead = now;
}

void EyeAnimator::updateIrisAutonomous(uint32_t now) {
    // Fractal subdivision iris animation
    // Uses multiple levels of subdivision for smooth random movement
    float sum = 0.5f;
    
    for (int i = 0; i < IRIS_LEVELS; i++) {
        uint16_t iexp = 1 << (i + 1);      // 2, 4, 8, 16, ...
        uint16_t imask = iexp - 1;         // 1, 3, 7, 15, ...
        uint16_t ibits = m_irisFrame & imask;
        
        if (ibits) {
            float weight = (float)ibits / (float)iexp;
            float n = m_irisPrev[i] * (1.0f - weight) + m_irisNext[i] * weight;
            sum += n / (float)(1 << (IRIS_LEVELS - i));
        } else {
            m_irisPrev[i] = m_irisNext[i];
            m_irisNext[i] = -0.5f + random(0, 1000) / 1000.0f;
        }
    }
    
    m_currentIris = m_irisMin + (sum * m_irisRange);
    
    if (++m_irisFrame >= (1 << IRIS_LEVELS)) {
        m_irisFrame = 0;
    }
}

void EyeAnimator::processNetworkInput() {
    if (!m_sync || !m_sync->hasController()) return;

    // Get interpolated values from network
    uint32_t now = millis();

    if (m_sync->getLastRemoteTime() > 0 && m_sync->getLastRemoteState().eyeX != 0) {
        float remoteX = m_sync->getLastRemoteState().eyeX;
        float remoteY = m_sync->getLastRemoteState().eyeY;

        // Only use network values if they are fresh
        if ((now - m_sync->getLastRemoteTime()) < 100) {
            m_movement.setTargetAcquired();
            m_movement.setTarget(remoteX, remoteY);
            m_movement.setRandomMode(false);

            // Handle commands
            EyeSyncMessage msg = m_sync->getLastRemoteState();
            switch (msg.command) {
                case CMD_BLINK: eyesBlink(); break;
                case CMD_BOOP: eyesBoop(); break;
                case CMD_CLOSE: eyesClose(); break;
                case CMD_WIDE: eyesWide(); break;
                case CMD_NORMAL: eyesNormal(); break;
            }
        }
    } else {
        // No fresh network data, start idle delay
        m_movement.setTargetLost();
    }
}
