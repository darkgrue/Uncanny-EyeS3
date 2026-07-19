/**
 * @file EyeSync.cpp
 * @brief Implementation of ESP-NOW peer synchronization.
 *
 * Initializes ESP-NOW, registers send/receive callbacks, and routes incoming
 * EyeSyncMessage packets to registered callbacks. The static singleton pattern
 * (s_instance) is used to bridge the C-style ESP-NOW callbacks back to the
 * object instance. All shared state is protected with a portMUX_TYPE spinlock
 * so WiFi-task callbacks (any core) and the render task (Core 1) do not race.
 */
#include "EyeSync.h"
#include <Arduino.h>
#include <esp_wifi.h>

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

  // esp_now_add_peer()'s peer_info.channel field is only used to validate
  // against the radio's current channel — it does not move the radio there.
  // Without this call the device stays on whatever channel WiFi last used
  // (e.g. a previously-associated AP's channel cached in NVS), which can
  // silently diverge from another device's channel even when both configs
  // agree, so neither ever sees the other's ESP-NOW broadcasts.
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed");
    return false;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataReceived);

  // Apply PMK if one was configured before begin().
  if (m_hasPmk)
  {
    if (esp_now_set_pmk(m_networkPmk) != ESP_OK)
      Serial.println("[EyeSync] WARNING: esp_now_set_pmk() failed.");
    else
      Serial.println("[EyeSync] ESP-NOW PMK set.");
  }

  // Register the broadcast peer so broadcast() can send without prior discovery.
  // ESP-NOW requires all destinations to be registered via esp_now_add_peer()
  // before esp_now_send() will accept them.
  static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t bcast;
  memset(&bcast, 0, sizeof(bcast));
  memcpy(bcast.peer_addr, BROADCAST_MAC, 6);
  bcast.channel = channel;
  bcast.encrypt = false;
  if (esp_now_add_peer(&bcast) != ESP_OK)
  {
    Serial.println("[EyeSync] WARNING: failed to add broadcast peer");
  }

  m_initialized = true;
  m_channel = channel;
  return true;
}

/**
 * @brief Broadcast an EyeSyncMessage to all ESP-NOW devices on the channel.
 *
 * Stamps the configured network token into the message before sending.
 * Devices with a different (or absent) token will silently discard it.
 */
void EyeSyncManager::broadcast(const EyeSyncMessage &msg)
{
  if (!m_initialized)
    return;

  EyeSyncMessage stamped  = msg;
  stamped.networkToken    = m_networkToken;

  static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(BROADCAST_MAC, (const uint8_t *)&stamped, sizeof(stamped));
}

/** @brief Store the PMK for application during begin(). */
void EyeSyncManager::setNetworkPmk(const uint8_t pmk[16])
{
  memcpy(m_networkPmk, pmk, 16);
  m_hasPmk = true;
}

/** @brief Add a MAC to the sender allowlist (up to 8 entries). */
void EyeSyncManager::addAllowedMac(const uint8_t mac[6])
{
  if (m_allowedMacCount < 8)
    memcpy(m_allowedMacs[m_allowedMacCount++], mac, 6);
}

/**
 * @brief Return true if the sender is permitted to control this device.
 *
 * Two independent checks:
 *   Token:   if this device has a non-zero token, the message token must match.
 *   Allowlist: if the allowlist is non-empty, the sender MAC must appear in it.
 * Both checks must pass. Either check is skipped when not configured.
 */
bool EyeSyncManager::isAuthorized(const uint8_t *senderMac, uint32_t token) const
{
  if (m_networkToken != 0 && token != m_networkToken)
  {
    Serial.printf("[EyeSync] Rejected message: token 0x%08" PRIX32
                  " from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  token,
                  senderMac[0], senderMac[1], senderMac[2],
                  senderMac[3], senderMac[4], senderMac[5]);
    return false;
  }

  if (m_allowedMacCount > 0)
  {
    for (int i = 0; i < m_allowedMacCount; i++)
    {
      if (memcmp(senderMac, m_allowedMacs[i], 6) == 0)
        return true;
    }
    Serial.printf("[EyeSync] Rejected message: MAC %02X:%02X:%02X:%02X:%02X:%02X not in allowlist\n",
                  senderMac[0], senderMac[1], senderMac[2],
                  senderMac[3], senderMac[4], senderMac[5]);
    return false;
  }

  return true;
}

/**
 * @brief Register or refresh a peer MAC for peer count tracking.
 *
 * Refreshes the last-seen timestamp for known peers (called on every received
 * packet, so this keeps the drop-detection timer alive). Adds new peers up to
 * the 8-entry limit and logs the first appearance.
 */
