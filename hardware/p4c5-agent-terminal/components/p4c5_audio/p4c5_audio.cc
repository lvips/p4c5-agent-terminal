/**
 * @file p4c5_audio.cc
 * @brief P4C5 音频子系统实现
 *
 * 基于 xiaozhi main/audio/codecs/box_audio_codec.cc 移植。
 * 使用 espressif/esp_audio_codec ~2.4.1 提供的 es8311/es7210 codec 驱动。
 *
 * 关键差异 vs xiaozhi：
 *   - 此处为 C API 包装层（xiaozhi 是 C++ class）
 *   - I2C bus handle 由 p4c5_board 统一管理，通过 p4c5_audio_set_i2c_bus() 注入
 *   - I2S TX 用 STD 模式（DAC），RX 用 TDM 模式（ADC 4ch）
 *
 * 参考：
 *   - xiaozhi: main/audio/codecs/box_audio_codec.cc
 *   - xiaozhi: main/boards/kevin-p4c5-4g/config.h
 *   - espressif/esp_audio_codec: es8311_codec_new(), es7210_codec_new()
 */

#include "p4c5_audio.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/i2s_tdm.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <es8311_codec.h>
#include <es7210_codec.h>

static const char* TAG = "p4c5_audio";

/* ── 内部状态 ── */
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2s_chan_handle_t       s_tx_handle = NULL;
static i2s_chan_handle_t       s_rx_handle = NULL;
static const audio_codec_data_if_t*   s_data_if = NULL;
static const audio_codec_ctrl_if_t*   s_out_ctrl_if = NULL;
static const audio_codec_if_t*        s_out_codec_if = NULL;
static const audio_codec_ctrl_if_t*   s_in_ctrl_if = NULL;
static const audio_codec_if_t*        s_in_codec_if = NULL;
static const audio_codec_gpio_if_t*   s_gpio_if = NULL;
static esp_codec_dev_handle_t         s_output_dev = NULL;
static esp_codec_dev_handle_t         s_input_dev = NULL;
static bool s_initialized = false;

/* ── 内部函数 ── */

static esp_err_t create_duplex_channels(void)
{
    /* 创建 I2S duplex channel */
    i2s_chan_config_t chan_cfg = {
        .id = P4C5_I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle), TAG, "i2s_new_channel failed");

    /* TX: STD 模式 (DAC, 16bit stereo) */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = P4C5_AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = P4C5_I2S_MCLK_GPIO,
            .bclk = P4C5_I2S_BCLK_GPIO,
            .ws = P4C5_I2S_WS_GPIO,
            .dout = P4C5_I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_handle, &std_cfg), TAG, "TX init failed");

    /* RX: TDM 模式 (ADC, 4ch + AEC ref) */
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = P4C5_AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
                                              I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM,
        },
        .gpio_cfg = {
            .mclk = P4C5_I2S_MCLK_GPIO,
            .bclk = P4C5_I2S_BCLK_GPIO,
            .ws = P4C5_I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = P4C5_I2S_DIN_GPIO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_tdm_mode(s_rx_handle, &tdm_cfg), TAG, "RX init failed");

    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_handle), TAG, "TX enable failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx_handle), TAG, "RX enable failed");

    ESP_LOGI(TAG, "I2S duplex channels created (TX=STD, RX=TDM 4ch)");
    return ESP_OK;
}

/* ── 公开 API ── */

