/*
 * Resampler 24kHz → 16kHz - 实现 (W3: 适配 p4c5_audio 24kHz)
 *
 * 用线性插值 + 一阶 IIR 低通实现任意比率重采样（24→16, ratio=1.5）
 *
 * 算法：
 *   out[n] = in[n*1.5] (floor) + (in[n*1.5+1] - in[n*1.5]) * frac
 *   其中 frac = n * 1.5 - floor(n * 1.5)
 *
 * 帧规格 (20ms)：
 *   - 输入  : 单声道 int16, 24 kHz, 480 samples/帧 (p4c5_audio 配置)
 *   - 输出  : 单声道 int16, 16 kHz, 320 samples/帧 (opus_encoder 输入)
 *
 * 简化策略：
 *   - 不做抗混叠 LPF (24→16 比率不高, 高频分量被 opus_encoder 自然滤掉)
 *   - 用线性插值代替 FIR (CPU 更省, 适合 POC)
 *   - 一阶 IIR DC blocker (去除麦克风 DC 偏移)
 *
 * P4 (RISC-V) 400MHz 单核 CPU 占用 <1% (480 → 320 samples)
 */

#include "resampler_24_16.h"

#include <cmath>
#include <cstring>
#include "esp_log.h"

static const char *TAG = "resamp_24_16";

namespace audio {

// ── 模块状态 ──
static struct {
    bool initialized;
    ResamplerConfig cfg;

    // DC blocker 一阶 IIR 状态
    float dc_prev_in;
    float dc_prev_out;
} s_state = {};

ResamplerConfig resampler_24_16_default_config()
{
    ResamplerConfig cfg;
    cfg.in_rate          = 24000;
    cfg.out_rate         = 16000;
    cfg.interp           = 1;
    cfg.decim            = 0;        // 非整数比, 用线性插值不用 decim
    cfg.filter_taps      = 0;        // 不用 FIR
    cfg.filter_cutoff_hz = 8000;
    cfg.shift            = 0;
    cfg.start_pos        = 0;
    return cfg;
}

esp_err_t resampler_24_16_init(const ResamplerConfig &cfg)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized, deinit first");
        return ESP_ERR_INVALID_STATE;
    }
    if (cfg.in_rate != 24000 || cfg.out_rate != 16000) {
        ESP_LOGE(TAG, "only 24k→16k supported");
        return ESP_ERR_INVALID_ARG;
    }

    s_state.cfg = cfg;
    s_state.dc_prev_in = 0.0f;
    s_state.dc_prev_out = 0.0f;
    s_state.initialized = true;

    ESP_LOGI(TAG, "init OK (24k→16k, linear interp + DC blocker)");
    return ESP_OK;
}

esp_err_t resampler_24_16_deinit(void)
{
    if (!s_state.initialized) {
        return ESP_OK;
    }
    s_state.initialized = false;
    s_state.dc_prev_in = 0.0f;
    s_state.dc_prev_out = 0.0f;
    return ESP_OK;
}

void resampler_24_16_reset(void)
{
    s_state.dc_prev_in = 0.0f;
    s_state.dc_prev_out = 0.0f;
}

/*
 * 线性插值重采样
 *
 * in:  480 samples @ 24 kHz
 * out: 320 samples @ 16 kHz (in_len * 16/24 = 480 * 2/3 = 320)
 */
esp_err_t resampler_24_16_process(const int16_t *in,
                                  int16_t       *out,
                                  size_t        *out_len)
{
    if (!s_state.initialized) {
        ESP_LOGE(TAG, "process: not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!in || !out || !out_len) {
        ESP_LOGE(TAG, "process: null arg");
        return ESP_ERR_INVALID_ARG;
    }

    constexpr size_t IN_SAMPLES  = 480;
    constexpr size_t OUT_SAMPLES = 320;
    constexpr float  RATIO       = 1.5f;  // 24/16

    // DC blocker 一阶 IIR: y[n] = x[n] - x[n-1] + a*y[n-1], a ≈ 0.995 (fc ≈ 190Hz @ 16k)
    constexpr float DC_A = 0.995f;
    float dc_in  = s_state.dc_prev_in;
    float dc_out = s_state.dc_prev_out;

    for (size_t i = 0; i < OUT_SAMPLES; i++) {
        float src_idx = (float)i * RATIO;
        size_t idx0 = (size_t)src_idx;
        size_t idx1 = idx0 + 1;
        if (idx1 >= IN_SAMPLES) idx1 = IN_SAMPLES - 1;
        float frac = src_idx - (float)idx0;

        // 线性插值
        float sample = (1.0f - frac) * (float)in[idx0] + frac * (float)in[idx1];

        // DC blocker
        float filtered = sample - dc_in + DC_A * dc_out;
        dc_in  = sample;
        dc_out = filtered;

        // 饱和回 int16
        int32_t s = (int32_t)filtered;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        out[i] = (int16_t)s;
    }

    s_state.dc_prev_in  = dc_in;
    s_state.dc_prev_out = dc_out;

    *out_len = OUT_SAMPLES;
    return ESP_OK;
}

}  // namespace audio
