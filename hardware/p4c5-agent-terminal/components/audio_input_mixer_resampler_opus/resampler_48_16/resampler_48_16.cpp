/*
 * Resampler 48kHz → 16kHz - 实现
 *
 * 用 ESP-DSP dsps_firmr_init_s16 + dsps_firmr_s16_ansi 实现多速率 FIR 抽取。
 * 滤波器设计：
 *   - 64 tap FIR, interp=1, decim=3
 *   - 截止频率 = out_rate/2 = 8 kHz（满足 Nyquist for 16 kHz）
 *   - 归一化截止 = cutoff/(in_rate/2) = 8000 / 24000 = 0.3333
 *   - fir1(N-1, Wn) = fir1(63, 0.3333) with Hamming window
 *   - 系数 Q15 归一化（sum ≈ 32767），shift=0 → final_shift=-15，Q15→int16 标准右移
 *
 * P4 (RISC-V) 走 ANSI C 路径（无 LX7 SIMD）。
 * S3 + CONFIG_DSP_OPTIMIZED 自动选 dsps_firmr_s16_aes3 汇编。
 */

#include "resampler_48_16.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include "esp_log.h"

// ESP-DSP 多速率 FIR s16 API
#include "dsps_fir.h"

static const char *TAG = "resamp_48_16";

namespace audio {

// ── 模块状态 ──
static struct {
    bool                 initialized;
    ResamplerConfig      cfg;
    ResamplerFrameSize   frame;

    fir_s16_t            fir;            // ESP-DSP 滤波器状态

    // 我们的 LPF 系数（运行时生成，Q15 归一化）
    int16_t             *coeffs;

    // 我们自己分配的 delay line（也可传 NULL 让 ESP-DSP 自己 malloc，
    // 但我们手动管理便于 deinit 释放和 reset）
    int16_t             *delay;
} s_state = {};

ResamplerConfig resampler_48_16_default_config()
{
    ResamplerConfig cfg;
    cfg.in_rate          = 48000;
    cfg.out_rate         = 16000;
    cfg.interp           = 1;
    cfg.decim            = 3;        // 48000 / 16000 = 3
    cfg.filter_taps      = 64;
    cfg.filter_cutoff_hz = 8000;
    cfg.shift            = 0;        // Q15: shift=0 → final_shift=-15，acc>>15 实现 unity gain
    cfg.start_pos        = 0;
    return cfg;
}

/*
 * 生成 64 tap LPF 系数（fir1(63, cutoff_norm) + Hamming 窗）
 * 归一化到 Q15 整数（sum ≈ 32767），保证输出与输入同量级。
 *
 * 注意：这里系数是低通原型（未做 interp/decim 重排），
 * ESP-DSP 的 dsps_firmr_s16_ansi 在内部按 interp/decim 步长访问。
 *
 * cutoff_norm = cutoff_hz / (in_rate / 2)，范围 (0, 1)
 *   例如 8000 / 24000 = 0.3333
 */
static void generate_lpf_coeffs(int16_t *coeffs, int taps,
                                 double cutoff_norm, double sample_scale)
{
    // fir1 / sinc 低通原型：h[n] = 2 * fc * sinc(2 * fc * (n - M))
    //   其中 M = (N-1)/2
    // 然后乘 Hamming 窗：w[n] = 0.54 - 0.46 * cos(2πn / (N-1))
    //
    // 用 fir1(63, 0.3333) 标准做法 — MATLAB/Python scipy.signal.firwin(64, 0.3333)
    // 这里用经典 sinc + Hamming 重现。

    const double M = (double)(taps - 1) / 2.0;
    double sum = 0.0;

    // 先算未归一化系数
    double *h = (double *)malloc(sizeof(double) * taps);
    if (!h) {
        ESP_LOGE(TAG, "malloc failed for h[]");
        return;
    }

    for (int n = 0; n < taps; ++n) {
        double k = (double)n - M;
        // sinc(x) = sin(πx) / (πx)，x = 0 时取 1
        double sinc_arg = 2.0 * cutoff_norm * k;
        double sinc_val;
        if (fabs(sinc_arg) < 1e-12) {
            sinc_val = 1.0;
        } else {
            sinc_val = sin(M_PI * sinc_arg) / (M_PI * sinc_arg);
        }
        double lp = 2.0 * cutoff_norm * sinc_val;

        // Hamming 窗
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * (double)n / (double)(taps - 1));

        h[n] = lp * w;
        sum += h[n];
    }

