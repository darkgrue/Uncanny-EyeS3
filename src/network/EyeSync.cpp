/**
 * @file EyeSync.cpp
 * @brief Implementation of ESP-NOW peer synchronization and state interpolation.
 *
 * Initializes ESP-NOW, registers send/receive callbacks, and routes incoming
 * EyeSyncMessage packets to registered callbacks. The static singleton pattern
 * (s_instance) is used to bridge the C-style ESP-NOW callbacks back to the
 * object instance.
 *
 * EyeInterpolator performs smoothstep easing between received states using
 * timestamps so that locally rendered motion is fluid even when network
 * packets arrive irregularly.
 */
#include "EyeSync.h"
#include <Arduino.h>

EyeSyncManager *EyeSyncManager::s_instance = nullptr;

EyeSyncManager::EyeSyncManager()
{
  memset(m_controllerMac, 0, 6);
  memset(m_peerMACS, 0, sizeof(m_peerMACS));
  m_peerCount = 0;
}

/**
 * @brief Initialize ESP-NOW and register send/receive callbacks.
 *
 * WiFi must already be in station mode (set in setupNetwork()). Stores
 * the instance pointer in a static for use by the static callback handlers.
 * Supports both ESP-IDF v4.x and v5.x APIs via compile-time detection.
 */
bool EyeSyncManager::begin(uint8_t channel)
{
  if (m_initialized)
    return true;

  s_instance = this;

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return false;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);

  m_initialized = true;
  m_channel = channel;
  return true;
}

/**
 * @brief Broadcast an EyeSyncMessage to all registered peers.
 *
 * Iterates over all known peer MAC addresses and sends the same message
 * to each. Called by EyeAnimator::broadcastState() each frame when
 * the device is a controller or has active peers.
 */
void EyeSyncManager::broadcast(const EyeSyncMessage &msg)
{
  if (!m_initialized)
    return;

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));

  for (int i = 0; i < m_peerCount; i++)
  {
    memcpy(peerInfo.peer_addr, m_peerMACS[i], 6);
    peerInfo.channel = m_channel;
    peerInfo.encrypt = false;

    if (esp_now_send(peerInfo.peer_addr, (uint8_t *)&msg, sizeof(msg)) == ESP_OK)
    {
      Serial.printf("Sent to %02X:%02X:%02X:%02X:%02X:%02X\n",
                    m_peerMACS[i][0], m_peerMACS[i][1], m_peerMACS[i][2],
                    m_peerMACS[i][3], m_peerMACS[i][4], m_peerMACS[i][5]);
    }
  }
}

/** @brief Send a message to a specific peer by MAC address. */
bool EyeSyncManager::sendTo(const uint8_t *mac, const EyeSyncMessage &msg)
{
  if (!m_initialized)
    return false;
  return esp_now_send(mac, (uint8_t *)&msg, sizeof(msg)) == ESP_OK;
}

#if ESP_NOW_NEW_API
/** @brief Called when an ESP-NOW send completes (new API). */
void EyeSyncManager::onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
  (void)tx_info;
  (void)status;
}
#else
/** @brief Called when an ESP-NOW send completes (legacy API). */
void EyeSyncManager::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  (void)mac_addr;
  (void)status;
}
#endif

#if ESP_NOW_NEW_API
/**
 * @brief Called when an ESP-NOW packet is received (new API).
 *
 * Validates the message size, copies the payload into an EyeSyncMessage,
 * extracts the sender MAC from esp_now_info, stores it as the last remote
 * state, and fires the onDataReceived callback.
 */
void EyeSyncManager::onDataReceived(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)
{
  if (!s_instance || data_len != sizeof(EyeSyncMessage))
  {
    Serial.println("Invalid message size");
    return;
  }

  EyeSyncMessage msg;
  memcpy(&msg, data, sizeof(msg));
  if (esp_now_info && esp_now_info->src_addr)
  {
    memcpy(msg.macAddress, esp_now_info->src_addr, 6);
  }

  s_instance->m_lastRemoteState = msg;
  s_instance->m_lastRemoteTime = millis();

  if (s_instance->m_onDataReceived)
  {
    s_instance->m_onDataReceived(msg, msg.macAddress);
  }
}
#else
/**
 * @brief Called when an ESP-NOW packet is received (legacy API).
 *
 * Validates message size, copies payload and sender MAC into the
 * EyeSyncMessage, updates last-remote state tracking, and fires
 * the onDataReceived callback.
 */
