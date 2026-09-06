/*
 * Audio Mixer - 4 通道 → 单声道 混音组件
 *
 * 用途：M3 语音上行链路中，ES7210 四麦（4ch / 48 kHz / int16）混音为单声道。
 *       后续接到 resampler_48_16 做 48 → 16 kHz 降采样，再到 opus_encoder。
 *
 * ★ 硬件通道布局（M5Stack Tab5 官方 BSP 确认，hal_audio.cpp）:
 *     ch0 = MIC-L   (左麦克风)      ← 取
 *     ch1 = AEC     (扬声器回采参考) ← 跳过（混入会致 RMS 冲顶 / 削波）
 *     ch2 = MIC-R   (右麦克风)      ← 取
 *     ch3 = unused  (未使用)         ← 跳过
 *
 * 算法（仅取 ch0 + ch2 等权平均）:
 *   mix[n] = clamp16((mic0[n] + mic2[n]) / 2);
 *
 *   - int32 累加防溢出（2 × 32767 = 65534，仍在 int32 安全范围）
 *   - 饱和回 int16（防止 overflow 回绕）
 *   - 可选 1 阶 IIR 高通（~100 Hz @ 48 kHz）去除 DC / 低频环境噪声
 *
 * 输入布局：交错 int16 PCM，帧布局 [ch0,ch1,ch2,ch3, ch0,ch1,ch2,ch3, ...]
 *           （物理含义：[MIC-L, AEC, MIC-R, unused] 每帧重复）
 *
 * 性能：
 *   - 960 samples × 4 ch @ 48 kHz / 20ms = 3840 int16 读 + 960 int16 写
 *   - < 0.1 % 单核 CPU（P4 @ 400 MHz）
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 高通滤波器配置
struct HighpassConfig {
    bool    enable;        // 是否启用高通（默认 false，M3 先不开）
    float   cutoff_hz;     // 截止频率 Hz，默认 100 Hz
    float   sample_rate;   // 采样率 Hz（48kHz）
};

// 混音器配置
struct AudioMixerConfig {
    uint8_t        in_channels;    // 输入通道数，必须 = 4（固定）
    uint32_t       sample_rate;    // 采样率 Hz（48000）
    HighpassConfig highpass;       // 高通配置
};

// 默认配置：4ch in, 1ch out, 48 kHz, 无高通
AudioMixerConfig audio_mixer_default_config();

/*
 * 初始化混音器
 *
 * 分配内部状态（高通 IIR 的 x/y 历史）。可在同一进程内多次 init/deinit。
 *
 * @param cfg 混音器配置
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM / ESP_ERR_INVALID_STATE
 */
esp_err_t audio_mixer_init(const AudioMixerConfig &cfg);

/*
 * 反初始化
 */
void audio_mixer_deinit();

/*
 * 查询是否已初始化
 */
bool audio_mixer_is_initialized();

/*
 * 处理一帧 4 通道 → 单声道
 *
 * 输入：in4ch[0..in4ch + samples*4)，帧布局 [c0,c1,c2,c3, c0,c1,c2,c3, ...]
 * 输出：out1ch[0..out1ch + samples)，单声道
 *
 * 输入输出长度约束：
 *   - samples > 0
 *   - 输入缓冲至少 samples * 4 * sizeof(int16_t)
 *   - 输出缓冲至少 samples * sizeof(int16_t)
 *
 * @param in4ch   输入 4 通道交织 PCM (int16)
 * @param out1ch  输出单声道 PCM (int16)
 * @param samples 帧内采样点数（如 48kHz/20ms = 960）
 * @return ESP_OK / ESP_ERR_INVALID_STATE（未 init）/ ESP_ERR_INVALID_ARG
 */
esp_err_t audio_mixer_process(const int16_t *in4ch,
                              int16_t       *out1ch,
                              size_t         samples);

/*
 * 复位内部状态（高通历史等），不释放资源。
 *
 * 调用时机：长静音 / 通道切换 / 流重启时调用，避免旧状态泄漏到新流。
 */
void audio_mixer_reset();

}  // namespace audio
