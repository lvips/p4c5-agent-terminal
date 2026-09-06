/**
 * @file p4c5_audio.h
 * @brief P4C5 完整音频子系统 — ES8311 (DAC) + ES7210 (ADC) + NS4150B (PA)
 *
 * 基于 xiaozhi main/audio/codecs/box_audio_codec.cc 移植为独立 C API 组件。
 * 使用 espressif/esp_audio_codec ~2.4.1 提供 ES8311/ES7210 codec 驱动。
 *
 * 架构：
 *   I2C0 (SDA=7, SCL=8) ← AXP2101 / ES8311(0x18) / ES7210(0x40) / ST7123 / IMU / DAC
 *   I2S0 (MCLK=13 BCLK=12 WS=10 DOUT=9 DIN=11)
 *     TX: STD 模式 → ES8311 DAC（单声道 16bit 24kHz）
 *     RX: TDM 模式 ← ES7210 ADC（4 通道 16bit 24kHz，含 AEC 参考）
 *   PA_EN (GPIO3) → NS4150B 使能
 *
 * 测试覆盖（audio-test-spec.md）：
 *   TC-01 ~ TC-12 全链路验证
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────
 * 初始化 / 反初始化
 * ────────────────────────────────────────────── */

/**
 * @brief 注入共享 I2C master bus handle（必须在 init 之前调用）
 *
 * p4c5_board 创建 I2C bus 后调用此函数注入，供 ES8311/ES7210 I2C 控制使用。
 *
 * @param i2c_bus  i2c_master_bus_handle_t（void* 以维持 C ABI）
 */
void p4c5_audio_set_i2c_bus(void* i2c_bus);

/**
 * @brief 初始化音频子系统
 *
 * 完整初始化序列：
 *   1. 创建 I2S duplex channel（TX=STD, RX=TDM）
 *   2. 初始化 ES8311 DAC（I2C 0x18, PA pin=GPIO3）
 *   3. 初始化 ES7210 ADC（I2C 0x40, 4 麦全开 + AEC ref）
 *   4. 使能 NS4150B PA（GPIO3 → HIGH）
 *
 * 对应测试：TC-01 (ES8311), TC-02 (ES7210), TC-03 (I2S 时钟)
 *
 * @return ESP_OK 成功
 * @return ESP_ERR_INVALID_STATE I2C bus 未注入
 * @return ESP_ERR_NO_MEM codec 创建失败
 */
esp_err_t p4c5_audio_init(void);

/**
 * @brief 反初始化音频子系统，释放全部资源
 *
 * 释放顺序：codec dev → codec if → ctrl if → gpio if → data if → I2S channel
 * 对应测试：TC-11（资源释放 / 内存泄漏检查）
 */
void p4c5_audio_deinit(void);

/* ──────────────────────────────────────────────
 * 播放 / 录音
 * ────────────────────────────────────────────── */

/**
 * @brief 播放 PCM 数据（阻塞）
 *
 * 通过 I2S TX → ES8311 DAC 输出。
 * 数据格式：16-bit signed PCM, 单声道, 24kHz
 * 阻塞直到写完或超时 1 秒。
 *
 * 对应测试：TC-04 (1kHz 测试音), TC-12 (双工播放)
 *
 * @param data     16-bit signed PCM buffer
 * @param samples  采样点数（不是字节数）
 * @return ESP_OK 成功
 * @return ESP_ERR_TIMEOUT 写入超时
 * @return ESP_ERR_INVALID_STATE 未初始化或输出未使能
 */
esp_err_t p4c5_audio_play(const int16_t* data, size_t samples);