void EyeSyncManager::onDataReceived(const uint8_t *mac, const uint8_t *data, int data_len)
{
  if (!s_instance || data_len != sizeof(EyeSyncMessage))
  {
    Serial.println("Invalid message size");
    return;
  }

  EyeSyncMessage msg;
  memcpy(&msg, data, sizeof(msg));
  memcpy(msg.macAddress, mac, 6);

  s_instance->m_lastRemoteState = msg;
  s_instance->m_lastRemoteTime = millis();

  if (s_instance->m_onDataReceived)
  {
    s_instance->m_onDataReceived(msg, mac);
  }
}
#endif

/** @brief Number of registered ESP-NOW peers. */
int EyeSyncManager::getPeerCount() const
{
  return m_peerCount;
}

/** @brief MAC address of the peer at the given index. */
const uint8_t *EyeSyncManager::getPeerMac(int index) const
{
  if (index < 0 || index >= m_peerCount)
    return nullptr;
  return m_peerMACS[index];
}

/** @brief Record the MAC address of the controller peer. */
void EyeSyncManager::setControllerMac(const uint8_t *mac)
{
  m_hasController = true;
  memcpy(m_controllerMac, mac, 6);
}

// ---------------------------------------------------------------------------
// EyeInterpolator
// ---------------------------------------------------------------------------

EyeInterpolator::EyeInterpolator()
{
  memset(&m_target, 0, sizeof(m_target));
  memset(&m_prev, 0, sizeof(m_prev));
  m_targetTime = 0;
  m_prevTime = 0;
}

/**
 * @brief Update with a new target state from the network.
 *
 * Rolls the current target into m_prev, stores the new remote state
 * as the target, and records its timestamp.
 */
void EyeInterpolator::updateTarget(const EyeSyncMessage &remote, uint32_t now)
{
  m_prev = m_target;
  m_prevTime = m_targetTime;
  m_target = remote;
  m_targetTime = now;
}

/**
 * @brief Smoothstep-interpolated eye X at the given time.
 *
 * Uses smoothstep easing (3t² - 2t³) between the previous and target
 * states. Returns target value if interpolation has completed.
 */
float EyeInterpolator::getX(uint32_t now) const
{
  if (m_targetTime == 0)
    return 0.5f;

  float t = (float)(now - m_targetTime) / 1000.0f;
  float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;

  if (t >= duration || duration <= 0)
  {
    return m_target.eyeX;
  }

  float e = t / duration;
  e = 3.0f * e * e - 2.0f * e * e * e;

  return m_prev.eyeX + (m_target.eyeX - m_prev.eyeX) * e;
}

/** @brief Smoothstep-interpolated eye Y. */
float EyeInterpolator::getY(uint32_t now) const
{
  if (m_targetTime == 0)
    return 0.5f;

  float t = (float)(now - m_targetTime) / 1000.0f;
  float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;

  if (t >= duration || duration <= 0)
  {
    return m_target.eyeY;
  }

  float e = t / duration;
  e = 3.0f * e * e - 2.0f * e * e * e;

  return m_prev.eyeY + (m_target.eyeY - m_prev.eyeY) * e;
}

/**
 * @brief Interpolated pupil factor.
 *
 * Uses simple linear interpolation (no smoothstep) since pupil
 * transitions are slower and smoother than eye position.
 */
float EyeInterpolator::getPupil(uint32_t now) const
{
  if (m_targetTime == 0)
    return 0.5f;

  float t = (float)(now - m_targetTime) / 1000.0f;
  float duration = (float)(m_targetTime - m_prevTime) / 1000.0f;

  if (t >= duration || duration <= 0)
  {
    return m_target.pupilFactor;
  }

  return m_prev.pupilFactor + (m_target.pupilFactor - m_prev.pupilFactor) * (t / duration);
}

/**
 * @brief Returns true if the last remote state is older than maxAge.
 * @param now Current time in milliseconds.
 * @param maxAge Staleness threshold in milliseconds (default 100ms).
 */
bool EyeInterpolator::isStale(uint32_t now, uint32_t maxAge) const
{
  return (now - m_targetTime) > maxAge;
}