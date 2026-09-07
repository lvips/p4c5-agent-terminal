/*
 * Audio Wake Word — WakeNet9 唤醒词检测 (P1)
 *
 * 用 ESP-SR WakeNet9 神经网络唤醒词, 替换自实现 RMS 阈值检测.
 *
 * 默认唤醒词: "嗨乐鑫" (wn9_hilexin)
 * 可用模型 (看 ESP-SR model/wakenet9 目录):
 *   - wn9_hilexin      嗨乐鑫 (中文, 默认)
 *   - wn9_nihaoxiaozhi 你好小智 (中文)
 *   - wn9_alexa        Alexa (英文)
 *   - wn9_heywillow    Hey Willow (英文)
 *
 * 用法:
 *   AudioWakeWord wake;
 *   wake.init("MRMM", "model", "wn9_hilexin");
 *   wake.on_detected([](const char* word) {
 *       ESP_LOGI("WAKE", "detected: %s", word);
 *   });
 *   wake.start();
 *
 *   for (;;) {
 *       // 喂 16kHz mono PCM
 *       int16_t pcm_16k[480];
 *       wake.feed(pcm_16k, 480);
 *   }
 *
 * 参考: /tmp/xiaozhi_p4c5/main/audio/wake_words/afe_wake_word.cc
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <functional>
#include <string>
#include "esp_err.h"

class AudioWakeWord {
public:
    AudioWakeWord();
    ~AudioWakeWord();

    /* 初始化
     *
     * @param input_format    通道格式 (与 AFE 一致), "MRMM" (1 mic + 1 ref + 2 mic)
     * @param model_partition 模型分区名, 默认 "model"
     * @param model_name      WakeNet 模型名, NULL = 自动选第一个
     */
    esp_err_t init(const char* input_format,
                   const char* model_partition = "model",
                   const char* model_name      = nullptr);

    /* 喂 16kHz mono PCM (必须 size == get_feed_chunk_samples()) */
    esp_err_t feed(const int16_t* pcm_16k_mono, size_t samples);

    /* 设置唤醒检测回调 (在 fetch task 里触发) */
    void on_detected(std::function<void(const std::string& wake_word)> cb);

    /* 启动 / 停止 wake 检测 */
    esp_err_t start();
    esp_err_t stop();

    /* 信息 */
    size_t get_feed_chunk_samples() const;
    bool   is_initialized() const { return impl_ != nullptr; }

    /* 获取所有支持的 wake words (从模型 metadata 解析) */
    const std::vector<std::string>& get_wake_words() const { return wake_words_; }

private:
    /* PIMPL 模式: 隐藏 ESP-SR 类型, 避免在 header 暴露不完整类型 */
    void* impl_ = nullptr;     /* AudioWakeWordImpl* */

    int  feed_chunk_bytes_  = 0;
    bool running_           = false;
    std::vector<std::string> wake_words_;
    std::function<void(const std::string&)> on_detected_cb_;
    void* task_handle_      = nullptr;

    void detection_task();
    static void detection_task_trampoline(void* arg);
};
