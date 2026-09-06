/*
 * Wake Word Detector - 实现 (W4 POC)
 *
 * 算法:
 *   1. 每帧计算 RMS = sqrt(sum(x²)/N)
 *   2. RMS > wake_threshold → 唤醒计数 +1
 *   3. RMS < sleep_threshold → 睡眠计数 +1
 *   4. 唤醒计数 ≥ wake_hold_frames → 触发 WAKE event, 重置
 *   5. 睡眠计数 ≥ sleep_hold_frames → 触发 SLEEP event, 重置
 */

#include "wake_word_detector.h"
#include "esp_log.h"

#include <cmath>

static const char *TAG = "wake_det";

namespace audio {

// ── 模块状态 ──
static struct {
    bool initialized;
    WakeDetectorConfig cfg;
    uint32_t wake_count;     // 唤醒计数 (连续超过 wake_threshold 的帧数)
    uint32_t sleep_count;    // 睡眠计数 (连续低于 sleep_threshold 的帧数)
    bool active;             // 是否已唤醒 (用于区分 IDLE/WAKE 后状态)
    uint64_t frames_processed;
    uint64_t wake_events;
    uint64_t sleep_events;
} s_state = {};

static uint32_t calc_rms(const int16_t *buf, size_t n)
{
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t v = buf[i];
        sum_sq += (uint64_t)(v * v);
    }
    uint32_t mean = (uint32_t)(sum_sq / n);
    /* 整数牛顿法开方 */
    if (mean == 0) return 0;
    uint32_t x = mean;
    uint32_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + mean / x) >> 1;
    }
    return x;
}

esp_err_t wake_word_detector_init(const WakeDetectorConfig& cfg)
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }
    s_state.cfg = cfg;
    s_state.wake_count = 0;
    s_state.sleep_count = 0;
    s_state.active = false;
    s_state.frames_processed = 0;
    s_state.wake_events = 0;
    s_state.sleep_events = 0;
    s_state.initialized = true;
    ESP_LOGI(TAG, "init OK (sample_rate=%d, wake_th=%u, sleep_th=%u, "
                 "wake_hold=%u frames=%ums, sleep_hold=%u frames=%ums)",
             cfg.sample_rate, cfg.wake_rms_threshold, cfg.sleep_rms_threshold,
             cfg.wake_hold_frames, cfg.wake_hold_frames * 1000 / (cfg.sample_rate / 480),
             cfg.sleep_hold_frames, cfg.sleep_hold_frames * 1000 / (cfg.sample_rate / 480));
    return ESP_OK;
}

esp_err_t wake_word_detector_deinit()
{
    s_state.initialized = false;
    ESP_LOGI(TAG, "deinit");
    return ESP_OK;
}

WakeEvent wake_word_detector_feed(const int16_t *pcm, size_t samples)
{
    if (!s_state.initialized || !pcm || samples == 0) {
        return WakeEvent::NONE;
    }

    s_state.frames_processed++;

    uint32_t rms = calc_rms(pcm, samples);

    /* 状态机 */
    if (!s_state.active) {
        /* 当前 IDLE: 检测唤醒 */
        if (rms > s_state.cfg.wake_rms_threshold) {
            s_state.wake_count++;
            if (s_state.wake_count >= s_state.cfg.wake_hold_frames) {
                s_state.active = true;
                s_state.wake_count = 0;
                s_state.sleep_count = 0;
                s_state.wake_events++;
                ESP_LOGI(TAG, "🌟 WAKE detected! (RMS=%u, events=%llu)",
                         rms, (unsigned long long)s_state.wake_events);
                return WakeEvent::WAKE;
            }
        } else {
            if (s_state.wake_count > 0) {
                s_state.wake_count--;
            }
        }
    } else {
        /* 当前 ACTIVE: 检测睡眠 */
        if (rms < s_state.cfg.sleep_rms_threshold) {
            s_state.sleep_count++;
            if (s_state.sleep_count >= s_state.cfg.sleep_hold_frames) {
                s_state.active = false;
                s_state.sleep_count = 0;
                s_state.wake_count = 0;
                s_state.sleep_events++;
                ESP_LOGI(TAG, "💤 SLEEP detected (RMS=%u, events=%llu)",
                         rms, (unsigned long long)s_state.sleep_events);
                return WakeEvent::SLEEP;
            }
        } else {
            if (s_state.sleep_count > 0) {
                s_state.sleep_count--;
            }
        }
    }

    /* 每 50 帧 (~1s) 输出 RMS 状态 */
    if ((s_state.frames_processed % 50) == 0) {
        ESP_LOGD(TAG, "RMS=%u wake_count=%u sleep_count=%u active=%d",
                 rms, s_state.wake_count, s_state.sleep_count, (int)s_state.active);
    }

    return WakeEvent::NONE;
}

void wake_word_detector_print_stats()
{
    ESP_LOGI(TAG, "📊 [stats] frames=%llu wake=%llu sleep=%llu active=%d",
             (unsigned long long)s_state.frames_processed,
             (unsigned long long)s_state.wake_events,
             (unsigned long long)s_state.sleep_events,
             (int)s_state.active);
}

}  // namespace audio