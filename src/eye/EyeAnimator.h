#ifndef EYE_ANIMATOR_H
#define EYE_ANIMATOR_H

#include "common/EyeState.h"
#include "EyeConfig.h"
#include "EyeRenderer.h"
#include "animation/BlinkFSM.h"
#include "animation/EyeMovement.h"
#include "common/DisplayHAL.h"
#include "input/InputBase.h"
#include "network/EyeSync.h"
#include "eyes.h"

class EyeAnimator {
public:
    EyeAnimator();
    
    // Initialize with display and eye definition (uses precomputed tables)
    bool begin(DisplayHAL* display, const EyeDefinition& eyeDef);
    
    // Set input sources
    void setInput(InputBase* input) { m_input = input; }
    
    // Set network sync (can be nullptr if solo)
    void setSyncManager(EyeSyncManager* sync) { m_sync = sync; }
    
    // Configuration
    void setLightSensor(int pin, uint16_t minVal, uint16_t maxVal, float curve = 1.0f);
    void setPupilRange(float minPupil, float maxPupil);
    
    // Main update loop - call frequently
    void update(uint32_t now);
    
    // Broadcast current state to network peers
    // Returns true if broadcasting (controller) or false (follower)
    bool broadcastState();
    
    // Check if this eye is the controller (has input control)
    bool isController() const { return m_input && m_input->hasExclusiveControl(); }
    
    // Get current eye position for external use
    float getEyeX() const { return m_movement.getX(); }
    float getEyeY() const { return m_movement.getY(); }
    
    // Get pupil factor
    float getPupilFactor() const { return m_currentIris; }
    
    // Check if rendering is needed
    bool needsRender() const { return m_needsRender; }
    
    // Access renderer for custom drawing
    EyeRenderer* getRenderer() { return &m_renderer; }
    
    // User callable functions (M4_Eyes compatible)
    void eyesBlink() { m_blink.trigger(); }
    void eyesBoop() { m_booped = true; }
    void eyesClose() { m_blink.close(); }
    void eyesNormal() { m_blink.normal(); m_movement.setRandomMode(true); }
void eyesWide() { m_blink.wide(); }

    int getEyeIndex() const { return m_eyeIndex; }
    bool setEyeIndex(int index);

private:
    void updateLightSensor(uint32_t now);
    void updateIrisAutonomous(uint32_t now);
    void processNetworkInput();
    
    DisplayHAL* m_display = nullptr;
    InputBase* m_input = nullptr;
    EyeSyncManager* m_sync = nullptr;
    
    // Eye definition reference (must persist during rendering)
    const EyeDefinition* m_eyeDef = nullptr;
    int m_eyeIndex = 0;
    
    EyeRenderer m_renderer;
    EyeMovement m_movement;
    BlinkFSM m_blink;
    
    // Light sensor
    int m_lightSensorPin = -1;
    uint16_t m_lightMin = 0;
    uint16_t m_lightMax = 1023;
    float m_lightCurve = 1.0f;
    float m_irisMin = 0.45f;
    float m_irisRange = 0.35f;
    uint32_t m_lastLightRead = 0;
    float m_currentIris = 0.5f;
    
    // Autonomous iris animation (when no light sensor)
    static constexpr int IRIS_LEVELS = 7;
    float m_irisPrev[IRIS_LEVELS] = { 0 };
    float m_irisNext[IRIS_LEVELS] = { 0 };
    uint16_t m_irisFrame = 0;
    
    // Boop detection
    bool m_booped = false;
    
    // State flags
    bool m_needsRender = true;
    bool m_initialized = false;
};

#endif // EYE_ANIMATOR_H
