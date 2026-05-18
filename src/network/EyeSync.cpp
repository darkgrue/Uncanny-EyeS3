#include "EyeSync.h"
#include <Arduino.h>

EyeSyncManager* EyeSyncManager::s_instance = nullptr;

EyeSyncManager::EyeSyncManager() {
    memset(m_controllerMac, 0, 6);
    memset(m_peerMACS, 0, sizeof(m_peerMACS));
    m_peerCount = 0;
}

bool EyeSyncManager::begin(uint8_t channel) {
    if (m_initialized) return true;

    s_instance = this;  // Store instance for static callbacks

    // WiFi should already be initialized in station mode
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);

    m_initialized = true;
    m_channel = channel;
    return true;
}

void EyeSyncManager::broadcast(const EyeSyncMessage& msg) {
    if (!m_initialized) return;
    
    // Get peer count
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    
    for (int i = 0; i < m_peerCount; i++) {
        memcpy(peerInfo.peer_addr, m_peerMACS[i], 6);
        peerInfo.channel = m_channel;
        peerInfo.encrypt = false;
        
        if (esp_now_send(peerInfo.peer_addr, (uint8_t*)&msg, sizeof(msg)) == ESP_OK) {
            Serial.printf("Sent to %02X:%02X:%02X:%02X:%02X:%02X\n",
                m_peerMACS[i][0], m_peerMACS[i][1], m_peerMACS[i][2],
                m_peerMACS[i][3], m_peerMACS[i][4], m_peerMACS[i][5]);
        }
    }
}

bool EyeSyncManager::sendTo(const uint8_t* mac, const EyeSyncMessage& msg) {
    if (!m_initialized) return false;
    
    return esp_now_send(mac, (uint8_t*)&msg, sizeof(msg)) == ESP_OK;
}

void EyeSyncManager::onDataSent(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        // Serial.println("Send success");
    } else {
        // Serial.println("Send failed");
    }
}

void EyeSyncManager::onDataReceived(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    if (!s_instance || data_len != sizeof(EyeSyncMessage)) {
        Serial.println("Invalid message size");
        return;
    }

    const uint8_t* mac = esp_now_info->src_addr;
    EyeSyncMessage msg;
    memcpy(&msg, data, sizeof(msg));
    memcpy(msg.macAddress, mac, 6);

    s_instance->m_lastRemoteState = msg;
    s_instance->m_lastRemoteTime = millis();

    if (s_instance->m_onDataReceived) {
        s_instance->m_onDataReceived(msg, mac);
    }
}

int EyeSyncManager::getPeerCount() const {
    return m_peerCount;
}

const uint8_t* EyeSyncManager::getPeerMac(int index) const {
    if (index < 0 || index >= m_peerCount) return nullptr;
    return m_peerMACS[index];
}

void EyeSyncManager::setControllerMac(const uint8_t* mac) {
    m_hasController = true;
    memcpy(m_controllerMac, mac, 6);
}

// EyeInterpolator implementation
EyeInterpolator::EyeInterpolator() {
    memset(&m_target, 0, sizeof(m_target));
    memset(&m_prev, 0, sizeof(m_prev));
    m_targetTime = 0;
    m_prevTime = 0;
}

void EyeInterpolator::updateTarget(const EyeSyncMessage& remote, uint32_t now) {
    m_prev = m_target;
    m_prevTime = m_targetTime;
    m_target = remote;
    m_targetTime = now;
}

float EyeInterpolator::getX(uint32_t now) const {
    if (m_targetTime == 0) return 0.5f;
    
    float t = (float)(now - m_targetTime) / 1000.0f;  // ms
    float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;
    
    if (t >= duration || duration <= 0) {
        return m_target.eyeX;
    }
    
    // Linear interpolation with easing
    float e = t / duration;
    e = 3.0f * e * e - 2.0f * e * e * e;  // Smoothstep
    
    return m_prev.eyeX + (m_target.eyeX - m_prev.eyeX) * e;
}

float EyeInterpolator::getY(uint32_t now) const {
    if (m_targetTime == 0) return 0.5f;
    
    float t = (float)(now - m_targetTime) / 1000.0f;
    float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;
    
    if (t >= duration || duration <= 0) {
        return m_target.eyeY;
    }
    
    float e = t / duration;
    e = 3.0f * e * e - 2.0f * e * e * e;
    
    return m_prev.eyeY + (m_target.eyeY - m_prev.eyeY) * e;
}

float EyeInterpolator::getPupil(uint32_t now) const {
    if (m_targetTime == 0) return 0.5f;
    
    float t = (float)(now - m_targetTime) / 1000.0f;
    float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;
    
    if (t >= duration || duration <= 0) {
        return m_target.pupilFactor;
    }
    
    return m_prev.pupilFactor + (m_target.pupilFactor - m_prev.pupilFactor) * (t / duration);
}

bool EyeInterpolator::isStale(uint32_t now, uint32_t maxAge) const {
    return (now - m_targetTime) > maxAge;
}
