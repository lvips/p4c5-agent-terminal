/*
 * 软件 AEC 回声消除 - 接口 (W3)
 *
 * 设计目标:
 *   - POC 阶段简化版, 在 ESP32-P4 上可用 (不依赖 ESP-SR 硬件加速)
 *   - 适配 OMT/xiaozhi 4 麦 + 1 ref 硬件设计:
 *       ch0=mic1, ch1=AEC ref, ch2=mic3, ch3=mic4
 *   - 算法: NLMS 自适应滤波 (16kHz, 单参考)
 *
 * 重要限制:
 *   - ESP32-P4 没有 ESP-SR 硬件加速, 此软件 AEC 质量 < ESP-SR AFE
 *   - 仅适用于 POC 演示; 量产推荐 ESP-SR AFE (需 ESP32-S3)
 *   - 仅当硬件真的有 AEC ref 回采 (ES8311 输出 → ES7210 MIC2)
 *     时此 AEC 才有效; 否则 ref 通道实际是 MIC2, 处理无意义
 *
 * 调用流程 (audio_uplink_task):
 *   1. 读取 4 通道 24kHz PCM
 *   2. 分离 ch0/ch2 (2 麦) → 平均为 mono_mic
 *   3. 提取 ch1 → ref
 *   4. aec_sw_process(mic, ref, out) → out (24kHz mono)
 *   5. resampler_24_16 → 16kHz → opus
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 配置
struct AecConfig {
    uint32_t sample_rate;       // 16000/24000 (P4C5 默认 24kHz)
    uint16_t filter_taps;       // 自适应滤波长度 (推荐 64-256)
    float    step_size;         // NLMS 学习步长 (0.001-0.01, 越小越稳定)
    float    leakage;           // NLMS 泄漏因子 (0.999, 防止发散)
    float    ref_gain;          // 参考通道增益 (1.0 = 原始, 0.5 = 减半)
    bool     enable_aec;        // 总开关
    bool     enable_residual;   // 残留回声抑制 (简单做法: 频谱减法)
};

AecConfig aec_sw_default_config();
esp_err_t aec_sw_init(const AecConfig &cfg);
esp_err_t aec_sw_deinit(void);
void      aec_sw_reset(void);

/*
 * 处理一帧 (20ms @ 24kHz = 480 samples)
 *
 * @param mic  输入 mic 信号 (mono int16, ch0/ch2 平均)
 * @param ref  输入 AEC ref 信号 (mono int16, ch1)
 * @param out  输出 AEC 后信号 (mono int16)
 * @param samples 帧内采样点 (24kHz 时 480, 16kHz 时 320)
 * @return ESP_OK / ESP_ERR_INVALID_STATE
 */
esp_err_t aec_sw_process(const int16_t *mic,
                         const int16_t *ref,
                         int16_t       *out,
                         size_t         samples);

}  // namespace audio