/*
 * Wake Word Detector - 接口 (W4 POC)
 *
 * P4 自实现 RMS-based 语音唤醒检测 (无 ESP-SR AFE 硬件加速)
 *
 * 设计:
 *   - 计算每帧 RMS (20ms @ 24kHz = 480 samples)
 *   - RMS > WAKE_RMS_THRESHOLD 持续 WAKE_HOLD_FRAMES 帧 → 触发 wake event
 *   - RMS < SLEEP_RMS_THRESHOLD 持续 SLEEP_HOLD_FRAMES 帧 → 触发 sleep event
 *   - 用于 audio_uplink_task 的状态机 (IDLE → RECORDING → SLEEP → IDLE)
 *
 * 限制:
 *   - 不是真正的 wake word detection (不需要 ML 模型)
 *   - 任何超过阈值的语音都会触发 (例如咳嗽、拍手)
 *   - 生产建议: 后期换 ESP32-S3 + ESP-SR WakeNet9 (~300KB 模型)
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 唤醒事件
enum class WakeEvent {
    NONE,    // 无事件
    WAKE,    // 检测到唤醒 (语音开始)
    SLEEP,   // 检测到睡眠 (语音结束)
};

// 配置
struct WakeDetectorConfig {
    int      sample_rate       = 24000;   // 输入采样率
    uint32_t wake_rms_threshold = 1500;   // 唤醒 RMS 阈值 (0-32768)
    uint32_t sleep_rms_threshold = 500;   // 睡眠 RMS 阈值
    uint32_t wake_hold_frames   = 5;      // 唤醒持续帧数 (5 帧 @ 20ms = 100ms)
    uint32_t sleep_hold_frames  = 100;    // 睡眠持续帧数 (100 帧 @ 20ms = 2s)
};

// 初始化
esp_err_t wake_word_detector_init(const WakeDetectorConfig& cfg);

// 反初始化
esp_err_t wake_word_detector_deinit();

// 喂入一帧 PCM (24kHz mono Int16, 480 samples = 20ms)
// 返回: 检测到的事件
WakeEvent wake_word_detector_feed(const int16_t *pcm, size_t samples);

// 查询统计 (用于串口命令)
void wake_word_detector_print_stats();

}  // namespace audio