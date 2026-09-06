/*
 * Resampler 24kHz → 16kHz - 接口 (W3: 适配 p4c5_audio 24kHz)
 *
 * p4c5_audio 配置为 24kHz 4ch 录音。
 * 链路: p4c5_audio → audio_mixer (4→1) → resampler_24_16 (24k→16k) → opus_encoder
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "esp_err.h"

namespace audio {

// 配置 (POC: 固定 24→16, 不支持任意比率)
struct ResamplerConfig {
    uint32_t in_rate;
    uint32_t out_rate;
    uint8_t  interp;
    uint8_t  decim;
    uint16_t filter_taps;
    uint32_t filter_cutoff_hz;
    int16_t  shift;
    uint16_t start_pos;
};

ResamplerConfig resampler_24_16_default_config();
esp_err_t resampler_24_16_init(const ResamplerConfig &cfg);
esp_err_t resampler_24_16_deinit(void);
void resampler_24_16_reset(void);

/*
 * 线性插值重采样
 * in:  480 samples @ 24 kHz, mono int16
 * out: 320 samples @ 16 kHz, mono int16
 */
esp_err_t resampler_24_16_process(const int16_t *in,
                                  int16_t       *out,
                                  size_t        *out_len);

}  // namespace audio
