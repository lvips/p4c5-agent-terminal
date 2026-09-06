/*
 * 软件 AEC 回声消除 - 实现 (W3)
 *
 * 算法: NLMS (Normalized Least Mean Squares) 自适应滤波
 *
 * 原理:
 *   - 用 ref (扬声器回采) 通过 FIR 自适应滤波器估计 echo
 *   - error = mic - estimated_echo
 *   - 更新滤波器权重: w[n+1] = w[n] + mu * error * ref / (||ref||^2 + eps)
 *   - 输出 error 即为 AEC 后的近端语音
 *
 * 简化 (POC 阶段):
 *   - 滤波器长度固定 (configurable, 默认 128)
 *   - 学习步长固定 (默认 0.005)
 *   - 用泄漏因子防止发散
 *   - 不做残余回声抑制 (留给后续 spectral subtraction)
 *
 * 参考开源案例:
 *   - xiaozhi ESP-SR AFE (SR_LOW_COST): 仅 ESP32-S3 硬件加速
 *   - speexdsp AEC: 适合嵌入式, 但需额外 ~50KB ROM
 *   - WebRTC APM: 最佳但 ~200KB RAM, P4 不适合
 *   - 自实现 NLMS: 最小, ~16KB RAM (本次实现)
 */

#include "aec_sw.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include "esp_log.h"

static const char *TAG = "aec_sw";

namespace audio {

// ── 模块状态 ──
static struct {
    bool initialized;
    AecConfig cfg;

    // NLMS 滤波器权重 (FIR taps)
    float *weights;
    // 参考信号历史 (滑动窗口)
    float *ref_history;
} s_state = {};

AecConfig aec_sw_default_config()
{
    AecConfig cfg;
    cfg.sample_rate     = 24000;
    cfg.filter_taps     = 128;
    cfg.step_size       = 0.005f;
    cfg.leakage         = 0.999f;
    cfg.ref_gain        = 1.0f;
    cfg.enable_aec      = true;
    cfg.enable_residual = false;
    return cfg;
}

esp_err_t aec_sw_init(const AecConfig &cfg)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized, deinit first");
        return ESP_ERR_INVALID_STATE;
    }
    if (cfg.filter_taps < 8 || cfg.filter_taps > 1024) {
        ESP_LOGE(TAG, "filter_taps out of range [8, 1024]: %u", cfg.filter_taps);
        return ESP_ERR_INVALID_ARG;
    }

    s_state.cfg = cfg;
    s_state.weights = (float *)calloc(cfg.filter_taps, sizeof(float));
    s_state.ref_history = (float *)calloc(cfg.filter_taps, sizeof(float));
    if (!s_state.weights || !s_state.ref_history) {
        ESP_LOGE(TAG, "calloc failed (%u taps × 8 bytes)",
                 cfg.filter_taps);
        free(s_state.weights);
        free(s_state.ref_history);
        s_state.weights = nullptr;
        s_state.ref_history = nullptr;
        return ESP_ERR_NO_MEM;
    }

    s_state.initialized = true;
    ESP_LOGI(TAG, "init OK (taps=%u mu=%.4f leak=%.4f aec=%d)",
             cfg.filter_taps, cfg.step_size, cfg.leakage,
             (int)cfg.enable_aec);
    return ESP_OK;
}

esp_err_t aec_sw_deinit(void)
{
    if (!s_state.initialized) return ESP_OK;
    free(s_state.weights);
    free(s_state.ref_history);
    s_state.weights = nullptr;
    s_state.ref_history = nullptr;
    s_state.initialized = false;
    return ESP_OK;
}

void aec_sw_reset(void)
{
    if (s_state.initialized && s_state.weights && s_state.ref_history) {
        memset(s_state.weights, 0, s_state.cfg.filter_taps * sizeof(float));
        memset(s_state.ref_history, 0, s_state.cfg.filter_taps * sizeof(float));
    }
}

esp_err_t aec_sw_process(const int16_t *mic,
                         const int16_t *ref,
                         int16_t       *out,
                         size_t         samples)
{
    if (!s_state.initialized) return ESP_ERR_INVALID_STATE;
    if (!mic || !ref || !out || samples == 0) return ESP_ERR_INVALID_ARG;

    const uint16_t N = s_state.cfg.filter_taps;
    const float    mu = s_state.cfg.step_size;
    const float    leak = s_state.cfg.leakage;
    const float    ref_gain = s_state.cfg.ref_gain;
    const bool     aec_on = s_state.cfg.enable_aec;

    if (!aec_on) {
        /* AEC 关闭: 直接拷贝 mic 到 out */
        memcpy(out, mic, samples * sizeof(int16_t));
        return ESP_OK;
    }

    float *w = s_state.weights;
    float *x = s_state.ref_history;

    for (size_t n = 0; n < samples; n++) {
        /* 1. 滑动参考窗口: x[0] = 当前样本, x[1..N-1] = 历史 */
        memmove(&x[1], &x[0], (N - 1) * sizeof(float));
        x[0] = (float)ref[n] * ref_gain;

        /* 2. 用 FIR 滤波器估计回声: y = Σ w[k] * x[k] */
        float echo_est = 0.0f;
        for (uint16_t k = 0; k < N; k++) {
            echo_est += w[k] * x[k];
        }

        /* 3. 误差 = mic - echo_est (消除后的近端语音) */
        float error = (float)mic[n] - echo_est;

        /* 4. NLMS 更新: w[k] = leak * w[k] + mu * error * x[k] / (||x||^2 + eps) */
        float x_power = 0.0f;
        for (uint16_t k = 0; k < N; k++) {
            x_power += x[k] * x[k];
        }
        x_power += 1e-6f;  // 防除零
        float norm_factor = mu * error / x_power;

        for (uint16_t k = 0; k < N; k++) {
            w[k] = leak * w[k] + norm_factor * x[k];
        }

        /* 5. 饱和回 int16 */
        int32_t s = (int32_t)error;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        out[n] = (int16_t)s;
    }

    return ESP_OK;
}

}  // namespace audio