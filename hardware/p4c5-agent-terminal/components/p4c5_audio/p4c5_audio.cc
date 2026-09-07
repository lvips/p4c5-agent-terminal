/**
 * @file p4c5_audio.cc
 * @brief P4C5 完整音频子系统实现
 *
 * 基于 xiaozhi main/audio/codecs/box_audio_codec.cc 完整移植为独立 C API 组件。
 * 使用 espressif/esp_audio_codec ~2.4.1 提供的 ES8311/ES7210 codec 驱动。
 *
 * 关键架构决策（与 xiaozhi 一致）：
 *   - I2S TX: STD 模式（单声道 16bit）→ ES8311 DAC
 *   - I2S RX: TDM 模式（4 通道 16bit）← ES7210 ADC
 *   - 两者共享 MCLK/BCLK/WS 时钟线，通过 i2s_new_channel() 创建 duplex channel
 *   - ES8311 用 I2C 控制（port=I2C_NUM_0 或独立 port，由 bus_handle 决定）
 *   - AEC 参考：I2S RX 的 TDM slot1 用于回声消除参考通道
 *
 * 参考文件：
 *   - xiaozhi: main/audio/codecs/box_audio_codec.cc (完整实现)
 *   - xiaozhi: main/audio/codecs/box_audio_codec.h (类定义)
 *   - xiaozhi: main/boards/kevin-p4c5-4g/config.h (引脚)
 *   - xiaozhi: main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc (板级集成)
 */

#include "p4c5_audio.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/i2s_tdm.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <es8311_codec.h>
#include <es7210_adc.h>
#include <math.h>
#include <mutex>
#include <cstring>

static const char* TAG = "p4c5_audio";

/* ──────────────────────────────────────────────
 * DMA 配置常量（与 xiaozhi 一致）
 * ────────────────────────────────────────────── */
#define AUDIO_CODEC_DMA_DESC_NUM   6
#define AUDIO_CODEC_DMA_FRAME_NUM  240
#define AUDIO_CODEC_I2S_TIMEOUT    pdMS_TO_TICKS(1000)

/* ──────────────────────────────────────────────
 * 内部状态
 * ────────────────────────────────────────────── */
static i2c_master_bus_handle_t      s_i2c_bus       = NULL;
static i2s_chan_handle_t            s_tx_handle     = NULL;
static i2s_chan_handle_t            s_rx_handle     = NULL;
static const audio_codec_data_if_t* s_data_if       = NULL;
static const audio_codec_ctrl_if_t* s_out_ctrl_if   = NULL;
static const audio_codec_if_t*      s_out_codec_if  = NULL;
static const audio_codec_ctrl_if_t* s_in_ctrl_if    = NULL;
static const audio_codec_if_t*      s_in_codec_if   = NULL;
static const audio_codec_gpio_if_t* s_gpio_if       = NULL;
static esp_codec_dev_handle_t       s_output_dev    = NULL;
static esp_codec_dev_handle_t       s_input_dev     = NULL;
static std::mutex                   s_mutex;
static bool                         s_initialized   = false;
static bool                         s_input_enabled = false;
static bool                         s_output_enabled = false;
static int                          s_input_sample_rate  = P4C5_AUDIO_SAMPLE_RATE;
static int                          s_output_sample_rate = P4C5_AUDIO_SAMPLE_RATE;
static int                          s_input_gain    = 37;  /* dB (P1: 30→37, ES7210 max 37.5) */
static int                          s_output_volume = 60;  /* 0-100 */

/* ──────────────────────────────────────────────
 * 内部函数：创建 I2S duplex channel
 *
 * TX: STD 模式（ES8311 DAC, 单声道 16bit）
 * RX: TDM 模式（ES7210 ADC, 4 通道 16bit）
 * 共享 MCLK/BCLK/WS 引脚
 *
 * 参考: xiaozhi BoxAudioCodec::CreateDuplexChannels()
 * ────────────────────────────────────────────── */
