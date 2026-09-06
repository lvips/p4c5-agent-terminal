/*
 * Audio Mixer - 实现
 *
 * ES7210 TDM 4 通道 int16 PCM → 单声道 int16 PCM
 *
 * ★ 硬件通道布局（M5Stack Tab5 官方 BSP 确认）:
 *     ch0 = MIC-L   (左麦克风)
 *     ch1 = AEC     (扬声器回采参考 — 不参与混音，否则扬声器信号削波)
 *     ch2 = MIC-R   (右麦克风)
 *     ch3 = unused  (未使用)
 *
 * 算法（仅取 ch0 + ch2 等权平均）:
 *   avg[n] = (in[4n+0] + in[4n+2]) / 2
 *   out[n] = clamp16(avg[n])
 *
 *   - int32 累加防溢出（2 × ±32767 = ±65534，远在 int32 安全范围）
 *   - 算术右移 1 位 = 除 2
 *   - 饱和回 int16（防御性）
 *
 * 可选 1 阶 IIR 高通（DC 阻塞器）：
 *   差分方程：
 *     y[n] = a * (y[n-1] + x[n] - x[n-1])
 *   其中 a ≈ RC / (RC + dt)，fc = 1 / (2π·RC)，dt = 1/fs
 *   对 fc=100 Hz, fs=48 kHz：
 *     RC = 1 / (2π·100) = 1.5915e-3 s
 *     a  = RC / (RC + dt) = (1.5915e-3) / (1.5915e-3 + 2.0833e-5) ≈ 0.9871
 *
 *   该 1 阶 IIR 高通是经典 DC blocker 实现（差分 + 单极 IIR），
 *   群延迟小 (~0.5 sample)，CPU 几乎为零。
 */

#include "audio_mixer.h"

#include <cmath>
#include <cstring>
#include "esp_log.h"

static const char *TAG = "audio_mixer";

namespace audio {

// ── 模块状态 ──
static struct {
    bool             initialized;
    AudioMixerConfig cfg;

    // 高通 IIR 历史
    float hp_x_prev;
    float hp_y_prev;
    float hp_a;          // 1 阶 IIR 系数 a ≈ RC/(RC+dt)
} s_state = {};

AudioMixerConfig audio_mixer_default_config()
{
    AudioMixerConfig cfg;
    cfg.in_channels       = 4;
    cfg.sample_rate       = 48000;
    cfg.highpass.enable   = false;
    cfg.highpass.cutoff_hz = 100.0f;
    cfg.highpass.sample_rate = 48000.0f;
    return cfg;
}

esp_err_t audio_mixer_init(const AudioMixerConfig &cfg)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized, deinit first");
        return ESP_ERR_INVALID_STATE;
    }
    if (cfg.in_channels != 4) {
        ESP_LOGE(TAG, "invalid in_channels: %u (only 4 supported)", cfg.in_channels);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.sample_rate == 0 || cfg.sample_rate > 192000) {
        ESP_LOGE(TAG, "invalid sample_rate: %lu", (unsigned long)cfg.sample_rate);
        return ESP_ERR_INVALID_ARG;
    }

    s_state.cfg       = cfg;
    s_state.hp_x_prev = 0.0f;
    s_state.hp_y_prev = 0.0f;

    if (cfg.highpass.enable) {
        // 计算 1 阶 IIR DC blocker 系数
        //   fc = 1/(2π·RC),  a = RC/(RC+dt),  dt = 1/fs
        float fc  = (cfg.highpass.cutoff_hz > 0.0f)
                        ? cfg.highpass.cutoff_hz : 100.0f;
        float fs  = (cfg.highpass.sample_rate > 0.0f)
                        ? cfg.highpass.sample_rate : (float)cfg.sample_rate;
        float RC  = 1.0f / (2.0f * (float)M_PI * fc);
        float dt  = 1.0f / fs;
        s_state.hp_a = RC / (RC + dt);
        ESP_LOGI(TAG, "highpass enabled: fc=%.1f Hz, fs=%.0f Hz, a=%.5f",
                 fc, fs, s_state.hp_a);
    } else {
        s_state.hp_a = 0.0f;
    }

    s_state.initialized = true;
    ESP_LOGI(TAG, "initialized: %uch → 1ch, %lu Hz, hp=%s",
             cfg.in_channels, (unsigned long)cfg.sample_rate,
             cfg.highpass.enable ? "on" : "off");
    return ESP_OK;
}

