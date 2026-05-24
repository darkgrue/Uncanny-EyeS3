/**
 * @file SDConfig.h
 * @brief SD card configuration reader for device settings.
 *
 * Reads /eyes_config.json from an SD card on the board's SPI pins at boot.
 * The system operates with safe defaults when no card or file is present —
 * SD is optional at runtime.
 *
 * Config file format (/eyes_config.json):
 * @code
 * {
 *   "network": {
 *     "channel": 1,
 *     "key": "SharedPassphrase",
 *     "allowed_macs": [
 *       "AA:BB:CC:DD:EE:FF"
 *     ]
 *   }
 * }
 * @endcode
 *
 * The "key" field accepts either a plain passphrase (up to 16 chars, zero-padded
 * to the required 16-byte ESP-NOW PMK width) or an exact 32 hex-digit string
 * ("0123456789ABCDEF0123456789ABCDEF") decoded directly to 16 bytes.
 * An application-layer token derived from the key is embedded in every
 * EyeSyncMessage so that devices with different keys silently ignore each other.
 */
#ifndef SD_CONFIG_H
#define SD_CONFIG_H

#include <stdint.h>
#include <stddef.h>

static constexpr int SD_CONFIG_MAX_ALLOWED_MACS = 8;
static constexpr const char *SD_CONFIG_PATH     = "/eyes_config.json";

/**
 * @brief Parsed device configuration.
 *
 * Populated by SDConfig::load(). All fields carry safe defaults so the rest
 * of the firmware can use this struct unconditionally regardless of whether
 * an SD card was present.
 */
struct DeviceConfig
{
  bool loaded;                                           ///< true if a config file was successfully parsed
  uint8_t  networkPmk[16];                              ///< 16-byte ESP-NOW PMK (all-zero = unset)
  uint32_t networkToken;                                 ///< App-layer auth token derived from key (0 = disabled)
  uint8_t  networkChannel;                               ///< ESP-NOW WiFi channel (1-13)
  uint8_t  allowedMacs[SD_CONFIG_MAX_ALLOWED_MACS][6];  ///< Sender MAC allowlist
  int      allowedMacCount;                              ///< Number of entries in allowedMacs (0 = allow all)
};

/**
 * @brief Mounts the SD card, reads the config file, and unmounts.
 *
 * The card is only needed at startup. After load() returns, the SPI bus
 * and SD card are released so they do not interfere with other peripherals.
 */
class SDConfig
{
public:
  /**
   * @brief Attempt to load /eyes_config.json from the SD card.
   *
   * Fills cfg with safe defaults first, then overlays any values found in
   * the file. Returns true if the file was read and parsed without error.
   * Returns false (with defaults in cfg) if the card is absent, the file is
   * missing, or the JSON is malformed.
   *
   * Pin numbers are passed by the caller (from BoardPins.h) so that this
   * class does not need to include BoardPins.h itself — that header defines
   * macros that conflict with Arduino variant constants when included before
   * Arduino.h.
   *
   * @param cfg     Output struct to populate.
   * @param csPin   SPI chip-select GPIO for the SD card.
   * @param sckPin  SPI clock GPIO.
   * @param misoPin SPI MISO GPIO.
   * @param mosiPin SPI MOSI GPIO.
   * @return true on successful load; false if defaults were used.
   */
  static bool load(DeviceConfig &cfg, int csPin, int sckPin, int misoPin, int mosiPin);

private:
  static void     applyDefaults(DeviceConfig &cfg);
  static bool     parseMac(const char *str, uint8_t out[6]);
  static void     deriveKey(const char *passphrase, uint8_t pmk[16]);
  static uint32_t computeToken(const uint8_t pmk[16]);
};

#endif // SD_CONFIG_H