static esp_err_t create_duplex_channels(void)
{
    ESP_RETURN_ON_FALSE(s_input_sample_rate == s_output_sample_rate,
                        ESP_ERR_INVALID_ARG, TAG,
                        "Input/output sample rate must match (both %d)",
                        s_input_sample_rate);

    /* 创建 duplex channel（TX + RX 共享时钟） */
    i2s_chan_config_t chan_cfg = {
        .id = P4C5_I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle),
        TAG, "i2s_new_channel failed");

    /* TX: STD 模式 (DAC → ES8311) */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)s_output_sample_rate,
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
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_tx_handle, &std_cfg),
        TAG, "TX STD init failed");

    /* RX: TDM 模式 (ADC ← ES7210, 4 麦通道) */
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)s_input_sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(
                I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
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
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_RETURN_ON_ERROR(
        i2s_channel_init_tdm_mode(s_rx_handle, &tdm_cfg),
        TAG, "RX TDM init failed");

    /* 使能两个通道 */
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_handle), TAG, "TX enable failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx_handle), TAG, "RX enable failed");

    ESP_LOGI(TAG, "I2S duplex: TX=STD(%dHz/16bit/mono) RX=TDM(%dHz/16bit/4ch)",
             s_output_sample_rate, s_input_sample_rate);
    ESP_LOGI(TAG, "  Pins: MCLK=%d BCLK=%d WS=%d DOUT=%d DIN=%d",
             P4C5_I2S_MCLK_GPIO, P4C5_I2S_BCLK_GPIO,
             P4C5_I2S_WS_GPIO, P4C5_I2S_DOUT_GPIO, P4C5_I2S_DIN_GPIO);

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：I2C bus 注入
 * ────────────────────────────────────────────── */

void p4c5_audio_set_i2c_bus(void* i2c_bus)
{
    s_i2c_bus = (i2c_master_bus_handle_t)i2c_bus;
    ESP_LOGI(TAG, "I2C bus handle set: %p", i2c_bus);
}