void audio_mixer_deinit()
{
    if (!s_state.initialized) {
        return;
    }
    s_state.initialized = false;
    s_state.hp_x_prev   = 0.0f;
    s_state.hp_y_prev   = 0.0f;
    ESP_LOGI(TAG, "deinitialized");
}

bool audio_mixer_is_initialized()
{
    return s_state.initialized;
}

void audio_mixer_reset()
{
    s_state.hp_x_prev = 0.0f;
    s_state.hp_y_prev = 0.0f;
}

esp_err_t audio_mixer_process(const int16_t *in4ch,
                              int16_t       *out1ch,
                              size_t         samples)
{
    if (!s_state.initialized) {
        ESP_LOGE(TAG, "process: not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!in4ch || !out1ch || samples == 0) {
        ESP_LOGE(TAG, "process: invalid arg (in=%p out=%p samples=%zu)",
                 (const void *)in4ch, (void *)out1ch, samples);
        return ESP_ERR_INVALID_ARG;
    }

    // 局部缓存状态指针，减少结构体访问开销（编译器通常会自动优化）
    const float hp_a       = s_state.hp_a;
    const bool  hp_enabled = s_state.cfg.highpass.enable;
    float       hp_x_prev  = s_state.hp_x_prev;
    float       hp_y_prev  = s_state.hp_y_prev;

    for (size_t n = 0; n < samples; ++n) {
        // ★ 通道布局（M5Stack Tab5 官方 BSP 确认）:
        //   ch0 = MIC-L（左麦克风）
        //   ch1 = AEC  （扬声器回采参考 — 跳过，不参与混音）
        //   ch2 = MIC-R（右麦克风）
        //   ch3 = unused（未使用）
        //
        // 只取 MIC-L + MIC-R 等权平均: (s0 + s2) / 2
        // 跳过 ch1(AEC) 避免扬声器信号被混入导致 RMS 冲顶/削波
        int32_t s0 = (int32_t)in4ch[4 * n + 0];  // MIC-L
        int32_t s2 = (int32_t)in4ch[4 * n + 2];  // MIC-R

        // int32 累加（2 × ±32767 = ±65534，远在 int32 安全范围）
        int32_t sum = s0 + s2;

        // 等权平均（算术右移 1 位 = 除 2）
        // 实际安全范围：±65534/2 = ±32767，正好在 int16 范围
        // 仍做饱和防御
        int32_t avg = sum >> 1;

        // 饱和到 int16
        int16_t mixed;
        if (avg > 32767) {
            mixed = 32767;
        } else if (avg < -32768) {
            mixed = -32768;
        } else {
            mixed = (int16_t)avg;
        }

        // 可选高通
        if (hp_enabled) {
            float x  = (float)mixed;
            float y  = hp_a * (hp_y_prev + x - hp_x_prev);
            hp_x_prev = x;
            hp_y_prev = y;

            // 饱和回 int16
            int32_t yi = (int32_t)lroundf(y);
            if (yi > 32767) {
                mixed = 32767;
            } else if (yi < -32768) {
                mixed = -32768;
            } else {
                mixed = (int16_t)yi;
            }
        }

        out1ch[n] = mixed;
    }

    // 写回状态
    s_state.hp_x_prev = hp_x_prev;
    s_state.hp_y_prev = hp_y_prev;

    return ESP_OK;
}

}  // namespace audio
