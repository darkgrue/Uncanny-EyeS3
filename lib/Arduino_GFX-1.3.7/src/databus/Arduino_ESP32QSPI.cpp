#include "Arduino_ESP32QSPI.h"

#if defined(ESP32)

/**
 * @brief Arduino_ESP32QSPI
 *
 */
Arduino_ESP32QSPI::Arduino_ESP32QSPI(
    int8_t cs, int8_t sck, int8_t mosi, int8_t miso, int8_t quadwp, int8_t quadhd, bool is_shared_interface /* = false */)
    : _cs(cs), _sck(sck), _mosi(mosi), _miso(miso), _quadwp(quadwp), _quadhd(quadhd), _is_shared_interface(is_shared_interface)
{
}

/**
 * @brief begin
 *
 * @param speed
 * @param dataMode
 * @return true
 * @return false
 */
bool Arduino_ESP32QSPI::begin(int32_t speed, int8_t dataMode)
{
  // set SPI parameters
  _speed = (speed == GFX_NOT_DEFINED) ? QSPI_FREQUENCY : speed;
  _dataMode = (dataMode == GFX_NOT_DEFINED) ? QSPI_SPI_MODE : dataMode;

  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH); // disable chip select
#if (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
  if (_cs >= 32)
  {
    _csPinMask = digitalPinToBitMask(_cs);
    _csPortSet = (PORTreg_t)&GPIO.out1_w1ts.val;
    _csPortClr = (PORTreg_t)&GPIO.out1_w1tc.val;
  }
  else
#endif
      if (_cs != GFX_NOT_DEFINED)
  {
    _csPinMask = digitalPinToBitMask(_cs);
    _csPortSet = (PORTreg_t)&GPIO.out_w1ts;
    _csPortClr = (PORTreg_t)&GPIO.out_w1tc;
  }

  spi_bus_config_t buscfg = {
      .mosi_io_num = _mosi,
      .miso_io_num = _miso,
      .sclk_io_num = _sck,
      .quadwp_io_num = _quadwp,
      .quadhd_io_num = _quadhd,
      .max_transfer_sz = (SPI_MAX_PIXELS_AT_ONCE * 16) + 8,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
  };
  esp_err_t ret = spi_bus_initialize(QSPI_SPI_HOST, &buscfg, QSPI_DMA_CHANNEL);
  if (ret != ESP_OK)
  {
    ESP_ERROR_CHECK(ret);
    return false;
  }

  spi_device_interface_config_t devcfg = {
      .command_bits = 8,
      .address_bits = 24,
      .mode = _dataMode,
      .clock_speed_hz = _speed,
      .spics_io_num = -1, // avoid use system CS control
      .flags = SPI_DEVICE_HALFDUPLEX,
      .queue_size = 4,  // 4 transfer slots for efficient QSPI operation
  };
  ret = spi_bus_add_device(QSPI_SPI_HOST, &devcfg, &_handle);
  if (ret != ESP_OK)
  {
    ESP_ERROR_CHECK(ret);
    return false;
  }

  if (!_is_shared_interface)
  {
    spi_device_acquire_bus(_handle, portMAX_DELAY);
  }

  memset(&_spi_tran_ext, 0, sizeof(_spi_tran_ext));
  _spi_tran = (spi_transaction_t *)&_spi_tran_ext;

  return true;
}

/**
 * @brief beginWrite
 *
 */
void Arduino_ESP32QSPI::beginWrite()
{
  if (_is_shared_interface)
  {
    spi_device_acquire_bus(_handle, portMAX_DELAY);
  }
}

/**
 * @brief endWrite
 *
 */
void Arduino_ESP32QSPI::endWrite()
{
  if (_is_shared_interface)
  {
    spi_device_acquire_bus(_handle, portMAX_DELAY);
  }
}

/**
 * @brief writeCommand
 *
 * @param c
 */
void Arduino_ESP32QSPI::writeCommand(uint8_t c)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)c) << 8;
  _spi_tran_ext.base.tx_buffer = NULL;
  _spi_tran_ext.base.length = 0;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief writeCommand16
 *
 * @param c
 */