/* ──────────────────────────────────────────────
 * 公开 API：初始化
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_init(void)
{
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(s_i2c_bus != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "I2C bus not set — call p4c5_audio_set_i2c_bus() first");

    ESP_LOGI(TAG, "=== Audio init: ES8311(0x%02X) + ES7210(0x%02X) @ %dHz ===",
             P4C5_ES8311_I2C_ADDR, P4C5_ES7210_I2C_ADDR, P4C5_AUDIO_SAMPLE_RATE);

    /* ── Step 1: 创建 I2S duplex channels ── */
    ESP_RETURN_ON_ERROR(create_duplex_channels(), TAG, "Duplex channel creation failed");

    /* ── Step 2: 创建 audio_codec data interface ── */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = P4C5_I2S_PORT,
        .rx_handle = s_rx_handle,
        .tx_handle = s_tx_handle,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(s_data_if != NULL, ESP_ERR_NO_MEM, TAG,
                        "audio_codec_new_i2s_data failed");

    /* ── Step 3: ES8311 (DAC output) ── */
    {
        audio_codec_i2c_cfg_t i2c_out_cfg = {
            .port = P4C5_I2C_PORT,
            .addr = P4C5_ES8311_I2C_ADDR,
            .bus_handle = s_i2c_bus,
        };
        s_out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_out_cfg);
        ESP_RETURN_ON_FALSE(s_out_ctrl_if != NULL, ESP_ERR_NO_MEM, TAG,
                            "ES8311 ctrl_if failed");

        s_gpio_if = audio_codec_new_gpio();
        ESP_RETURN_ON_FALSE(s_gpio_if != NULL, ESP_ERR_NO_MEM, TAG,
                            "audio_codec_new_gpio failed");

        es8311_codec_cfg_t es8311_cfg = {};
        es8311_cfg.ctrl_if = s_out_ctrl_if;
        es8311_cfg.gpio_if = s_gpio_if;
        es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
        es8311_cfg.pa_pin = P4C5_PA_EN_GPIO;
        es8311_cfg.use_mclk = true;
        es8311_cfg.hw_gain.pa_voltage = 5.0f;
        es8311_cfg.hw_gain.codec_dac_voltage = 3.3f;
        s_out_codec_if = es8311_codec_new(&es8311_cfg);
        ESP_RETURN_ON_FALSE(s_out_codec_if != NULL, ESP_ERR_NO_MEM, TAG,
                            "es8311_codec_new failed");

        esp_codec_dev_cfg_t out_dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = s_out_codec_if,
            .data_if = s_data_if,
        };
        s_output_dev = esp_codec_dev_new(&out_dev_cfg);
        ESP_RETURN_ON_FALSE(s_output_dev != NULL, ESP_ERR_NO_MEM, TAG,
                            "ES8311 output_dev failed");
    }
    ESP_LOGI(TAG, "ES8311 DAC initialized (addr=0x%02X, PA=GPIO%d)",
             P4C5_ES8311_I2C_ADDR, P4C5_PA_EN_GPIO);

    /* ── Step 4: ES7210 (ADC input, 4 mic + AEC ref) ── */
    {
        audio_codec_i2c_cfg_t i2c_in_cfg = {
            .port = P4C5_I2C_PORT,
            .addr = P4C5_ES7210_I2C_ADDR,
            .bus_handle = s_i2c_bus,
        };
        s_in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_in_cfg);
        ESP_RETURN_ON_FALSE(s_in_ctrl_if != NULL, ESP_ERR_NO_MEM, TAG,
                            "ES7210 ctrl_if failed");

        es7210_codec_cfg_t es7210_cfg = {};
        es7210_cfg.ctrl_if = s_in_ctrl_if;
        /* 官方方案 (与 p4c5_board_test 一致): MIC1 主麦 + MIC3 ref
         *  不再选 MIC2/MIC4 (这俩在 P4C5 板上未启用) */
        es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC3;
        s_in_codec_if = es7210_codec_new(&es7210_cfg);
        ESP_RETURN_ON_FALSE(s_in_codec_if != NULL, ESP_ERR_NO_MEM, TAG,
                            "es7210_codec_new failed");

        esp_codec_dev_cfg_t in_dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
            .codec_if = s_in_codec_if,
            .data_if = s_data_if,
        };
        s_input_dev = esp_codec_dev_new(&in_dev_cfg);
        ESP_RETURN_ON_FALSE(s_input_dev != NULL, ESP_ERR_NO_MEM, TAG,
                            "ES7210 input_dev failed");
    }
    ESP_LOGI(TAG, "ES7210 ADC initialized (addr=0x%02X, 4mic%s)",
             P4C5_ES7210_I2C_ADDR,
             P4C5_AUDIO_INPUT_REF ? " + AEC ref" : "");

    /* W7 诊断: dump ES7210 关键寄存器, 验证 mic bias + power + gain */
    {
        /* 通过 ctrl_if 读寄存器 (8-bit addr, 8-bit value) */
        uint8_t regs[][2] = {
            /* addr, label */
            {0x01, 0x00},   /* CLOCK_OFF_REG01 */
            {0x07, 0x00},   /* OSR_REG07 */
            {0x08, 0x00},   /* MODE_CONFIG_REG08 */
            {0x40, 0x00},   /* ANALOG_REG40 */
            {0x41, 0x00},   /* MIC12_BIAS_REG41 */
            {0x42, 0x00},   /* MIC34_BIAS_REG42 */
            {0x43, 0x00},   /* MIC1_GAIN_REG43 */
            {0x44, 0x00},   /* MIC2_GAIN_REG44 */
            {0x45, 0x00},   /* MIC3_GAIN_REG45 */
            {0x46, 0x00},   /* MIC4_GAIN_REG46 */
            {0x4B, 0x00},   /* MIC12_POWER_REG4B */
            {0x4C, 0x00},   /* MIC34_POWER_REG4C */
        };
        const char* labels[] = {
            "CLOCK_OFF", "OSR", "MODE_CFG", "ANALOG",
            "MIC12_BIAS", "MIC34_BIAS",
            "MIC1_GAIN", "MIC2_GAIN", "MIC3_GAIN", "MIC4_GAIN",
            "MIC12_POWER", "MIC34_POWER"
        };
        ESP_LOGI(TAG, "─── ES7210 register dump ───");
        for (int i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
            uint8_t val = 0;
            if (s_in_ctrl_if && s_in_ctrl_if->read_reg) {
                s_in_ctrl_if->read_reg(s_in_ctrl_if, regs[i][0], 1, &val, 1);
            }
            ESP_LOGI(TAG, "  REG 0x%02X %-12s = 0x%02X",
                     regs[i][0], labels[i], val);
        }
        ESP_LOGI(TAG, "─────────────────────────────");
    }

    s_initialized = true;
    /* 对应测试 TC-01 / TC-02：日志输出 init 完成 */
    ESP_LOGI(TAG, "BoxAudioDevice initialized");
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：反初始化
 *
 * 释放顺序：codec dev → codec if → ctrl if → gpio if → data if → I2S
 * 对应测试 TC-11
 * ────────────────────────────────────────────── */

