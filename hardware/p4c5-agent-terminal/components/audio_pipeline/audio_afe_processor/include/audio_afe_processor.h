/*
 * Audio AFE Processor — ESP-SR AFE 封装 (P1)
 *
 * 替换自实现 NLMS AEC + 24→16 重采样 + RMS VAD, 用 ESP-SR AFE 一体化处理:
 *   - AEC (AEC_MODE_VOIP_HIGH_PERF 双麦收敛)
 *   - NS  (NSNet1 神经网络噪声抑制)
 *   - VAD (VADNet1 神经网络语音活动检测)
 *   - AGC (暂关)
 *
 * 流水线:
 *   ES7210 ADC 4ch@24k → AFE.feed(4ch) → AFE.fetch_with_delay()
 *     → 16kHz mono PCM + VAD state + Wake state
 *
 * 参考: /tmp/xiaozhi_p4c5/main/audio/processors/afe_audio_processor.cc
 *
 * 作者: Claude Code (p4c5 P1 实施)
 * 日期: 2026-09-07
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <functional>
#include "esp_err.h"

// VAD 状态 (与 ESP-SR 的 vad_state_t 一致)
typedef enum {
    AFE_VAD_SILENCE_P4 = 0,
    AFE_VAD_SPEECH_P4  = 1,
} audio_afe_vad_state_t;

// Wake 状态 (与 esp_afe_sr_iface 的 wakeup_state_t 一致)
typedef enum {
    AFE_WAKEUP_NONE     = 0,
    AFE_WAKEUP_DETECTED = 1,
} audio_afe_wakeup_state_t;

class AudioAfeProcessor {
public:
    AudioAfeProcessor();
    ~AudioAfeProcessor();

    /* 初始化 AFE handle
     *
     * @param input_format  通道格式字符串, e.g. "MMMR" (3 mic + 1 AEC ref)
     *                      p4c5 ES7210 4ch: ch0+ch2=mic, ch1=AEC_ref, ch3=unused
     *                      但 ESP-SR 要求连续的 M, 然后 R 在末尾, 所以:
     *                      实际硬件采样 ch0=mic, ch1=ref, ch2=mic, ch3=mic → "M R M M" = "MRMM"
     * @param model_partition  模型分区名, 默认 "model"
     * @param enable_aec  是否启用 AEC
     * @param enable_ns   是否启用 NSNet1 噪声抑制
     * @param enable_vad  是否启用 VAD
     * @param enable_wake 是否启用 WakeNet (一般通信模式关闭, wake 走独立 handle)
     */
    esp_err_t init(const char* input_format,
                   const char* model_partition = "model",
                   bool enable_aec = true,
                   bool enable_ns  = true,
                   bool enable_vad = true,
                   bool enable_wake = false);

    /* 喂入 4ch PCM (24kHz, 20ms = 480 samples/ch, total 1920 samples)
     * 必须 size == get_feed_chunksize()
     */
    esp_err_t feed(const int16_t* in_4ch, size_t samples);

    /* 阻塞取 AFE 处理后的 16kHz mono PCM (通常 30ms = 480 samples)
     * @param out_pcm    输出 buffer (调用方分配, 至少 1024 samples)
     * @param out_samples 输出实际 samples 数
     * @param out_vad    VAD 状态
     * @return ESP_OK
     */
    esp_err_t fetch(int16_t* out_pcm, size_t out_buf_size,
                    size_t* out_samples,
                    audio_afe_vad_state_t* out_vad);

    /* 非阻塞查询 (没数据时立刻返回 ESP_ERR_NOT_FOUND) */
    esp_err_t fetch_immediate(int16_t* out_pcm, size_t out_buf_size,
                              size_t* out_samples,
                              audio_afe_vad_state_t* out_vad);

    /* 切换 AEC 开关 (运行中可切换, 比如从录音切到播放) */
    esp_err_t enable_aec(bool enable);

    /* 切换 VAD 开关 */
    esp_err_t enable_vad(bool enable);

    /* 切回放 (重置 AEC 参考 buffer) */
    esp_err_t reset();

    /* 信息查询 */
    size_t get_feed_chunk_samples() const;   // 喂入 frame samples (所有通道总和)
    size_t get_fetch_chunk_samples() const;  // 输出 frame samples (mono 16k)
    bool is_initialized() const { return impl_ != nullptr; }

private:
    /* PIMPL: 隐藏 ESP-SR 类型 */
    void* impl_ = nullptr;       /* AudioAfeImpl* */
    int  feed_chunk_bytes_  = 0;
    int  fetch_chunk_bytes_ = 0;
};