void Arduino_ESP32QSPI::writeCommand16(uint16_t c)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = c;
  _spi_tran_ext.base.tx_buffer = NULL;
  _spi_tran_ext.base.length = 0;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief write
 *
 * @param d
 */
void Arduino_ESP32QSPI::write(uint8_t d)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
  _spi_tran_ext.base.cmd = 0x32;
  _spi_tran_ext.base.addr = 0x003C00;
  _spi_tran_ext.base.tx_data[0] = d;
  _spi_tran_ext.base.length = 8;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief write16
 *
 * @param d
 */
void Arduino_ESP32QSPI::write16(uint16_t d)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
  _spi_tran_ext.base.cmd = 0x32;
  _spi_tran_ext.base.addr = 0x003C00;
  _spi_tran_ext.base.tx_data[0] = d >> 8;
  _spi_tran_ext.base.tx_data[1] = d;
  _spi_tran_ext.base.length = 16;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief writeC8D8
 *
 * @param c
 * @param d
 */
void Arduino_ESP32QSPI::writeC8D8(uint8_t c, uint8_t d)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)c) << 8;
  _spi_tran_ext.base.tx_data[0] = d;
  _spi_tran_ext.base.length = 8;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief writeC8D16D16
 *
 * @param c
 * @param d1
 * @param d2
 */
void Arduino_ESP32QSPI::writeC8D16(uint8_t c, uint16_t d)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)c) << 8;
  _spi_tran_ext.base.tx_data[0] = d >> 8;
  _spi_tran_ext.base.tx_data[1] = d;
  _spi_tran_ext.base.length = 16;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief writeC8D16D16
 *
 * @param c
 * @param d1
 * @param d2
 */
void Arduino_ESP32QSPI::writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2)
{
  CS_LOW();
  _spi_tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  _spi_tran_ext.base.cmd = 0x02;
  _spi_tran_ext.base.addr = ((uint32_t)c) << 8;
  _spi_tran_ext.base.tx_data[0] = d1 >> 8;
  _spi_tran_ext.base.tx_data[1] = d1;
  _spi_tran_ext.base.tx_data[2] = d2 >> 8;
  _spi_tran_ext.base.tx_data[3] = d2;
  _spi_tran_ext.base.length = 32;
  POLL_START();
  POLL_END();
  CS_HIGH();
}

/**
 * @brief writeRepeat
 *
 * @param p
 * @param len
 */