void p4c5_audio_deinit(void)
{
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing audio...");

    /* 关闭 codec 设备 */
    if (s_output_dev) {
        esp_codec_dev_close(s_output_dev);
        esp_codec_dev_delete(s_output_dev);
        s_output_dev = NULL;
    }
    if (s_input_dev) {
        esp_codec_dev_close(s_input_dev);
        esp_codec_dev_delete(s_input_dev);
        s_input_dev = NULL;
    }

    /* 释放 codec 接口 */
    if (s_in_codec_if) {
        audio_codec_delete_codec_if(s_in_codec_if);
        s_in_codec_if = NULL;
    }
    if (s_in_ctrl_if) {
        audio_codec_delete_ctrl_if(s_in_ctrl_if);
        s_in_ctrl_if = NULL;
    }
    if (s_out_codec_if) {
        audio_codec_delete_codec_if(s_out_codec_if);
        s_out_codec_if = NULL;
    }
    if (s_out_ctrl_if) {
        audio_codec_delete_ctrl_if(s_out_ctrl_if);
        s_out_ctrl_if = NULL;
    }
    if (s_gpio_if) {
        audio_codec_delete_gpio_if(s_gpio_if);
        s_gpio_if = NULL;
    }
    if (s_data_if) {
        audio_codec_delete_data_if(s_data_if);
        s_data_if = NULL;
    }

    /* 释放 I2S channels */
    if (s_tx_handle) {
        i2s_channel_disable(s_tx_handle);
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
    }
    if (s_rx_handle) {
        i2s_channel_disable(s_rx_handle);
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }

    s_initialized = false;
    s_input_enabled = false;
    s_output_enabled = false;

    ESP_LOGI(TAG, "Audio deinitialized (all resources freed)");
}

/* ──────────────────────────────────────────────
 * 公开 API：播放
 * 对应测试 TC-04, TC-12
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_play(const int16_t* data, size_t samples)
{
    if (!s_initialized || !s_output_dev || !s_output_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes = samples * sizeof(int16_t);
    size_t written = 0;
    esp_err_t err = esp_codec_dev_write(s_output_dev, (void*)data, bytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_write failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：录音（单声道混合）
 * 对应测试 TC-08
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_record(int16_t* dest, size_t samples)
{
    if (!s_initialized || !s_input_dev || !s_input_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes = samples * sizeof(int16_t);
    esp_err_t err = esp_codec_dev_read(s_input_dev, (void*)dest, bytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_read failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：4 通道独立录音
 * 对应测试 TC-07
 *
 * TDM 模式下 I2S RX 返回 interleaved 4 通道数据：
 * [CH0_s0, CH1_s0, CH2_s0, CH3_s0, CH0_s1, CH1_s1, ...]
 *
 * 由于 esp_codec_dev_read 的 channel_mask 控制输出格式，
 * 当 channel_mask 仅包含 channel 0 时，输出为单声道混合。
 * 要获取原始 4 通道，需重新 open 设备并设置完整 channel_mask。
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_record_multi(int16_t* dest, size_t samples)
{
    if (!s_initialized || !s_input_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * 读取原始 4 通道数据：
     * TDM 每帧 4 samples (MIC1 + MIC2 + MIC3 + MIC4)
     * 总字节数 = samples * 4 channels * sizeof(int16_t)
     */
    size_t total_bytes = samples * 4 * sizeof(int16_t);
    esp_err_t err = esp_codec_dev_read(s_input_dev, (void*)dest, total_bytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "record_multi read failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：音量控制
 * 对应测试 TC-06
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_set_volume(uint8_t level)
{
    if (!s_initialized || !s_output_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    s_output_volume = level;
    esp_err_t err = esp_codec_dev_set_out_vol(s_output_dev, (int)level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_out_vol(%d) failed: %s", level, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Volume set to %d", level);
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：静音控制
 * 对应测试 TC-05
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_set_mute(bool mute)
{
    if (!s_initialized || !s_output_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_codec_dev_set_out_mute(s_output_dev, mute);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_out_mute(%d) failed: %s", mute, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Mute %s", mute ? "ON" : "OFF");
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：输入增益
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_set_input_gain(uint8_t gain_db)
{
    if (!s_initialized || !s_input_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    s_input_gain = gain_db;
    /* 设置通道 0 (MIC1) 的增益 */
    esp_err_t err = esp_codec_dev_set_in_channel_gain(
        s_input_dev,
        ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        (float)gain_db);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_in_channel_gain(%d) failed: %s",
                 gain_db, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Input gain set to %d dB", gain_db);
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：使能/禁用输入
 *
 * 参考 xiaozhi BoxAudioCodec::EnableInput()
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_enable_input(bool enable)
{
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_initialized || !s_input_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (enable == s_input_enabled) {
        return ESP_OK;
    }

    if (enable) {
        /* 打开输入设备：配置采样格式
         *
         * W3: 全开 4 通道 (ch0+ch1+ch2+ch3), 让上层 audio_uplink_task 自行分离:
         *   - ch0 = MIC1 (主麦)
         *   - ch1 = AEC ref (扬声器回采, P4C5_AUDIO_INPUT_REF=true 时由硬件回采)
         *   - ch2 = MIC3 (副麦)
         *   - ch3 = MIC4 (备用麦)
         * 参考: xiaozhi BoxAudioCodec::EnableInput() 也开全部 4 通道
         */
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) |
                            ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1) |
                            ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2) |
                            ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3),
            .sample_rate = (uint32_t)s_output_sample_rate,
            .mclk_multiple = 0,
        };

        esp_err_t err = esp_codec_dev_open(s_input_dev, &fs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "input open failed: %s", esp_err_to_name(err));
            return err;
        }

        /* 设置 MIC1 增益 (默认 30dB，与 xiaozhi 一致) */
        err = esp_codec_dev_set_in_channel_gain(
            s_input_dev,
            ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            (float)s_input_gain);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "set_in_channel_gain failed: %s", esp_err_to_name(err));
            /* 非致命错误，继续 */
        }

        s_input_enabled = true;
        ESP_LOGI(TAG, "Input enabled: 4ch, %dHz, gain=%ddB%s",
                 s_input_sample_rate, s_input_gain,
                 P4C5_AUDIO_INPUT_REF ? " + AEC ref" : "");
    } else {
        esp_err_t err = esp_codec_dev_close(s_input_dev);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "input close failed: %s", esp_err_to_name(err));
            return err;
        }
        s_input_enabled = false;
        ESP_LOGI(TAG, "Input disabled");
    }

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：使能/禁用输出
 *
 * 参考 xiaozhi BoxAudioCodec::EnableOutput()
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_enable_output(bool enable)
{
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_initialized || !s_output_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (enable == s_output_enabled) {
        return ESP_OK;
    }

    if (enable) {
        /* 打开输出设备：单声道 16bit */
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)s_output_sample_rate,
            .mclk_multiple = 0,
        };

        esp_err_t err = esp_codec_dev_open(s_output_dev, &fs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "output open failed: %s", esp_err_to_name(err));
            return err;
        }

        /* 设置初始音量 */
        esp_codec_dev_set_out_vol(s_output_dev, s_output_volume);

        s_output_enabled = true;
        ESP_LOGI(TAG, "Output enabled: mono, %dHz, vol=%d",
                 s_output_sample_rate, s_output_volume);
    } else {
        esp_err_t err = esp_codec_dev_close(s_output_dev);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "output close failed: %s", esp_err_to_name(err));
            return err;
        }
        s_output_enabled = false;
        ESP_LOGI(TAG, "Output disabled");
    }

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：测试音
 * 对应测试 TC-04 (1kHz 正弦波)
 *
 * 生成正弦波：使用 sinf() 浮点运算
 * 24kHz 采样率，16-bit 单声道
 * 自动使能输出 → 播放 → 不关闭输出（保留音量）
 * ────────────────────────────────────────────── */