bool EyeSyncManager::addPeer(const uint8_t *mac)
{
  uint32_t now = millis();

  taskENTER_CRITICAL(&m_mux);

  for (int i = 0; i < m_peerCount; i++)
  {
    if (memcmp(m_peerMACS[i], mac, 6) == 0)
    {
      m_peerLastSeen[i] = now;
      taskEXIT_CRITICAL(&m_mux);
      return true;
    }
  }

  if (m_peerCount >= 8)
  {
    taskEXIT_CRITICAL(&m_mux);
    return false;
  }

  memcpy(m_peerMACS[m_peerCount], mac, 6);
  m_peerLastSeen[m_peerCount] = now;
  m_peerCount++;
  int count = m_peerCount;
  taskEXIT_CRITICAL(&m_mux);

  Serial.printf("[EyeSync] New peer: %02X:%02X:%02X:%02X:%02X:%02X (%d total)\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], count);
  return true;
}

/**
 * @brief Remove and log peers silent for longer than timeoutMs.
 *
 * Uses swap-with-last compaction to avoid shifting the whole array. Resets
 * hasController() if the dropped peer was the registered controller.
 * Collects dropped MACs under the spinlock, then logs after releasing it so
 * Serial I/O does not run inside a critical section.
 */
void EyeSyncManager::pruneDropped(uint32_t timeoutMs)
{
  uint32_t now = millis();
  uint8_t droppedMacs[8][6];
  int droppedCount = 0;

  taskENTER_CRITICAL(&m_mux);
  for (int i = 0; i < m_peerCount; )
  {
    if (now - m_peerLastSeen[i] > timeoutMs)
    {
      memcpy(droppedMacs[droppedCount++], m_peerMACS[i], 6);

      if (m_hasController && memcmp(m_peerMACS[i], m_controllerMac, 6) == 0)
      {
        m_hasController = false;
        memset(m_controllerMac, 0, 6);
      }

      // Swap with last entry and shrink — avoids shifting the whole array.
      m_peerCount--;
      if (i < m_peerCount)
      {
        memcpy(m_peerMACS[i], m_peerMACS[m_peerCount], 6);
        m_peerLastSeen[i] = m_peerLastSeen[m_peerCount];
      }
      // Do not increment i: re-check the swapped-in entry.
    }
    else
    {
      i++;
    }
  }
  taskEXIT_CRITICAL(&m_mux);

  for (int i = 0; i < droppedCount; i++)
  {
    Serial.printf("[EyeSync] Peer dropped: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  droppedMacs[i][0], droppedMacs[i][1], droppedMacs[i][2],
                  droppedMacs[i][3], droppedMacs[i][4], droppedMacs[i][5]);
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
  const uint8_t *senderMac = (esp_now_info && esp_now_info->src_addr)
                                  ? esp_now_info->src_addr
                                  : msg.macAddress;
  memcpy(msg.macAddress, senderMac, 6);

  if (!s_instance->isAuthorized(senderMac, msg.networkToken))
    return;

  s_instance->addPeer(msg.macAddress);

  taskENTER_CRITICAL(&s_instance->m_mux);
  s_instance->m_lastRemoteState = msg;
  s_instance->m_lastRemoteTime = millis();
  taskEXIT_CRITICAL(&s_instance->m_mux);

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

  if (!s_instance->isAuthorized(mac, msg.networkToken))
    return;

  s_instance->addPeer(mac);

  taskENTER_CRITICAL(&s_instance->m_mux);
  s_instance->m_lastRemoteState = msg;
  s_instance->m_lastRemoteTime = millis();
  taskEXIT_CRITICAL(&s_instance->m_mux);

  if (s_instance->m_onDataReceived)
  {
    s_instance->m_onDataReceived(msg, mac);
  }
}
#endif

/** @brief Number of registered ESP-NOW peers. */
int EyeSyncManager::getPeerCount() const
{
  taskENTER_CRITICAL(&m_mux);
  int count = m_peerCount;
  taskEXIT_CRITICAL(&m_mux);
  return count;
}

/** @brief MAC address of the peer at the given index. */
const uint8_t *EyeSyncManager::getPeerMac(int index) const
{
  taskENTER_CRITICAL(&m_mux);
  if (index < 0 || index >= m_peerCount)
  {
    taskEXIT_CRITICAL(&m_mux);
    return nullptr;
  }
  taskEXIT_CRITICAL(&m_mux);
  return m_peerMACS[index];
}

/** @brief True when a controller peer has been identified. */
bool EyeSyncManager::hasController() const
{
  taskENTER_CRITICAL(&m_mux);
  bool r = m_hasController;
  taskEXIT_CRITICAL(&m_mux);
  return r;
}

/** @brief Get the most recently received state from the controller. */
EyeSyncMessage EyeSyncManager::getLastRemoteState() const
{
  taskENTER_CRITICAL(&m_mux);
  EyeSyncMessage r = m_lastRemoteState;
  taskEXIT_CRITICAL(&m_mux);
  return r;
}

/** @brief Millis() timestamp of the last received state. */
uint32_t EyeSyncManager::getLastRemoteTime() const
{
  taskENTER_CRITICAL(&m_mux);
  uint32_t r = m_lastRemoteTime;
  taskEXIT_CRITICAL(&m_mux);
  return r;
}

/** @brief Record the MAC address of the controller peer. */
void EyeSyncManager::setControllerMac(const uint8_t *mac)
{
  taskENTER_CRITICAL(&m_mux);
  m_hasController = true;
  memcpy(m_controllerMac, mac, 6);
  taskEXIT_CRITICAL(&m_mux);
}