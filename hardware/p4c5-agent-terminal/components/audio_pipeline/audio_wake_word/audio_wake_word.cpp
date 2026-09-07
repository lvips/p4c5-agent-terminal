/*
 * Audio Wake Word — WakeNet9 实现 (P1)
 *
 * 基于 xiaozhi p4c5 main/audio/wake_words/afe_wake_word.cc 简化.
 *
 * 与 AudioAfeProcessor 不同, wake_word 用 AFE_TYPE_SR 模式:
 *   - 独立 AFE handle, 不和 AFE_TYPE_VC 抢资源
 *   - 在独立 task 里跑 fetch_with_delay() 阻塞检测唤醒
 *   - 检测到 → on_detected_cb_(wake_word)
 *
 * 用 PIMPL 隐藏 ESP-SR 类型, header 不暴露 esp_afe_sr_iface_t.
 */
#include "audio_wake_word.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"  /* esp-sr v2.3.x: srmodel API moved from esp_srmodel.h to model_path.h */

#include <sstream>
#include <cstring>

static const char* TAG = "WakeWord";

/* ── PIMPL 实现结构体 (cpp 内私有) ── */
struct AudioWakeWordImpl {
    const esp_afe_sr_iface_t* iface = nullptr;
    esp_afe_sr_data_t*        data  = nullptr;
    srmodel_list_t*           models = nullptr;
    int  feed_chunk_bytes  = 0;

    ~AudioWakeWordImpl() {
        if (data != nullptr && iface != nullptr) iface->destroy(data);
        if (models != nullptr) esp_srmodel_deinit(models);
    }
};

AudioWakeWord::AudioWakeWord() {}
AudioWakeWord::~AudioWakeWord() {
    stop();
    delete (AudioWakeWordImpl*)impl_;
}

esp_err_t AudioWakeWord::init(const char* input_format,
                              const char* model_partition,
                              const char* model_name)
{
    if (impl_ != nullptr) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    AudioWakeWordImpl* impl = new AudioWakeWordImpl();
    impl_ = impl;

    /* 1. 加载模型 */
    impl->models = esp_srmodel_init(model_partition);
    if (impl->models == nullptr) {
        ESP_LOGE(TAG, "esp_srmodel_init('%s') failed", model_partition);
        return ESP_ERR_NOT_FOUND;
    }

    /* 2. 找 WakeNet9 模型 */
    char* wn_name = nullptr;
    if (model_name != nullptr) {
        /* 复制用户传入的 model_name (避免 const char* → char* 转换错误) */
        wn_name = strdup(model_name);
    } else {
        wn_name = esp_srmodel_filter(impl->models, ESP_WN_PREFIX, NULL);
    }
    if (wn_name == nullptr) {
        ESP_LOGE(TAG, "No WakeNet model found (want %s)", model_name ? model_name : "any");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Using WakeNet model: %s", wn_name);

    /* 3. 解析 wake words 列表 */
    char* words_str = esp_srmodel_get_wake_words(impl->models, wn_name);
    if (words_str != nullptr) {
        std::stringstream ss(words_str);
        std::string word;
        while (std::getline(ss, word, ';')) {
            if (!word.empty()) wake_words_.push_back(word);
        }
        ESP_LOGI(TAG, "WakeNet supports %zu wake words:", wake_words_.size());
        for (const auto& w : wake_words_) {
            ESP_LOGI(TAG, "  - '%s'", w.c_str());
        }
    }

    /* 4. 配置 WakeNet 用 AFE_TYPE_SR 模式 */
    afe_config_t* wake_cfg = afe_config_init(
        input_format,
        impl->models,
        AFE_TYPE_SR,
        AFE_MODE_HIGH_PERF);

    wake_cfg->aec_init = true;
    wake_cfg->aec_mode = AEC_MODE_SR_HIGH_PERF;
    wake_cfg->wakenet_init = true;
    wake_cfg->wakenet_model_name = wn_name;

    wake_cfg->afe_perferred_core = 1;
    wake_cfg->afe_perferred_priority = 1;
    wake_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    impl->iface = esp_afe_handle_from_config(wake_cfg);
    if (impl->iface == nullptr) {
        ESP_LOGE(TAG, "esp_afe_handle_from_config failed");
        return ESP_ERR_NOT_SUPPORTED;
    }
    impl->data = impl->iface->create_from_config(wake_cfg);
    if (impl->data == nullptr) {
        ESP_LOGE(TAG, "iface->create_from_config failed");
        return ESP_ERR_NO_MEM;
    }

    impl->feed_chunk_bytes = impl->iface->get_feed_chunksize(impl->data);
    feed_chunk_bytes_ = impl->feed_chunk_bytes;

    ESP_LOGI(TAG, "WakeWord init OK: feed_chunk=%d bytes (%.1fms@16kHz mono)",
             feed_chunk_bytes_,
             feed_chunk_bytes_ * 1000.0f / 16000.0f);

    return ESP_OK;
}

esp_err_t AudioWakeWord::start() {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    if (running_) return ESP_OK;
    running_ = true;

    BaseType_t ret = xTaskCreate(
        detection_task_trampoline,
        "wake_detect",
        4096,
        this,
        3,
        nullptr);
    if (ret != pdPASS) {
        running_ = false;
        ESP_LOGE(TAG, "Failed to create detection task");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "WakeWord detection started");
    return ESP_OK;
}

esp_err_t AudioWakeWord::stop() {
    running_ = false;
    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(50));
        task_handle_ = nullptr;
    }
    return ESP_OK;
}

void AudioWakeWord::on_detected(std::function<void(const std::string&)> cb) {
    on_detected_cb_ = cb;
}

void AudioWakeWord::detection_task_trampoline(void* arg) {
    auto* self = (AudioWakeWord*)arg;
    self->detection_task();
    vTaskDelete(nullptr);
}

void AudioWakeWord::detection_task() {
    auto* impl = (AudioWakeWordImpl*)impl_;
    ESP_LOGI(TAG, "Detection task running (wake_words: %zu)", wake_words_.size());

    while (running_) {
        afe_fetch_result_t* res = impl->iface->fetch_with_delay(impl->data, portMAX_DELAY);
        if (res == nullptr) continue;

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "🚨 WAKEUP DETECTED!");

            const char* word = "unknown";
            if (res->data != nullptr && res->data_size > 0) {
                word = (const char*)res->data;
            } else if (!wake_words_.empty()) {
                word = wake_words_[0].c_str();
            }

            if (on_detected_cb_) {
                on_detected_cb_(std::string(word));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "Detection task exiting");
}

esp_err_t AudioWakeWord::feed(const int16_t* pcm_16k_mono, size_t samples) {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioWakeWordImpl*)impl_;
    size_t need_bytes = (size_t)impl->feed_chunk_bytes;
    if (samples * sizeof(int16_t) != need_bytes) {
        ESP_LOGE(TAG, "feed size mismatch: got %zu samples, need %d bytes",
                 samples, impl->feed_chunk_bytes);
        return ESP_ERR_INVALID_ARG;
    }
    impl->iface->feed(impl->data, pcm_16k_mono);
    return ESP_OK;
}

size_t AudioWakeWord::get_feed_chunk_samples() const {
    return (size_t)feed_chunk_bytes_ / sizeof(int16_t);
}