esp_err_t p4c5_audio_test_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Test tone: %lu Hz, %lu ms",
             (unsigned long)freq_hz, (unsigned long)duration_ms);

    /* 确保输出已使能 */
    bool was_enabled = s_output_enabled;
    if (!s_output_enabled) {
        esp_err_t err = p4c5_audio_enable_output(true);
        if (err != ESP_OK) return err;
    }

    uint32_t sample_rate = (uint32_t)s_output_sample_rate;
    uint32_t total_samples = sample_rate * duration_ms / 1000;
    uint32_t phase_acc = 0;
    /* 相位增量：freq / sample_rate * 2^32 (定点) */
    uint32_t phase_inc = (uint32_t)((double)freq_hz / (double)sample_rate * 4294967296.0);

    /* 分块播放，避免大内存分配 */
    const size_t chunk_samples = 480;  /* 20ms @ 24kHz */
    int16_t* buf = (int16_t*)malloc(chunk_samples * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "test_tone: malloc failed");
        return ESP_ERR_NO_MEM;
    }

    uint32_t remaining = total_samples;
    while (remaining > 0) {
        size_t n = (remaining < chunk_samples) ? remaining : chunk_samples;
        for (size_t i = 0; i < n; i++) {
            /* 定点正弦：phase_acc / 2^32 * 2π */
            float phase = (float)phase_acc / 4294967296.0f * 6.2831853f;
            buf[i] = (int16_t)(sinf(phase) * 30000.0f);  /* ~91% 满幅 */
            phase_acc += phase_inc;
        }

        size_t bytes = n * sizeof(int16_t);
        esp_err_t err = esp_codec_dev_write(s_output_dev, (void*)buf, bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "test_tone write failed: %s", esp_err_to_name(err));
            free(buf);
            return err;
        }
        remaining -= n;
    }

    free(buf);
    ESP_LOGI(TAG, "Test tone complete (%lu samples)", (unsigned long)total_samples);
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：查询初始化状态
 * ────────────────────────────────────────────── */

bool p4c5_audio_is_initialized(void)
{
    return s_initialized;
}
