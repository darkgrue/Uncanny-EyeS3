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
    m_movement.setRandomDuration(250, 500);  // 250-500ms - smoother, slower saccades
    m_movement.setSaccadeDelay(4000);  // 4 second delay before resuming idle movement
    m_movement.setRandomMode(true);
    
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
    // Time-based smooth pupil animation mimicking human pupillary unrest
    // - Saccade-like changes every 2-5 seconds
    // - Smooth 600-1000ms transitions between states
    // - Uses real display FPS timing, not wall clock
    uint32_t dt = now - m_lastIrisChange;
    
    // Check if it's time for a new target (saccade-like pupil change)
    if (dt >= m_irisHoldDuration) {
        // Generate new target with physiological bias
        // Pupil naturally varies around a baseline with small random fluctuations
        float u1 = (float)random(0, 1000) / 1000.0f;
        float u2 = (float)random(0, 1000) / 1000.0f;
        
        // Lognormal distribution - most movements are small
        float normalSample = sqrt(-2.0f * log(u1 + 0.0001f)) * cos(2.0f * PI * u2);
        float lognormalSample = normalSample * 0.3f - 0.1f;
        
        // Clamp to reasonable range (-0.3 to +0.3 offset from baseline)
        m_irisTarget = constrain(lognormalSample, -0.3f, 0.3f);
        
        // Vary hold duration for more natural timing (2-5 seconds)
        m_irisHoldDuration = 2000000 + random(0, 3000000);
        
        // Vary transition duration (600-1000ms for realistic saccade-like motion)
        m_irisTransitionDuration = 600000 + random(0, 400000);
        
        m_lastIrisChange = now;
    }
    
    // Smooth interpolation toward target using sigmoid easing
    float t = (float)dt / (float)m_irisTransitionDuration;
    t = constrain(t, 0.0f, 1.0f);
    
    // Smooth step easing (sigmoid-like)
    float eased = t * t * (3.0f - 2.0f * t);
    
    // Apply easing to interpolate from previous target to current target
    // This creates the smooth saccade-like transition
    m_irisSmooth = m_irisPrev[0] + (m_irisTarget - m_irisPrev[0]) * eased;
    
    // Update prev storage for next transition
    if (dt >= m_irisHoldDuration) {
        m_irisPrev[0] = m_irisTarget;
    }
    
    // Calculate final iris value
    // Normal sum centered at 0.5, with smooth random offset
    float sum = 0.5f + m_irisSmooth;
    sum = constrain(sum, 0.3f, 0.7f);  // Clamp to valid pupil range
    
    m_currentIris = m_irisMin + (sum * m_irisRange);
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
