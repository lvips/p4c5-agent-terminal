/*
 * OPUS Encoder - 实现
 *
 * 封装 libopus C API 为 C++ 接口
 *
 * 注意：libopus 的 C 函数（opus_encoder_init 等）与本命名空间函数同名，
 *       因此通过 :: 全局命名空间引用 libopus 的 C 函数。
 */

#include "opus_encoder.h"

#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// libopus C API
#include "opus.h"

static const char *TAG = "opus_enc";

// 推荐最大输出包大小：
//   16kbps + 20ms 典型输出 ~40 字节，留余量取 1276
//   OPUS RFC 6716 协议上限 4000 字节，VOIP 场景远低于此
static constexpr size_t MAX_PACKET_SIZE = 1276;

namespace audio {

// ── 模块状态 ──
static struct {
    bool              initialized;
    ::OpusEncoder    *encoder;       // libopus 句柄
    OpusEncoderConfig config;
    size_t            frame_samples; // 每帧采样点数
    SemaphoreHandle_t mutex;
} s_state = {};

// ── 公共接口 ──

esp_err_t opus_encoder_init(const OpusEncoderConfig &cfg)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized, deinit first");
        return ESP_ERR_INVALID_STATE;
    }

    // 参数校验
    if (cfg.channels == 0 || cfg.channels > 2) {
        ESP_LOGE(TAG, "invalid channels: %u (must be 1 or 2)", cfg.channels);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.sample_rate == 0) {
        ESP_LOGE(TAG, "invalid sample_rate: %lu", (unsigned long)cfg.sample_rate);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.bitrate == 0 || cfg.bitrate > 512000) {
        ESP_LOGE(TAG, "invalid bitrate: %lu (must be 500..512000)",
                 (unsigned long)cfg.bitrate);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg.frame_ms != 20 && cfg.frame_ms != 40 && cfg.frame_ms != 60
        && cfg.frame_ms != 10 && cfg.frame_ms != 5) {
        ESP_LOGW(TAG, "non-standard frame_ms: %lu (typical: 20)",
                 (unsigned long)cfg.frame_ms);
    }

    // 计算每帧采样点数
    size_t frame_samples = (size_t)cfg.sample_rate * cfg.frame_ms / 1000;
    if (frame_samples == 0 || frame_samples > 5760) {
        // OPUS 最大帧 120ms@48kHz = 5760
        ESP_LOGE(TAG, "frame_samples out of range: %zu", frame_samples);
        return ESP_ERR_INVALID_ARG;
    }

    // 分配 + 初始化 OpusEncoder（libopus 自己算大小）
    int error = 0;
    ::OpusEncoder *enc = ::opus_encoder_create(
        (opus_int32)cfg.sample_rate, (int)cfg.channels,
        OPUS_APPLICATION_VOIP, &error);
    if (!enc || error != OPUS_OK) {
        ESP_LOGE(TAG, "opus_encoder_create failed: %d (%s)",
                 error, opus_strerror(error));
        if (enc) {
            ::opus_encoder_destroy(enc);
        }
        return ESP_FAIL;
    }

    // 设置码率
    error = ::opus_encoder_ctl(enc, OPUS_SET_BITRATE((opus_int32)cfg.bitrate));
    if (error != OPUS_OK) {
        ESP_LOGW(TAG, "OPUS_SET_BITRATE(%lu) failed: %d (%s)",
                 (unsigned long)cfg.bitrate, error, opus_strerror(error));
        // 非致命
    }

    // VOIP 模式信号（强制语音，禁用音乐模式启发式）
    error = ::opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    if (error != OPUS_OK) {
        ESP_LOGW(TAG, "OPUS_SET_SIGNAL(VOICE) failed: %d", error);
    }

    // 复杂度（0-10，嵌入式默认 2；用户可通过 cfg.complexity 覆盖）
    // ★ fix(audio-stack): 默认 5→2。复杂度越低 opus_encode 内部递归/缓冲区栈帧越小，
    //   实测省 ~0.5-1KB。音质损失在语音场景可接受（16kHz mono 16kbps 已窄带）。
    int complexity = (cfg.complexity >= 0 && cfg.complexity <= 10) ? cfg.complexity : 2;
    error = ::opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
    if (error != OPUS_OK) {
        ESP_LOGW(TAG, "OPUS_SET_COMPLEXITY(%d) failed: %d", complexity, error);
    }

    // ★ fix(audio-stack): FEC 禁用 (1→0)。FEC 内部需要前一帧缓冲 → 多 1-2KB 栈帧。
    //   当前走 WiFi/4G 丢包率 <1%，FEC 收益不大；栈紧张时优先关闭。
    //   后续若丢包率上升，可通过 cfg 重新启用。
    error = ::opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
    if (error != OPUS_OK) {
        ESP_LOGW(TAG, "OPUS_SET_INBAND_FEC(0) failed: %d", error);
    }

    // 丢包率期望（配合 FEC，典型 1-5%）
    error = ::opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(2));
    if (error != OPUS_OK) {
        ESP_LOGW(TAG, "OPUS_SET_PACKET_LOSS_PERC(2) failed: %d", error);
    }

    auto mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        ESP_LOGE(TAG, "create mutex failed");
        ::opus_encoder_destroy(enc);
        return ESP_ERR_NO_MEM;
    }

    s_state.encoder       = enc;
    s_state.config        = cfg;
    s_state.frame_samples = frame_samples;
    s_state.mutex         = mutex;
    s_state.initialized   = true;

    ESP_LOGI(TAG, "initialized: %luHz/%uch/%lubps/%lums/complexity=%d (frame=%zu samples, max_pkt=%zu)",
             (unsigned long)cfg.sample_rate, cfg.channels,
             (unsigned long)cfg.bitrate, (unsigned long)cfg.frame_ms,
             complexity, frame_samples, MAX_PACKET_SIZE);
    return ESP_OK;
}

