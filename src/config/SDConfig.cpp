/**
 * @file SDConfig.cpp
 * @brief SD card configuration reader implementation.
 */
#include "SDConfig.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief Mount SD, read /eyes_config.json, unmount.
 *
 * SPI is initialised on the board's dedicated SD pins (SD_SCLK/MISO/MOSI/CS
 * from BoardPins.h). The card is unmounted after reading so that other code
 * can freely use the SPI bus if needed. All log messages are prefixed with
 * "[SDConfig]" for easy filtering.
 */
bool SDConfig::load(DeviceConfig &cfg, int csPin, int sckPin, int misoPin, int mosiPin)
{
  applyDefaults(cfg);

  SPI.begin(sckPin, misoPin, mosiPin, csPin);

  if (!SD.begin(csPin))
  {
    Serial.println("[SDConfig] No SD card detected — using defaults.");
    return false;
  }

  Serial.printf("[SDConfig] SD card mounted (%llu MB).\n",
                (unsigned long long)SD.cardSize() / (1024ULL * 1024ULL));

  if (!SD.exists(SD_CONFIG_PATH))
  {
    Serial.printf("[SDConfig] %s not found — using defaults.\n", SD_CONFIG_PATH);
    SD.end();
    return false;
  }

  File file = SD.open(SD_CONFIG_PATH, FILE_READ);
  if (!file)
  {
    Serial.printf("[SDConfig] Failed to open %s.\n", SD_CONFIG_PATH);
    SD.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  SD.end();

  if (err)
  {
    Serial.printf("[SDConfig] JSON parse error: %s — using defaults.\n", err.c_str());
    return false;
  }

  // --- eye section ---
  JsonObject eyeSect = doc["eye"];
  if (!eyeSect.isNull())
  {
    cfg.startEyeIndex = eyeSect["startIndex"] | 0;
    Serial.printf("[SDConfig] Start eye index: %d.\n", cfg.startEyeIndex);
  }

  // --- display section ---
  JsonObject disp = doc["display"];
  if (!disp.isNull())
  {
    cfg.displayUpsideDown = disp["upsideDown"] | false;
    Serial.printf("[SDConfig] Display upside-down: %s.\n", cfg.displayUpsideDown ? "true" : "false");
  }

  // --- network section ---
  JsonObject net = doc["network"];
  if (!net.isNull())
  {
    cfg.networkChannel = net["channel"] | 1;

    const char *key = net["key"] | "";
    if (key[0] != '\0')
    {
      deriveKey(key, cfg.networkPmk);
      cfg.networkToken = computeToken(cfg.networkPmk);
      Serial.printf("[SDConfig] Network key loaded, token = 0x%08" PRIX32 ".\n",
                    cfg.networkToken);
    }

    JsonArray macs = net["allowed_macs"];
    for (JsonVariant v : macs)
    {
      if (cfg.allowedMacCount >= SD_CONFIG_MAX_ALLOWED_MACS)
        break;
      const char *macStr = v.as<const char *>();
      if (macStr && parseMac(macStr, cfg.allowedMacs[cfg.allowedMacCount]))
      {
        Serial.printf("[SDConfig] Allowed MAC %d: %s\n",
                      cfg.allowedMacCount, macStr);
        cfg.allowedMacCount++;
      }
      else if (macStr)
      {
        Serial.printf("[SDConfig] WARNING: invalid MAC skipped: %s\n", macStr);
      }
    }
  }

  cfg.loaded = true;
  Serial.printf("[SDConfig] Configuration loaded (channel %d, %d allowed MAC(s)).\n",
                cfg.networkChannel, cfg.allowedMacCount);
  return true;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SDConfig::applyDefaults(DeviceConfig &cfg)
{
  cfg.loaded            = false;
  cfg.startEyeIndex     = 0;
  cfg.networkChannel    = 1;
  cfg.networkToken      = 0;
  cfg.allowedMacCount   = 0;
  cfg.displayUpsideDown = false;
  memset(cfg.networkPmk,   0, sizeof(cfg.networkPmk));
  memset(cfg.allowedMacs,  0, sizeof(cfg.allowedMacs));
}

/**
 * @brief Decode a passphrase into a 16-byte PMK.
 *
 * If the input is exactly 32 hexadecimal characters it is decoded directly
 * (two hex digits per byte). Otherwise the raw UTF-8 bytes are used,
 * truncated or zero-padded to exactly 16 bytes.
 */
void SDConfig::deriveKey(const char *passphrase, uint8_t pmk[16])
{
  memset(pmk, 0, 16);
  size_t len = strlen(passphrase);

  // 32-char hex string → decode two nibbles per byte
  if (len == 32)
  {
    bool allHex = true;
    for (size_t i = 0; i < 32 && allHex; i++)
    {
      char c = passphrase[i];
      allHex = (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }
    if (allHex)
    {
      auto fromHex = [](char c) -> uint8_t
      {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
        return (uint8_t)(c - 'A' + 10);
      };
      for (int i = 0; i < 16; i++)
        pmk[i] = (fromHex(passphrase[i * 2]) << 4) | fromHex(passphrase[i * 2 + 1]);
      return;
    }
  }

  // Raw passphrase bytes, truncated / zero-padded to 16
  memcpy(pmk, passphrase, len < 16 ? len : 16);
}

/**
 * @brief Compute a 32-bit application-layer token from a 16-byte PMK.
 *
 * Uses a simple polynomial hash seeded with "EYES". The result is guaranteed
 * non-zero (0 is reserved to mean "no token configured"). Devices with
 * different keys produce different tokens and therefore ignore each other.
 */
uint32_t SDConfig::computeToken(const uint8_t pmk[16])
{
  uint32_t token = 0x45594553u; // "EYES"
  for (int i = 0; i < 16; i++)
    token = token * 31u + pmk[i];
  return token ? token : 1u; // never return 0
}

/**
 * @brief Parse a colon-separated MAC string ("AA:BB:CC:DD:EE:FF") into bytes.
 * @return true if the string was a valid 17-character MAC.
 */
bool SDConfig::parseMac(const char *str, uint8_t out[6])
{
  if (!str || strlen(str) != 17)
    return false;

  for (int i = 0; i < 6; i++)
  {
    char hex[3] = {str[i * 3], str[i * 3 + 1], '\0'};
    char *end;
    long val = strtol(hex, &end, 16);
    if (*end != '\0' || val < 0 || val > 255)
      return false;
    out[i] = (uint8_t)val;
    if (i < 5 && str[i * 3 + 2] != ':')
      return false;
  }
  return true;
}