void Arduino_ESP32QSPI::writeRepeat(uint16_t p, uint32_t len)
{
  bool first_send = true;

  uint16_t bufLen = (len >= SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;
  int16_t xferLen, l;
  uint32_t c32;
  MSB_32_16_16_SET(c32, p, p);

  l = (bufLen + 1) / 2;
  for (uint32_t i = 0; i < l; i++)
  {
    _buffer32[i] = c32;
  }

  CS_LOW();
  // Issue pixels in blocks from temp buffer
  while (len) // While pixels remain
  {
    xferLen = (bufLen <= len) ? bufLen : len; // How many this pass?

    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    _spi_tran_ext.base.tx_buffer = _buffer16;
    _spi_tran_ext.base.length = xferLen << 4;

    POLL_START();
    POLL_END();

    len -= xferLen;
  }
  CS_HIGH();
}

/**
 * @brief writePixels
 *
 * @param data
 * @param len
 */
void Arduino_ESP32QSPI::writePixels(uint16_t *data, uint32_t len)
{

  CS_LOW();
  uint32_t l, l2;
  uint16_t p1, p2;
  bool first_send = true;
  while (len)
  {
    l = (len > SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;

    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    l2 = l >> 1;
    for (uint32_t i = 0; i < l2; ++i)
    {
      p1 = *data++;
      p2 = *data++;
      MSB_32_16_16_SET(_buffer32[i], p1, p2);
    }
    if (l & 1)
    {
      p1 = *data++;
      MSB_16_SET(_buffer16[l - 1], p1);
    }

    _spi_tran_ext.base.tx_buffer = _buffer32;
    _spi_tran_ext.base.length = l << 4;

    POLL_START();
    POLL_END();

    len -= l;
  }
  CS_HIGH();
}

/**
 * @brief writeBytes
 *
 * @param data
 * @param len
 */
void Arduino_ESP32QSPI::writeBytes(uint8_t *data, uint32_t len)
{
  CS_LOW();
  uint32_t l;
  bool first_send = true;
  while (len)
  {
    l = (len >= (SPI_MAX_PIXELS_AT_ONCE << 1)) ? (SPI_MAX_PIXELS_AT_ONCE << 1) : len;

    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }

    _spi_tran_ext.base.tx_buffer = data;
    _spi_tran_ext.base.length = l << 3;

    POLL_START();
    POLL_END();

    len -= l;
    data += l;
  }
  CS_HIGH();
}

/**
 * @brief writeIndexedPixels
 *
 * @param data
 * @param idx
 * @param len
 */
void Arduino_ESP32QSPI::writeIndexedPixels(uint8_t *data, uint16_t *idx, uint32_t len)
{
  CS_LOW();
  uint32_t l, l2;
  uint16_t p1, p2;
  bool first_send = true;
  while (len)
  {
    l = (len > SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;

    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    l2 = l >> 1;
    for (uint32_t i = 0; i < l2; ++i)
    {
      p1 = idx[*data++];
      p2 = idx[*data++];
      MSB_32_16_16_SET(_buffer32[i], p1, p2);
    }
    if (l & 1)
    {
      p1 = idx[*data++];
      MSB_16_SET(_buffer16[l - 1], p1);
    }

    _spi_tran_ext.base.tx_buffer = _buffer32;
    _spi_tran_ext.base.length = l << 4;

    POLL_START();
    POLL_END();

    len -= l;
  }
  CS_HIGH();
}

/**
 * @brief writeIndexedPixelsDouble
 *
 * @param data
 * @param idx
 * @param len
 */
void Arduino_ESP32QSPI::writeIndexedPixelsDouble(uint8_t *data, uint16_t *idx, uint32_t len)
{
  CS_LOW();
  uint32_t l;
  uint16_t p;
  bool first_send = true;
  while (len)
  {
    l = (len > (SPI_MAX_PIXELS_AT_ONCE >> 1)) ? (SPI_MAX_PIXELS_AT_ONCE >> 1) : len;

    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    for (uint32_t i = 0; i < l; ++i)
    {
      p = idx[*data++];
      MSB_32_16_16_SET(_buffer32[i], p, p);
    }

    _spi_tran_ext.base.tx_buffer = _buffer32;
    _spi_tran_ext.base.length = l << 5;

    POLL_START();
    POLL_END();

    len -= l;
  }
  CS_HIGH();
}

/******** low level bit twiddling **********/

/**
 * @brief CS_HIGH
 *
 * @return INLINE
 */
INLINE void Arduino_ESP32QSPI::CS_HIGH(void)
{
  *_csPortSet = _csPinMask;
}

/**
 * @brief CS_LOW
 *
 * @return INLINE
 */
INLINE void Arduino_ESP32QSPI::CS_LOW(void)
{
  *_csPortClr = _csPinMask;
}

/**
 * @brief POLL_START
 *
 * @return INLINE
 */
INLINE void Arduino_ESP32QSPI::POLL_START()
{
  esp_err_t ret = spi_device_polling_start(_handle, _spi_tran, portMAX_DELAY);
  // if (ret != ESP_OK)
  // {
  //   log_e("spi_device_polling_start error: %d", ret);
  // }
}

/**
 * @brief POLL_END
 *
 * @return INLINE
 */
INLINE void Arduino_ESP32QSPI::POLL_END()
{
  esp_err_t ret = spi_device_polling_end(_handle, portMAX_DELAY);
  // if (ret != ESP_OK)
  // {
  //   log_e("spi_device_polling_end error: %d", ret);
  // }
}

/**
 * @brief Queue a transaction asynchronously - returns immediately
 */
bool Arduino_ESP32QSPI::queueTrans(uint32_t len)
{
  if (_transPending) {
    return false;  // Already have one pending
  }

  // Queue the transaction (non-blocking)
  esp_err_t ret = spi_device_queue_trans(_handle, _spi_tran, 0);  // 0 = no wait if queue full
  if (ret == ESP_OK) {
    _transPending = true;
    _lastTransLen = len;
    return true;
  }
  return false;
}

/**
 * @brief Wait for queued transaction to complete
 */
bool Arduino_ESP32QSPI::waitTransComplete(uint32_t timeout_ms)
{
  if (!_transPending) {
    return true;  // Nothing pending
  }

  spi_transaction_t *rtrans = nullptr;
  esp_err_t ret = spi_device_get_trans_result(_handle, &rtrans, pdMS_TO_TICKS(timeout_ms));
  if (ret == ESP_OK) {
    _transPending = false;
    return true;
  }
  return false;
}

/**
 * @brief Check if transaction is complete (non-blocking)
 */
bool Arduino_ESP32QSPI::isTransComplete()
{
  if (!_transPending) {
    return true;
  }

  spi_transaction_t *rtrans = nullptr;
  esp_err_t ret = spi_device_get_trans_result(_handle, &rtrans, 0);  // 0 = non-blocking
  if (ret == ESP_OK) {
    _transPending = false;
    return true;
  }
  return false;
}

/**
 * @brief Begin async write - sets up address window and asserts CS
 */
void Arduino_ESP32QSPI::beginAsyncWrite()
{
  // Queue a dummy transaction just to assert CS LOW
  // The actual pixel data will be queued in asyncWritePixels
  _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
  _spi_tran_ext.base.cmd = 0x32;
  _spi_tran_ext.base.addr = 0x003C00;
  _spi_tran_ext.base.tx_buffer = nullptr;
  _spi_tran_ext.base.length = 0;

  esp_err_t ret = spi_device_queue_trans(_handle, _spi_tran, portMAX_DELAY);
  if (ret != ESP_OK) {
    log_e("beginAsyncWrite: queue_trans error: %d", ret);
  }
}

/**
 * @brief Queue entire pixel buffer as ONE async transaction
 */
void Arduino_ESP32QSPI::asyncWriteAllPixels(uint16_t *data, uint32_t totalLen)
{
  // For CO5300, we pack pixels into the internal buffer and send as one transfer
  // This only works if totalLen <= SPI_MAX_PIXELS_AT_ONCE
  // For larger transfers, we need to do chunked transfers

  uint32_t len = totalLen;
  uint32_t offset = 0;

  while (len > 0)
  {
    uint32_t chunk = (len > SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;

    // Copy chunk to our buffer with pixel packing
    uint32_t l2 = chunk >> 1;
    uint16_t *src = data + offset;
    for (uint32_t i = 0; i < l2; ++i)
    {
      MSB_32_16_16_SET(_buffer32[i], src[i * 2], src[i * 2 + 1]);
    }
    if (chunk & 1)
    {
      MSB_16_SET(_buffer16[chunk - 1], src[chunk - 1]);
    }

    bool first_send = (offset == 0);
    if (first_send)
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      _spi_tran_ext.base.cmd = 0x32;
      _spi_tran_ext.base.addr = 0x003C00;
    }
    else
    {
      _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }

    _spi_tran_ext.base.tx_buffer = _buffer32;
    _spi_tran_ext.base.length = chunk << 4;  // 16 bits per pixel

    esp_err_t ret = spi_device_queue_trans(_handle, _spi_tran, portMAX_DELAY);
    if (ret != ESP_OK) {
      log_e("asyncWriteAllPixels: queue_trans error: %d", ret);
      break;
    }

    _transPending = true;
    offset += chunk;
    len -= chunk;

    // For non-first chunks, we need to handle the fact that _spi_tran is reused
    // Each queue_trans reuses _spi_tran, so we need to be careful about timing
    if (!first_send) {
      // Wait for previous transaction before queueing next
      spi_transaction_t *rtrans = nullptr;
      spi_device_get_trans_result(_handle, &rtrans, portMAX_DELAY);
    }
  }
}

/**
 * @brief End async write - deassert CS
 */
void Arduino_ESP32QSPI::endAsyncWrite()
{
  // Assert CS HIGH
  CS_HIGH();

  // Wait for the last transaction to complete
  spi_transaction_t *rtrans = nullptr;
  esp_err_t ret = spi_device_get_trans_result(_handle, &rtrans, 10000);  // 10s timeout

  if (ret != ESP_OK) {
    log_e("endAsyncWrite: get_trans_result error: %d", ret);
  }

  _transPending = false;
}

/**
 * @brief Queue a single transaction and return immediately (non-blocking)
 */
bool Arduino_ESP32QSPI::queueSingleTrans(uint16_t *data, uint32_t len)
{
  if (_transPending) {
    return false;  // Already have one pending
  }

  // Pack into our buffer
  uint32_t l2 = len >> 1;
  for (uint32_t i = 0; i < l2; ++i)
  {
    MSB_32_16_16_SET(_buffer32[i], data[i * 2], data[i * 2 + 1]);
  }
  if (len & 1)
  {
    MSB_16_SET(_buffer16[len - 1], data[len - 1]);
  }

  _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
  _spi_tran_ext.base.cmd = 0x32;
  _spi_tran_ext.base.addr = 0x003C00;
  _spi_tran_ext.base.tx_buffer = _buffer32;
  _spi_tran_ext.base.length = len << 4;

  esp_err_t ret = spi_device_queue_trans(_handle, _spi_tran, 0);  // 0 = non-blocking
  if (ret == ESP_OK) {
    _transPending = true;
    _lastTransLen = len;
    return true;
  }
  return false;
}

/**
 * @brief Wait for pending transaction to complete
 */
bool Arduino_ESP32QSPI::waitSingleTrans(uint32_t timeout_ms)
{
  if (!_transPending) {
    return true;
  }

  spi_transaction_t *rtrans = nullptr;
  esp_err_t ret = spi_device_get_trans_result(_handle, &rtrans, pdMS_TO_TICKS(timeout_ms));
  if (ret == ESP_OK) {
    _transPending = false;
    return true;
  }
  return false;
}

/**
 * @brief Queue a chunk of pixel data asynchronously
 * @param data Pixel data buffer
 * @param len Number of bytes to transfer
 * @param isFirst True if this is the first chunk (sets up address window)
 * @param isLast True if this is the last chunk (ends the transfer)
 * @return true if queued successfully, false if queue is full
 */
bool Arduino_ESP32QSPI::queueChunk(uint8_t *data, uint32_t len, bool isFirst, bool isLast)
{
  // Find a free transaction slot
  uint8_t slot = 0;
  for (uint8_t i = 0; i < ASYNC_QUEUE_SIZE; i++) {
    if (!_asyncTransUsed[i]) {
      slot = i;
      break;
    }
  }
  if (_asyncTransUsed[slot]) {
    return false;  // No free slots
  }

  // Configure transaction flags
  uint32_t flags = SPI_TRANS_MODE_QIO;
  if (!isFirst) {
    flags |= SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
  }

  _asyncTrans[slot].base.flags = flags;
  _asyncTrans[slot].base.cmd = 0x32;
  _asyncTrans[slot].base.addr = 0x003C00;
  _asyncTrans[slot].base.tx_buffer = data;
  _asyncTrans[slot].base.length = len << 3;  // bytes to bits

  // Queue the transaction
  esp_err_t ret = spi_device_queue_trans(_handle, &_asyncTrans[slot].base, 0);  // non-blocking
  if (ret == ESP_OK) {
    _asyncTransUsed[slot] = 1;
    _activeTransCount++;
    return true;
  }
  return false;
}

/**
 * @brief Wait for all queued chunks to complete
 */
bool Arduino_ESP32QSPI::waitAllChunks(uint32_t timeout_ms)
{
  uint32_t startMs = millis();
  uint32_t remaining = timeout_ms;

  while (_activeTransCount > 0) {
    if (millis() - startMs >= timeout_ms) {
      return false;  // Timeout
    }

    // Check each slot
    for (uint8_t i = 0; i < ASYNC_QUEUE_SIZE; i++) {
      if (_asyncTransUsed[i]) {
        spi_transaction_t *rtrans = nullptr;
        esp_err_t ret = spi_device_get_trans_result(_handle, &rtrans, pdMS_TO_TICKS(remaining));
        if (ret == ESP_OK) {
          _asyncTransUsed[i] = 0;
          _activeTransCount--;
        }
      }
    }

    if (_activeTransCount > 0) {
      vTaskDelay(1);
    }
  }

  return true;
}

#endif // #if defined(ESP32)