/**
 * @brief 录音 PCM 数据（单声道混合输出，阻塞）
 *
 * 通过 I2S RX ← ES7210 ADC 读取 4 通道，混合为单声道输出。
 * 数据格式：16-bit signed PCM, 单声道, 24kHz
 *
 * 对应测试：TC-08 (录音回放)
 *
 * @param dest     输出 buffer
 * @param samples  目标采样点数
 * @return ESP_OK 成功
 * @return ESP_ERR_TIMEOUT 读取超时
 * @return ESP_ERR_INVALID_STATE 未初始化或输入未使能
 */
esp_err_t p4c5_audio_record(int16_t* dest, size_t samples);

/**
 * @brief 录音 PCM 数据（4 通道独立输出，阻塞）
 *
 * I2S RX 原始 4 通道数据，interleaved 格式：
 * [MIC1_0, MIC2_0, MIC3_0, MIC4_0, MIC1_1, MIC2_1, ...]
 *
 * 对应测试：TC-07 (4 麦独立录音)
 *
 * @param dest     输出 buffer，大小至少 samples * 4 * sizeof(int16_t)
 * @param samples  每个通道的采样点数
 * @return ESP_OK 成功
 */
esp_err_t p4c5_audio_record_multi(int16_t* dest, size_t samples);

/* ──────────────────────────────────────────────
 * 音量 / 静音 / 增益
 * ────────────────────────────────────────────── */

/**
 * @brief 设置输出音量 (0-100)
 *
 * 线性映射到 ES8311 音量范围（约 -96dB ~ 0dB）。
 * level=0 等效静音，level=100 为最大音量。
 *
 * 对应测试：TC-06 (音量调节范围)
 *
 * @param level 0-100
 * @return ESP_OK 成功
 */
esp_err_t p4c5_audio_set_volume(uint8_t level);

/**
 * @brief 静音/解除静音
 *
 * 通过 ES8311 寄存器控制硬件静音。
 * 对应测试：TC-05 (PA 使能控制)
 *
 * @param mute true=静音, false=解除
 * @return ESP_OK 成功
 */
esp_err_t p4c5_audio_set_mute(bool mute);

/**
 * @brief 设置麦克风输入增益 (dB)
 *
 * 设置 ES7210 通道 0（MIC1）的数字增益。
 * 默认增益 30dB（与 xiaozhi 一致）。
 *
 * @param gain_db 增益值 (0-50 推荐范围)
 * @return ESP_OK 成功
 */
esp_err_t p4c5_audio_set_input_gain(uint8_t gain_db);

/* ──────────────────────────────────────────────
 * 使能/禁用
 * ────────────────────────────────────────────── */

/**
 * @brief 使能/禁用麦克风输入
 *
 * enable=true: esp_codec_dev_open(input_dev_) 配置 4ch + AEC ref
 * enable=false: esp_codec_dev_close(input_dev_)
 *
 * @param enable true=使能, false=禁用
 */
esp_err_t p4c5_audio_enable_input(bool enable);

/**
 * @brief 使能/禁用扬声器输出
 *
 * enable=true: esp_codec_dev_open(output_dev_) 配置 1ch
 * enable=false: esp_codec_dev_close(output_dev_)
 *
 * @param enable true=使能, false=禁用
 */
esp_err_t p4c5_audio_enable_output(bool enable);

/* ──────────────────────────────────────────────
 * 测试 / 诊断
 * ────────────────────────────────────────────── */

/**
 * @brief 播放正弦波测试音（阻塞）
 *
 * 生成指定频率和时长的正弦波，通过扬声器播放。
 * 24kHz 采样率, 16-bit, 单声道。
 * 内部自动使能输出，播放完成后不关闭（保留音量设置）。
 *
 * 对应测试：TC-04 (1kHz 测试音)
 *
 * @param freq_hz     频率 (Hz), 推荐 20-10000
 * @param duration_ms 时长 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t p4c5_audio_test_tone(uint32_t freq_hz, uint32_t duration_ms);

/**
 * @brief 查询音频子系统是否已初始化
 */
bool p4c5_audio_is_initialized(void);

#ifdef __cplusplus
}
#endif