esp_err_t p4c5_audio_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing audio: ES8311(0x%02X) + ES7210(0x%02X) @ %dHz",
             P4C5_ES8311_I2C_ADDR, P4C5_ES7210_I2C_ADDR, P4C5_AUDIO_SAMPLE_RATE);

    /* 1. 创建 I2S duplex channels */
    ESP_RETURN_ON_ERROR(create_duplex_channels(), TAG, "Duplex channel creation failed");

    /* 2. 创建 audio_codec data interface */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = P4C5_I2S_PORT,
        .rx_handle = s_rx_handle,
        .tx_handle = s_tx_handle,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!s_data_if) {
        ESP_LOGE(TAG, "Failed to create I2S data interface");
        return ESP_ERR_NO_MEM;
    }

    /* 3. ES8311 (DAC output) */
    audio_codec_i2c_cfg_t i2c_out_cfg = {
        .port = P4C5_I2C_PORT,
        .addr = P4C5_ES8311_I2C_ADDR,
        .bus_handle = s_i2c_bus,
    };
    s_out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_out_cfg);
    if (!s_out_ctrl_if) return ESP_ERR_NO_MEM;

    s_gpio_if = audio_codec_new_gpio();
    if (!s_gpio_if) return ESP_ERR_NO_MEM;

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = s_out_ctrl_if;
    es8311_cfg.gpio_if = s_gpio_if;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin = P4C5_PA_EN_GPIO;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    s_out_codec_if = es8311_codec_new(&es8311_cfg);
    if (!s_out_codec_if) return ESP_ERR_NO_MEM;

    esp_codec_dev_cfg_t out_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = s_out_codec_if,
        .data_if = s_data_if,
    };
    s_output_dev = esp_codec_dev_new(&out_dev_cfg);
    if (!s_output_dev) return ESP_ERR_NO_MEM;

    /* 4. ES7210 (ADC input, 4 mic + AEC ref) */
    audio_codec_i2c_cfg_t i2c_in_cfg = {
        .port = P4C5_I2C_PORT,
        .addr = P4C5_ES7210_I2C_ADDR,
        .bus_handle = s_i2c_bus,
    };
    s_in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_in_cfg);
    if (!s_in_ctrl_if) return ESP_ERR_NO_MEM;

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = s_in_ctrl_if;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 |
                               ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    s_in_codec_if = es7210_codec_new(&es7210_cfg);
    if (!s_in_codec_if) return ESP_ERR_NO_MEM;

    esp_codec_dev_cfg_t in_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = s_in_codec_if,
        .data_if = s_data_if,
    };
    s_input_dev = esp_codec_dev_new(&in_dev_cfg);
    if (!s_input_dev) return ESP_ERR_NO_MEM;

    s_initialized = true;
    ESP_LOGI(TAG, "Audio initialized: ES8311(out) + ES7210(4mic+ref)");
    return ESP_OK;
}

int p4c5_audio_play(const int16_t* data, int samples)
{
    if (!s_initialized || !s_output_dev) return -1;
    esp_err_t err = esp_codec_dev_write(s_output_dev, (void*)data, samples * sizeof(int16_t));
    return (err == ESP_OK) ? samples : -1;
}

int p4c5_audio_record(int16_t* dest, int samples)
{
    if (!s_initialized || !s_input_dev) return -1;
    esp_err_t err = esp_codec_dev_read(s_input_dev, (void*)dest, samples * sizeof(int16_t));
    return (err == ESP_OK) ? samples : -1;
}

esp_err_t p4c5_audio_set_volume(int volume)
{
    if (!s_initialized || !s_output_dev) return ESP_ERR_INVALID_STATE;
    return esp_codec_dev_set_out_vol(s_output_dev, volume);
}

esp_err_t p4c5_audio_enable_input(bool enable)
{
    if (!s_initialized || !s_input_dev) return ESP_ERR_INVALID_STATE;
    // TODO: 实现完整的 input open/close 逻辑（参考 xiaozhi BoxAudioCodec::EnableInput）
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = P4C5_AUDIO_SAMPLE_RATE,
            .mclk_multiple = 0,
        };
        if (P4C5_AUDIO_INPUT_REF) {
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_input_dev, &fs), TAG, "input open failed");
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_channel_gain(
            s_input_dev, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 30), TAG, "set gain failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(s_input_dev), TAG, "input close failed");
    }
    return ESP_OK;
}

esp_err_t p4c5_audio_enable_output(bool enable)
{
    if (!s_initialized || !s_output_dev) return ESP_ERR_INVALID_STATE;
    // TODO: 实现完整的 output open/close 逻辑（参考 xiaozhi BoxAudioCodec::EnableOutput）
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = P4C5_AUDIO_SAMPLE_RATE,
            .mclk_multiple = 0,
        };
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_output_dev, &fs), TAG, "output open failed");
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_output_dev, 60), TAG, "set vol failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(s_output_dev), TAG, "output close failed");
    }
    return ESP_OK;
}

void p4c5_audio_deinit(void)
{
    if (!s_initialized) return;
    // TODO: 释放所有 codec 资源（参考 xiaozhi BoxAudioCodec::~BoxAudioCodec）
    ESP_LOGW(TAG, "Audio deinit — TODO: full cleanup");
    s_initialized = false;
}
