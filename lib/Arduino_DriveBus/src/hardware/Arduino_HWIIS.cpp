/*
 * @Description: Arduino_HWIIS.cpp
 * @version: V1.0.0
 * @Author: LILYGO_L
 * @Date: 2023-12-20 15:46:16
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-12-12 11:01:18
 * @License: GPL 3.0
 */
#include "Arduino_HWIIS.h"

Arduino_HWIIS::Arduino_HWIIS(i2s_port_t iis_num, int8_t bclk, int8_t lrck, int8_t data)
    : _iis_num(iis_num), _bclk(bclk), _lrck(lrck), _data(data)
{
}

bool Arduino_HWIIS::begin(i2s_mode_t iis_mode, ad_iis_data_mode_t device_state, i2s_channel_fmt_t channel_mode,
                          int8_t bits_per_sample, int32_t sample_rate)
{
    _iis_mode = iis_mode;
    _device_state = device_state;
    _channel_mode = channel_mode;
    _bits_per_sample = (bits_per_sample == DRIVEBUS_DEFAULT_VALUE) ? 16 : bits_per_sample;
    _sample_rate = (sample_rate == DRIVEBUS_DEFAULT_VALUE) ? 44100U : sample_rate;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(_iis_num, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 1024;

    if (device_state == AD_IIS_DATA_OUT)
    {
        if (i2s_new_channel(&chan_cfg, &_tx_handle, nullptr) != ESP_OK)
        {
            return false;
        }

        i2s_std_gpio_config_t gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = (gpio_num_t)_bclk,
            .ws = (gpio_num_t)_lrck,
            .dout = (gpio_num_t)_data,
            .din = GPIO_NUM_NC,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        };

        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate);
        clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_768;

        i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)_bits_per_sample, I2S_SLOT_MODE_STEREO);
        (void)channel_mode;

        i2s_std_config_t std_cfg = {
            .clk_cfg = clk_cfg,
            .slot_cfg = slot_cfg,
            .gpio_cfg = gpio_cfg,
        };

        if (i2s_channel_init_std_mode(_tx_handle, &std_cfg) != ESP_OK)
        {
            i2s_del_channel(_tx_handle);
            _tx_handle = nullptr;
            return false;
        }

        if (i2s_channel_enable(_tx_handle) != ESP_OK)
        {
            return false;
        }
    }
    else if (device_state == AD_IIS_DATA_IN)
    {
        if (i2s_new_channel(&chan_cfg, nullptr, &_rx_handle) != ESP_OK)
        {
            return false;
        }

        i2s_std_gpio_config_t gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = (gpio_num_t)_bclk,
            .ws = (gpio_num_t)_lrck,
            .dout = GPIO_NUM_NC,
            .din = (gpio_num_t)_data,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        };

        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate);
        clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_768;

        i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)_bits_per_sample, I2S_SLOT_MODE_STEREO);
        (void)channel_mode;

        i2s_std_config_t std_cfg = {
            .clk_cfg = clk_cfg,
            .slot_cfg = slot_cfg,
            .gpio_cfg = gpio_cfg,
        };

        if (i2s_channel_init_std_mode(_rx_handle, &std_cfg) != ESP_OK)
        {
            i2s_del_channel(_rx_handle);
            _rx_handle = nullptr;
            return false;
        }

        if (i2s_channel_enable(_rx_handle) != ESP_OK)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

size_t Arduino_HWIIS::Read(void *data, size_t bytes)
{
    if (_rx_handle == nullptr)
    {
        return 0;
    }
    size_t bytes_read = 0;
    i2s_channel_read(_rx_handle, data, bytes, &bytes_read, portMAX_DELAY);
    return bytes_read;
}

size_t Arduino_HWIIS::Write(const void *data, size_t bytes)
{
    if (_tx_handle == nullptr)
    {
        return 0;
    }
    size_t bytes_written = 0;
    i2s_channel_write(_tx_handle, data, bytes, &bytes_written, portMAX_DELAY);
    return bytes_written;
}

bool Arduino_HWIIS::end()
{
    bool result = true;
    if (_tx_handle != nullptr)
    {
        if (i2s_channel_disable(_tx_handle) != ESP_OK)
        {
            result = false;
        }
        if (i2s_del_channel(_tx_handle) != ESP_OK)
        {
            result = false;
        }
        _tx_handle = nullptr;
    }
    if (_rx_handle != nullptr)
    {
        if (i2s_channel_disable(_rx_handle) != ESP_OK)
        {
            result = false;
        }
        if (i2s_del_channel(_rx_handle) != ESP_OK)
        {
            result = false;
        }
        _rx_handle = nullptr;
    }
    return result;
}