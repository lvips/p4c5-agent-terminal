/*
 * TTS Player - 实现 (W3 下行音频)
 *
 * 接收 WS Binary PCM 帧 → ES8311 DAC 播放
 *
 * 同步调用 p4c5_audio_play() 是阻塞的 (~12ms @ 24kHz / 1440 samples)
 * - 单 task 阻塞播放 (够用, 因为 buffer 队列能吸收抖动)
 * - 生产可改为 I2S DMA + 中断驱动 (后续优化)
 */

#include "tts_player.h"
#include "p4c5_audio.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_log.h"

static const char *TAG = "tts_player";

// ══════════════════════════════════════════════════════════
// 队列配置
// ══════════════════════════════════════════════════════════

// 60ms @ 24kHz = 1440 samples = 2880 bytes (Int16 mono)
constexpr size_t FRAME_SAMPLES = 1440;
constexpr size_t FRAME_BYTES   = FRAME_SAMPLES * sizeof(int16_t);
// 队列深度: 8 帧 = 480ms (足够吸收网络抖动)
constexpr size_t QUEUE_DEPTH   = 8;

namespace audio {

// ── 模块状态 ──
static struct {
    bool initialized;
    QueueHandle_t pcm_queue;        // 帧队列
    TaskHandle_t  task_handle;
    uint64_t frames_received;       // 收到的帧数
    uint64_t frames_played;         // 播放的帧数
    uint64_t queue_overflow;        // 队列溢出 (网络突发)
} s_state = {};

// ── tts_player_task ──
static void tts_player_task(void *arg)
{
    (void)arg;

    int16_t *pcm_buf = (int16_t *)malloc(FRAME_BYTES);
    if (!pcm_buf) {
        ESP_LOGE(TAG, "malloc failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "tts_player_task 启动 (队列=%u 帧, 帧=%u ms)",
             QUEUE_DEPTH, 60);

    for (;;) {
        // 阻塞等待 PCM 帧 (portMAX_DELAY)
        if (xQueueReceive(s_state.pcm_queue, pcm_buf,
                          portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // 播放 (阻塞 ~12ms)
        esp_err_t ret = p4c5_audio_play(pcm_buf, FRAME_SAMPLES);
        if (ret == ESP_OK) {
            s_state.frames_played++;
            if ((s_state.frames_played % 100) == 0) {
                ESP_LOGI(TAG, "已播放 %llu 帧 (累计)",
                         (unsigned long long)s_state.frames_played);
            }
        } else {
            ESP_LOGW(TAG, "p4c5_audio_play 失败: %s", esp_err_to_name(ret));
        }
    }
}

esp_err_t tts_player_init()
{
    if (s_state.initialized) {
        ESP_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    s_state.pcm_queue = xQueueCreate(QUEUE_DEPTH, FRAME_BYTES);
    if (!s_state.pcm_queue) {
        ESP_LOGE(TAG, "xQueueCreate failed");
        return ESP_ERR_NO_MEM;
    }

    // 启用 DAC output
    esp_err_t ret = p4c5_audio_enable_output(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "p4c5_audio_enable_output failed: %s", esp_err_to_name(ret));
        vQueueDelete(s_state.pcm_queue);
        s_state.pcm_queue = nullptr;
        return ret;
    }

    // 启动 task (优先级 4, 较小避免抢占 mic task)
    BaseType_t ok = xTaskCreate(tts_player_task, "tts_player", 8192,
                                NULL, 4, &s_state.task_handle);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        p4c5_audio_enable_output(false);
        vQueueDelete(s_state.pcm_queue);
        s_state.pcm_queue = nullptr;
        return ESP_ERR_NO_MEM;
    }

    s_state.frames_received = 0;
    s_state.frames_played   = 0;
    s_state.queue_overflow  = 0;
    s_state.initialized     = true;
    ESP_LOGI(TAG, "init OK");
    return ESP_OK;
}

esp_err_t tts_player_deinit()
{
    if (!s_state.initialized) return ESP_OK;
    if (s_state.task_handle) vTaskDelete(s_state.task_handle);
    p4c5_audio_enable_output(false);
    if (s_state.pcm_queue) vQueueDelete(s_state.pcm_queue);
    s_state.pcm_queue = nullptr;
    s_state.task_handle = nullptr;
    s_state.initialized = false;
    ESP_LOGI(TAG, "deinit");
    return ESP_OK;
}

esp_err_t tts_player_feed_pcm(const int8_t *pcm, size_t len)
{
    if (!s_state.initialized || !pcm || len == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    // 校验帧长度 (允许 ±10% 容差, 处理 DAC 抖动)
    if (len != FRAME_BYTES) {
        ESP_LOGW(TAG, "非标帧长度 %u (期望 %u), 跳过", len, FRAME_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    // 入队 (短等待, 避免阻塞 callback 太久)
    int16_t buf[FRAME_SAMPLES];
    memcpy(buf, pcm, FRAME_BYTES);
    BaseType_t ok = xQueueSend(s_state.pcm_queue, buf, pdMS_TO_TICKS(20));
    if (ok != pdTRUE) {
        s_state.queue_overflow++;
        if ((s_state.queue_overflow % 100) == 1) {
            ESP_LOGW(TAG, "队列溢出 (累计 %llu 次)",
                     (unsigned long long)s_state.queue_overflow);
        }
        return ESP_ERR_TIMEOUT;
    }
    s_state.frames_received++;
    return ESP_OK;
}

void tts_player_print_stats()
{
    ESP_LOGI(TAG, "📊 [stats] received=%llu played=%llu overflow=%llu queue=%u/%u",
             (unsigned long long)s_state.frames_received,
             (unsigned long long)s_state.frames_played,
             (unsigned long long)s_state.queue_overflow,
             uxQueueMessagesWaiting(s_state.pcm_queue),
             QUEUE_DEPTH);
}

}  // namespace audio