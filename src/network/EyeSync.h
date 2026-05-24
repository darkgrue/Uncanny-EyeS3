/**
 * @file EyeSync.h
 * @brief ESP-NOW peer synchronization for multi-eye setups.
 *
 * One device acts as the controller (receives input from a WiiChuck or other
 * source) and broadcasts its eye state to follower devices over ESP-NOW.
 * Followers interpolate the received state to maintain smooth local animation.
 *
 * Supports both the legacy ESP-IDF v4.x API and the new v5.x API through
 * compile-time version detection.
 */
#ifndef EYE_SYNC_H
#define EYE_SYNC_H

#include "common/EyeState.h"
#include <esp_now.h>
#include <WiFi.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define ESP_NOW_NEW_API 1
#else
#define ESP_NOW_NEW_API 0
#endif

/** @brief Callback: invoked when a peer joins the ESP-NOW network. */
typedef void (*OnPeerJoined)(const uint8_t *mac);

/** @brief Callback: invoked when a peer leaves the ESP-NOW network. */
typedef void (*OnPeerLeft)(const uint8_t *mac);

/** @brief Callback: invoked when an EyeSyncMessage is received. */
typedef void (*OnDataReceived)(const EyeSyncMessage &msg, const uint8_t *mac);

/**
 * @brief Manages ESP-NOW peer state and message routing.
 *
 * Handles peer registration, broadcast of EyeSyncMessage to all peers,
 * and reception with callbacks for UI updates. Only one controller
 * is supported at a time; the controller's MAC is tracked so followers
 * can identify the source of state updates.
 */
class EyeSyncManager
{
public:
  EyeSyncManager();

  /**
   * @brief Initialize ESP-NOW and register peers.
   * @param channel WiFi channel (default 1).
   * @return true if initialization succeeded.
   */
  bool begin(uint8_t channel = 1);

  /** @brief Register a callback for peer join events. */
  void onPeerJoined(OnPeerJoined callback) { m_onPeerJoined = callback; }

  /** @brief Register a callback for peer leave events. */
  void onPeerLeft(OnPeerLeft callback) { m_onPeerLeft = callback; }

  /** @brief Register a callback for incoming state messages. */
  void onDataReceived(OnDataReceived callback) { m_onDataReceived = callback; }

  /** @brief Broadcast an EyeSyncMessage to all peers. */
  void broadcast(const EyeSyncMessage &msg);

  /** @brief Send a message to a specific peer by MAC. */
  bool sendTo(const uint8_t *mac, const EyeSyncMessage &msg);

  /**
   * @brief Register or refresh a peer MAC for peer count tracking.
   *
   * Updates the last-seen timestamp for known peers and adds new peers up
   * to the 8-entry limit. Called internally by onDataReceived().
   * @return true if the peer is known (newly added or already present).
   */
  bool addPeer(const uint8_t *mac);

  /**
   * @brief Remove and log peers silent for longer than timeoutMs.
   *
   * Compacts the peer list when a peer is dropped. If the dropped peer was
   * the registered controller, hasController() resets to false so followers
   * immediately fall back to autonomous movement. Call periodically (e.g.
   * every 5 s from loop()).
   * @param timeoutMs Silence threshold in milliseconds (default 5000).
   */
  void pruneDropped(uint32_t timeoutMs = 5000);

  /** @brief Number of active ESP-NOW peers. */
  int getPeerCount() const;

  /** @brief MAC address of peer at the given index. */
  const uint8_t *getPeerMac(int index) const;

  /** @brief True when a controller peer has been identified. */
  bool hasController() const;

  /** @brief Set the controller's MAC address. */
  void setControllerMac(const uint8_t *mac);

  /** @brief Get the most recently received state from the controller. */
  EyeSyncMessage getLastRemoteState() const;

  /** @brief Millis() timestamp of the last received state. */
  uint32_t getLastRemoteTime() const;

  // --- Security configuration (call before begin()) ---

  /**
   * @brief Set the 16-byte ESP-NOW Primary Master Key.
   *
   * Applied via esp_now_set_pmk() inside begin(). Secures future unicast
   * encrypted peers. All devices in the network must share the same PMK.
   * Has no effect if called after begin().
   */
  void setNetworkPmk(const uint8_t pmk[16]);

  /**
   * @brief Set the application-layer authentication token.
   *
   * broadcast() stamps this value into every outgoing EyeSyncMessage.
   * onDataReceived() silently discards messages whose token does not match.
   * Token 0 disables checking (default; accepts all messages).
   */
  void setNetworkToken(uint32_t token) { m_networkToken = token; }

  /**
   * @brief Add a MAC address to the sender allowlist.
   *
   * When the allowlist is non-empty, messages from unlisted senders are
   * silently discarded (in addition to the token check). Up to 8 entries.
   */
  void addAllowedMac(const uint8_t mac[6]);

private:
  bool isAuthorized(const uint8_t *senderMac, uint32_t token) const;
  static EyeSyncManager *s_instance;

#if ESP_NOW_NEW_API
  static void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);
  static void onDataReceived(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
#else
  static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
  static void onDataReceived(const uint8_t *mac, const uint8_t *data, int data_len);
#endif

  // Spinlock protecting all shared state written by the WiFi callback and read
  // from Core 0 (pruneDropped) or Core 1 (render task). Declared mutable so
  // const getters can lock without a const_cast.
  mutable portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

  bool     m_initialized   = false;
  bool     m_hasController = false;
  uint8_t  m_controllerMac[6];
  uint8_t  m_channel = 1;
  EyeSyncMessage m_lastRemoteState;
  uint32_t m_lastRemoteTime = 0;

  OnPeerJoined   m_onPeerJoined   = nullptr;
  OnPeerLeft     m_onPeerLeft     = nullptr;
  OnDataReceived m_onDataReceived = nullptr;

  uint8_t  m_peerMACS[8][6];
  uint32_t m_peerLastSeen[8] = {0};
  int      m_peerCount = 0;

  // Security
  uint8_t  m_networkPmk[16]              = {0};  // ESP-NOW PMK (set before begin())
  bool     m_hasPmk                      = false;
  uint32_t m_networkToken                = 0;     // App-layer token (0 = disabled)
  uint8_t  m_allowedMacs[8][6]           = {};    // Sender MAC allowlist
  int      m_allowedMacCount             = 0;
};

#endif // EYE_SYNC_H