esp_err_t opus_encoder_encode(const int16_t *pcm, size_t samples,
                               uint8_t *out, size_t *out_len)
{
    if (!s_state.initialized) {
        ESP_LOGE(TAG, "encode: not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!pcm || !out || !out_len) {
        ESP_LOGE(TAG, "encode: null arg");
        return ESP_ERR_INVALID_ARG;
    }
    if (samples != s_state.frame_samples) {
        ESP_LOGE(TAG, "encode: samples=%zu != frame_samples=%zu",
                 samples, s_state.frame_samples);
        return ESP_ERR_INVALID_SIZE;
    }

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "encode: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    opus_int32 ret = ::opus_encode(s_state.encoder, pcm, (int)samples,
                                    out, (opus_int32)*out_len);

    xSemaphoreGive(s_state.mutex);

    if (ret < 0) {
        ESP_LOGE(TAG, "opus_encode failed: %ld (%s)", (long)ret, opus_strerror(ret));
        *out_len = 0;
        return ESP_FAIL;
    }

    *out_len = (size_t)ret;
    return ESP_OK;
}

void opus_encoder_deinit()
{
    if (!s_state.initialized) {
        return;
    }

    if (s_state.mutex) {
        xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    }

    if (s_state.encoder) {
        ::opus_encoder_destroy(s_state.encoder);
        s_state.encoder = nullptr;
    }

    s_state.initialized = false;

    if (s_state.mutex) {
        xSemaphoreGive(s_state.mutex);
        vSemaphoreDelete(s_state.mutex);
        s_state.mutex = nullptr;
    }

    ESP_LOGI(TAG, "deinitialized");
}

bool opus_encoder_is_initialized()
{
    return s_state.initialized;
}

size_t opus_encoder_get_frame_samples()
{
    return s_state.frame_samples;
}

size_t opus_encoder_get_max_packet_size()
{
    return MAX_PACKET_SIZE;
}

}  // namespace audio
