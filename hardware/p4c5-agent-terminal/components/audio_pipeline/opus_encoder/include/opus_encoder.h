/*
 * OPUS Encoder - 语音编码组件
 *
 * 封装 libopus（via 78/esp-opus），将 PCM 音频编码为 OPUS 帧
 * 用于 M3 语音上行：audio_mixer → resampler_48_16 → opus_encoder → dsh_client (WS binary)
 *
 * 配置：
 *   - OPUS_APPLICATION_VOIP（语音优化模式）
 *   - 16 kbps 码率
 *   - 20 ms 帧（16kHz 下 = 320 samples/帧）
 *   - complexity = 5（嵌入式常用档位，照调研报告 3.2 节）
 *
 * 注意：本组件只做编码，不做降采样/声道转换。
 * 调用方需保证输入 PCM 的采样率/声道数与 init 参数一致。
 *   - 默认配置为 16 kHz 单声道 — 与 resampler_48_16 输出对齐
 *   - 走 OPUS_SET_COMPLEXITY(5) + OPUS_SIGNAL_VOICE + FEC + PLR=2%
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 编码器配置
struct OpusEncoderConfig {
    uint32_t sample_rate;   // 采样率 (Hz)，默认 16000（wideband，配合 resampler_48_16）
    uint8_t  channels;      // 声道数，1 = 单声道（语音场景推荐）
    uint32_t bitrate;       // 码率 (bps)，16000 = 16 kbps
    uint32_t frame_ms;      // 帧时长 (ms)，20 = 20ms/帧
    int      complexity;    // libopus 复杂度 (0..10)，默认 5（嵌入式推荐档）
};

// 默认配置：16kHz, 单声道, 16kbps, 20ms, complexity=5
inline OpusEncoderConfig opus_encoder_default_config() {
    return OpusEncoderConfig{
        .sample_rate = 16000,
        .channels    = 1,
        .bitrate     = 16000,
        .frame_ms    = 20,
        .complexity  = 5,
    };
}

/*
 * 初始化 OPUS 编码器
 *
 * 内部创建 OpusEncoder 实例，配置 VOIP 模式 + 目标码率。
 * 必须在 encode 之前调用，且每次 init 必须有对应 deinit。
 *
 * @param cfg  编码器配置（可用 opus_encoder_default_config()）
 * @return ESP_OK 成功, ESP_ERR_NO_MEM 内存不足, ESP_FAIL 初始化失败
 */
esp_err_t opus_encoder_init(const OpusEncoderConfig &cfg);

/*
 * 编码一帧 PCM → OPUS
 *
 * 输入：PCM 16-bit signed，samples 数量必须等于
 *       sample_rate * frame_ms / 1000 （默认 20ms@16kHz = 320 samples）
 *
 * 输出：OPUS 压缩帧写入 out，实际字节数写入 *out_len
 *       out 缓冲区建议至少 max_packet_size 字节
 *       （OPUS 限制：单帧最大 4000 字节，推荐 1276 字节够用 16kbps）
 *
 * @param pcm       输入 PCM 缓冲区（int16_t，samples 个采样点）
 * @param samples   输入采样点数（必须与配置的帧长匹配）
 * @param out       输出 OPUS 帧缓冲区
 * @param out_len   [in/out] in=缓冲区大小, out=实际编码字节数
 * @return ESP_OK 成功, ESP_ERR_INVALID_ARG 参数错误, ESP_ERR_INVALID_SIZE 帧长不匹配
 */
esp_err_t opus_encoder_encode(const int16_t *pcm, size_t samples,
                               uint8_t *out, size_t *out_len);

/*
 * 反初始化 OPUS 编码器
 *
 * 释放内部 OpusEncoder 实例，之后可重新 init。
 */
void opus_encoder_deinit();

/*
 * 查询：编码器是否已初始化
 */
bool opus_encoder_is_initialized();

/*
 * 查询：每帧需要的 PCM 采样点数
 *
 * 例如：16kHz + 20ms → 返回 320
 * 调用方用此值准备 pcm 缓冲区、决定每次 encode 的 samples 参数。
 */
size_t opus_encoder_get_frame_samples();

/*
 * 查询：推荐的最大输出包大小（字节）
 *
 * 16kbps + 20ms 典型输出 ~40 字节，留余量返回 1276 字节
 * （OPUS RFC 6716 限制单帧最大 4000 字节）
 */
size_t opus_encoder_get_max_packet_size();

}  // namespace audio