    // 归一化到 [sum * sample_scale]，Q15 量化
    double scale = sample_scale / sum;
    for (int n = 0; n < taps; ++n) {
        double v = h[n] * scale;
        // 饱和
        if (v >  32767.0) v =  32767.0;
        if (v < -32768.0) v = -32768.0;
        coeffs[n] = (int16_t)lround(v);
    }

    free(h);
}

esp_err_t resampler_48_16_init(const ResamplerConfig   &cfg,
                               const ResamplerFrameSize &frame_size)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized, deinit first");
        return ESP_ERR_INVALID_STATE;
    }
    if (cfg.in_rate == 0 || cfg.out_rate == 0 || cfg.interp == 0 || cfg.decim == 0) {
        ESP_LOGE(TAG, "invalid cfg (in_rate=%lu out_rate=%lu interp=%u decim=%u)",
                 (unsigned long)cfg.in_rate, (unsigned long)cfg.out_rate,
                 cfg.interp, cfg.decim);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.filter_taps < 4 || cfg.filter_taps > 1024) {
        ESP_LOGE(TAG, "invalid filter_taps: %u (4..1024)", cfg.filter_taps);
        return ESP_ERR_INVALID_ARG;
    }
    if (frame_size.in_samples == 0 || frame_size.out_samples == 0) {
        ESP_LOGE(TAG, "invalid frame_size (in=%zu out=%zu)",
                 frame_size.in_samples, frame_size.out_samples);
        return ESP_ERR_INVALID_ARG;
    }
    // 期望输出 = in / decim（理想）— 给个软警告
    size_t expected_out = frame_size.in_samples * cfg.interp / cfg.decim;
    if (frame_size.out_samples < expected_out) {
        ESP_LOGE(TAG, "out_samples=%zu < expected=%zu (in=%zu, interp=%u, decim=%u)",
                 frame_size.out_samples, expected_out,
                 frame_size.in_samples, cfg.interp, cfg.decim);
        return ESP_ERR_INVALID_ARG;
    }
    // 整除性（interp/decim 关系）
    if ((cfg.in_rate * cfg.interp) != (cfg.out_rate * cfg.decim)) {
        ESP_LOGW(TAG, "non-integer ratio: in*interp (%lu) != out*decim (%lu)",
                 (unsigned long)(cfg.in_rate * cfg.interp),
                 (unsigned long)(cfg.out_rate * cfg.decim));
    }

    // 分配 coeffs 和 delay
    int16_t *coeffs = (int16_t *)malloc(cfg.filter_taps * sizeof(int16_t));
    if (!coeffs) {
        ESP_LOGE(TAG, "malloc coeffs failed");
        return ESP_ERR_NO_MEM;
    }
    // delay line 长度 = coeffs_len / interp （ESP-DSP dsps_firmr_init_s16 要求）
    size_t delay_len = (size_t)cfg.filter_taps / cfg.interp;
    int16_t *delay = (int16_t *)calloc(delay_len + 4, sizeof(int16_t));
    if (!delay) {
        ESP_LOGE(TAG, "malloc delay failed");
        free(coeffs);
        return ESP_ERR_NO_MEM;
    }

    // 生成 LPF 系数
    double cutoff_norm = (double)cfg.filter_cutoff_hz / ((double)cfg.in_rate / 2.0);
    if (cutoff_norm <= 0.0 || cutoff_norm >= 1.0) {
        ESP_LOGE(TAG, "invalid cutoff_norm=%.4f", cutoff_norm);
        free(coeffs);
        free(delay);
        return ESP_ERR_INVALID_ARG;
    }
    // 归一化目标：让 sum(coeffs) = sample_scale
    //   shift=0 时 final_shift=-15，输出 ≈ acc >> 15（Q15→int16 标准右移，unity gain）
    //   滤波器峰值 ≈ 1.0（DC 处），所以 sample_scale = 32767（Q15 峰值）
    double sample_scale = 32767.0;
    generate_lpf_coeffs(coeffs, cfg.filter_taps, cutoff_norm, sample_scale);

    ESP_LOGI(TAG, "coeffs[0..3]: %d %d %d %d ... %d (taps=%u, fc=%.0f Hz, fs=%lu)",
             coeffs[0], coeffs[1], coeffs[2], coeffs[3],
             coeffs[cfg.filter_taps - 1],
             cfg.filter_taps, (double)cfg.filter_cutoff_hz,
             (unsigned long)cfg.in_rate);

    // 调 ESP-DSP 初始化
    esp_err_t err = dsps_firmr_init_s16(
        &s_state.fir,
        coeffs,
        delay,
        (int16_t)cfg.filter_taps,
        (int16_t)cfg.interp,
        (int16_t)cfg.decim,
        (int16_t)cfg.start_pos,
        (int16_t)cfg.shift);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dsps_firmr_init_s16 failed: %s", esp_err_to_name(err));
        free(coeffs);
        free(delay);
        return err;
    }

    s_state.cfg       = cfg;
    s_state.frame     = frame_size;
    s_state.coeffs    = coeffs;
    s_state.delay     = delay;
    s_state.initialized = true;

    ESP_LOGI(TAG, "initialized: %lu→%lu Hz, interp=%u decim=%u, taps=%u, "
                  "%zu→%zu samples/frame",
             (unsigned long)cfg.in_rate, (unsigned long)cfg.out_rate,
             cfg.interp, cfg.decim, cfg.filter_taps,
             frame_size.in_samples, frame_size.out_samples);
    return ESP_OK;
}

