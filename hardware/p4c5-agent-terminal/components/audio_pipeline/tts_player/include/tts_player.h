/*
 * TTS Player - 接口 (W3 下行音频)
 *
 * 接收 WS Binary PCM 帧 → ES8311 DAC 播放
 *
 * 设计:
 *   - 双缓冲队列: 接收 buffer + 播放 buffer
 *   - 60ms / 帧 @ 24kHz = 1440 samples = 2880 bytes (与 TTS Adapter 一致)
 *   - FreeRTOS task: tts_player_task (从队列取 PCM → p4c5_audio_play)
 *
 * 链路:
 *   TTS Adapter 下行 WS Binary PCM (24kHz mono Int16)
 *   → dsh_client_set_binary_callback() 接收
 *   → tts_player_feed_pcm() 入队
 *   → tts_player_task 消费
 *   → p4c5_audio_play() → ES8311 DAC → 扬声器
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 初始化 (创建队列 + 启动 task + 启用 output)
esp_err_t tts_player_init();

// 反初始化
esp_err_t tts_player_deinit();

// 喂入一帧 PCM (24kHz mono Int16, 60ms = 1440 samples = 2880 bytes)
// 来自 dsh_client binary callback
esp_err_t tts_player_feed_pcm(const int8_t *pcm, size_t len);

// 查询统计 (用于串口命令)
void tts_player_print_stats();

}  // namespace audio