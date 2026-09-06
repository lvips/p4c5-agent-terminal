/**
 * @file p4c5_audio.h
 * @brief P4C5 音频子系统 — ES8311 (DAC) + ES7210 (ADC) + NS4150B (PA)
 *
 * 基于 xiaozhi box_audio_codec.cc 移植，使用 espressif/esp_audio_codec ~2.4.1
 * 支持：
 *   - 24kHz 全双工 I2S
 *   - 4 麦克风输入 (ES7210 MIC1-4)
 *   - AEC 参考通道 (DAC loopback → ES7210 channel 2)
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化音频子系统
 *
 * 步骤：
 *   1. 创建 I2C master bus (I2C0, SDA=GPIO7, SCL=GPIO8)
 *   2. 创建 I2S duplex channel (MCLK=13, BCLK=12, WS=10, DOUT=9, DIN=11)
 *   3. 初始化 ES8311 (DAC, addr=0x18)
 *   4. 初始化 ES7210 (ADC, addr=0x40, 4mic + AEC ref)
 *   5. 使能 NS4150B PA (GPIO3)
 *
 * @return ESP_OK 成功，其他 esp_err_t 失败
 */
esp_err_t p4c5_audio_init(void);

/**
 * @brief 播放 PCM 数据
 * @param data  16-bit signed PCM buffer
 * @param samples 采样点数（不是字节数）
 * @return 实际写入的采样点数，< 0 表示错误
 */
int p4c5_audio_play(const int16_t* data, int samples);

/**
 * @brief 录音 PCM 数据
 * @param dest  16-bit signed PCM buffer
 * @param samples 采样点数
 * @return 实际读取的采样点数，< 0 表示错误
 */
int p4c5_audio_record(int16_t* dest, int samples);

/**
 * @brief 设置输出音量
 * @param volume 0-100
 */
esp_err_t p4c5_audio_set_volume(int volume);

/**
 * @brief 使能/禁用输入（麦克风）
 */
esp_err_t p4c5_audio_enable_input(bool enable);

/**
 * @brief 使能/禁用输出（扬声器）
 */
esp_err_t p4c5_audio_enable_output(bool enable);

/**
 * @brief 反初始化音频子系统，释放资源
 */
void p4c5_audio_deinit(void);

#ifdef __cplusplus
}
#endif