void resampler_48_16_deinit()
{
    if (!s_state.initialized) {
        return;
    }
    if (s_state.coeffs) {
        free(s_state.coeffs);
        s_state.coeffs = nullptr;
    }
    if (s_state.delay) {
        free(s_state.delay);
        s_state.delay = nullptr;
    }
    memset(&s_state.fir, 0, sizeof(s_state.fir));
    s_state.initialized = false;
    ESP_LOGI(TAG, "deinitialized");
}

bool resampler_48_16_is_initialized()
{
    return s_state.initialized;
}

void resampler_48_16_reset()
{
    if (!s_state.initialized) {
        return;
    }
    // 重置 delay line 与 decim 计数器到初值
    size_t delay_len = (size_t)s_state.cfg.filter_taps / s_state.cfg.interp;
    memset(s_state.fir.delay, 0, (delay_len + 4) * sizeof(int16_t));
    s_state.fir.pos       = 0;
    s_state.fir.d_pos     = s_state.cfg.start_pos;
    s_state.fir.start_pos = s_state.cfg.start_pos;
    s_state.fir.interp_pos = 0;
}

ResamplerFrameSize resampler_48_16_get_frame_size()
{
    return s_state.frame;
}

esp_err_t resampler_48_16_process(const int16_t *in,
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

    int32_t produced = dsps_firmr_s16_ansi(&s_state.fir, in, out,
                                            (int32_t)s_state.frame.in_samples);
    if (produced < 0) {
        ESP_LOGE(TAG, "dsps_firmr_s16_ansi failed: %ld", (long)produced);
        return ESP_FAIL;
    }
    if ((size_t)produced != s_state.frame.out_samples) {
        ESP_LOGW(TAG, "produced=%ld, expected=%zu (in=%zu, decim=%u)",
                 (long)produced, s_state.frame.out_samples,
                 s_state.frame.in_samples, s_state.cfg.decim);
    }
    *out_len = (size_t)produced;
    return ESP_OK;
}

}  // namespace audio
