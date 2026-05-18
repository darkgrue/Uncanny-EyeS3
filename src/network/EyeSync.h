#ifndef EYE_SYNC_H
#define EYE_SYNC_H

#include "EyeState.h"
#include <esp_now.h>
#include <WiFi.h>

// Callback types for network events
typedef void (*OnPeerJoined)(const uint8_t* mac);
typedef void (*OnPeerLeft)(const uint8_t* mac);
typedef void (*OnDataReceived)(const EyeSyncMessage& msg, const uint8_t* mac);

// ESP-NOW synchronization manager
class EyeSyncManager {
public:
    EyeSyncManager();
    
    // Initialize ESP-NOW (must call after WiFi mode is set)
    bool begin(uint8_t channel = 1);
    
    // Register callbacks
    void onPeerJoined(OnPeerJoined callback) { m_onPeerJoined = callback; }
    void onPeerLeft(OnPeerLeft callback) { m_onPeerLeft = callback; }
    void onDataReceived(OnDataReceived callback) { m_onDataReceived = callback; }
    
    // Broadcast eye state to all peers
    void broadcast(const EyeSyncMessage& msg);
    
    // Send to specific peer
    bool sendTo(const uint8_t* mac, const EyeSyncMessage& msg);
    
    // Get list of connected peers
    int getPeerCount() const;
    const uint8_t* getPeerMac(int index) const;
    
    // Check if we have a master (controller) eye
    bool hasController() const { return m_hasController; }
    void setControllerMac(const uint8_t* mac);
    
    // Get last received state (for interpolation)
    EyeSyncMessage getLastRemoteState() const { return m_lastRemoteState; }
    uint32_t getLastRemoteTime() const { return m_lastRemoteTime; }

private:
    static EyeSyncManager* s_instance;
    static void onDataSent(const esp_now_send_info_t* tx_info, esp_now_send_status_t status);
    static void onDataReceived(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len);

    bool m_initialized = false;
    bool m_hasController = false;
    uint8_t m_controllerMac[6];
    uint8_t m_channel = 1;
    EyeSyncMessage m_lastRemoteState;
    uint32_t m_lastRemoteTime = 0;
    
    OnPeerJoined m_onPeerJoined = nullptr;
    OnPeerLeft m_onPeerLeft = nullptr;
    OnDataReceived m_onDataReceived = nullptr;
    
    uint8_t m_peerMACS[8][6];
    int m_peerCount = 0;
};

// Interpolation helper for smooth remote eye movement
class EyeInterpolator {
public:
    EyeInterpolator();
    
    // Update with new target from network
    void updateTarget(const EyeSyncMessage& remote, uint32_t now);
    
    // Get interpolated values for local rendering
    float getX(uint32_t now) const;
    float getY(uint32_t now) const;
    float getPupil(uint32_t now) const;
    
    // Check if remote data is stale
    bool isStale(uint32_t now, uint32_t maxAge = 100000) const;

private:
    EyeSyncMessage m_target;
    uint32_t m_targetTime = 0;
    
    EyeSyncMessage m_prev;
    uint32_t m_prevTime = 0;
    
    float m_currentX = 0.5f;
    float m_currentY = 0.5f;
    float m_currentPupil = 0.5f;
};

#endif // EYE_SYNC_H
