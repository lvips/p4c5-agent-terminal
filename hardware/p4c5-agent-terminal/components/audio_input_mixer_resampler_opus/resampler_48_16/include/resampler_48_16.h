/*
 * Resampler 48kHz → 16kHz - 多速率 FIR 抽取组件
 *
 * 用途：M3 语音上行链路中，48 kHz 单声道 PCM → 16 kHz 单声道 PCM（decim=3）。
 *       跟在 audio_mixer 后面，给 opus_encoder 喂数据。
 *
 * 实现：ESP-DSP dsps_firmr_s16_ansi（多速率 FIR 抽取）
 *   - 64 tap LPF（截止 8 kHz @ 48 kHz，汉宁窗）
 *   - decim = 3, interp = 1（一级 1/3 抽取）
 *   - 群延迟 32 samples ≈ 0.67 ms @ 48 kHz
 *
 * 帧规格（20 ms）：
 *   - 输入  ：单声道 int16, 48 kHz, 960 samples/帧
 *   - 输出  ：单声道 int16, 16 kHz, 320 samples/帧
 *
 * 注意：
 *   - 该 API 内部保留延迟线状态（fir->delay），多次调用 process 时状态连续。
 *   - 流切换/长静音时请调用 resampler_48_16_reset() 复位状态。
 *   - 移植到 ESP32-P4（RISC-V）：走 ANSI C 实现（无 LX7 / NEON SIMD 加速），
 *     但 64 tap × 960 samples × 50 帧/s ≈ 3M MAC/s，对 400 MHz RISC-V < 1% 单核负载。
 *   - 若在 ESP32-S3 上打开 CONFIG_DSP_OPTIMIZED，将自动选择 dsps_firmr_s16_aes3 汇编路径。
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 多速率 FIR 配置
struct ResamplerConfig {
    uint32_t in_rate;        // 输入采样率（默认 48000）
    uint32_t out_rate;       // 输出采样率（默认 16000）
    uint16_t interp;         // 插值因子（默认 1）
    uint16_t decim;          // 抽取因子（默认 3，48000/16000 = 3）
    uint16_t filter_taps;    // LPF 长度（默认 64）
    uint16_t filter_cutoff_hz; // LPF 截止频率 Hz（默认 8000）
    int16_t  shift;          // 输出 shift（默认 0）
    uint16_t start_pos;      // decim 计数器初值（默认 0）
};

// 默认配置（48 → 16 kHz，64 tap，8 kHz 截止）
ResamplerConfig resampler_48_16_default_config();

// 便捷预设：固定 48k→16k、960→320 samples/帧
struct ResamplerFrameSize {
    size_t in_samples;     // 每帧输入采样数（默认 960）
    size_t out_samples;    // 每帧输出采样数（默认 320）
};

/*
 * 初始化降采样器
 *
 * 内部分配 coeffs/delay buffer（coeffs 由代码生成，delay 由 ESP-DSP 接管），
 * 并调用 dsps_firmr_init_s16 配置滤波器。
 *
 * @param cfg         滤波器配置
 * @param frame_size  帧大小（输入/输出每帧采样数）
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM / ESP_ERR_INVALID_STATE
 */
esp_err_t resampler_48_16_init(const ResamplerConfig   &cfg,
                               const ResamplerFrameSize &frame_size);

/*
 * 反初始化
 */
void resampler_48_16_deinit();

/*
 * 查询是否已初始化
 */
bool resampler_48_16_is_initialized();

/*
 * 处理一帧（48 kHz mono → 16 kHz mono）
 *
 * @param in        输入 PCM（int16，frame_size.in_samples 个采样点）
 * @param out       输出 PCM（int16，至少 frame_size.out_samples 个采样点容量）
 * @param out_len   [out] 实际写入 out 的采样点数（= frame_size.out_samples）
 * @return ESP_OK / ESP_ERR_INVALID_STATE / ESP_ERR_INVALID_ARG
 */
esp_err_t resampler_48_16_process(const int16_t *in,
                                  int16_t       *out,
                                  size_t        *out_len);

/*
 * 复位内部状态（延迟线 / decim 计数器）
 *
 * 调用时机：流切换、长静音后、首次启动时显式调用确保状态干净。
 */
void resampler_48_16_reset();

/*
 * 查询当前帧大小（方便调用方准备缓冲区）
 */
ResamplerFrameSize resampler_48_16_get_frame_size();

}  // namespace audio
