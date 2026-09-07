/*
 * Audio AFE Processor — ESP-SR AFE 实现 (P1)
 *
 * 基于 xiaozhi p4c5 main/audio/processors/afe_audio_processor.cc 简化 (无 C++ STL).
 * PIMPL 模式隐藏 ESP-SR 类型, header 不暴露 esp_afe_sr_iface_t.
 */
#include "audio_afe_processor.h"
#include "esp_log.h"
#include <cstring>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"  /* esp-sr v2.3.x: srmodel API moved from esp_srmodel.h to model_path.h */

static const char* TAG = "AFEProc";

/* ── PIMPL 实现结构体 (cpp 内私有) ── */
struct AudioAfeImpl {
    const esp_afe_sr_iface_t* iface = nullptr;
    esp_afe_sr_data_t*        data  = nullptr;
    srmodel_list_t*           models = nullptr;
    int feed_chunk_bytes  = 0;
    int fetch_chunk_bytes = 0;

    ~AudioAfeImpl() {
        if (data != nullptr && iface != nullptr) iface->destroy(data);
        if (models != nullptr) esp_srmodel_deinit(models);
    }
};

AudioAfeProcessor::AudioAfeProcessor() {}

AudioAfeProcessor::~AudioAfeProcessor() {
    delete (AudioAfeImpl*)impl_;
}

esp_err_t AudioAfeProcessor::init(const char* input_format,
                                   const char* model_partition,
                                   bool enable_aec,
                                   bool enable_ns,
                                   bool enable_vad,
                                   bool enable_wake)
{
    if (impl_ != nullptr) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    AudioAfeImpl* impl = new AudioAfeImpl();
    impl_ = impl;

    impl->models = esp_srmodel_init(model_partition);
    if (impl->models == nullptr) {
        ESP_LOGE(TAG, "esp_srmodel_init('%s') failed. "
                      "请确认 partitions.csv 有 model 分区且已烧录 ESP-SR 模型",
                 model_partition);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Loaded %d models from '%s' partition:", impl->models->num, model_partition);
    for (int i = 0; i < impl->models->num; i++) {
        ESP_LOGI(TAG, "  model[%d]: %s", i, impl->models->model_name[i]);
    }

    char* ns_model_name  = esp_srmodel_filter(impl->models, ESP_NSNET_PREFIX, NULL);
    char* vad_model_name = esp_srmodel_filter(impl->models, ESP_VADN_PREFIX, NULL);

    afe_config_t* afe_cfg = afe_config_init(
        input_format,
        impl->models,
        AFE_TYPE_VC,
        AFE_MODE_HIGH_PERF);
    if (afe_cfg == nullptr) {
        ESP_LOGE(TAG, "afe_config_init failed");
        return ESP_ERR_NO_MEM;
    }

    afe_cfg->aec_init = enable_aec;
    if (enable_aec) {
        afe_cfg->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    }

    if (enable_ns && ns_model_name != nullptr) {
        afe_cfg->ns_init = true;
        afe_cfg->ns_model_name = ns_model_name;
        afe_cfg->afe_ns_mode = AFE_NS_MODE_NET;
    } else {
        afe_cfg->ns_init = false;
        ESP_LOGW(TAG, "NS disabled (ns_model=%p)", ns_model_name);
    }

    afe_cfg->vad_init = enable_vad;
    afe_cfg->vad_mode = VAD_MODE_0;
    afe_cfg->vad_min_noise_ms = 100;
    if (enable_vad && vad_model_name != nullptr) {
        afe_cfg->vad_model_name = vad_model_name;
    }

    afe_cfg->agc_init = false;
    afe_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_cfg->wakenet_init = enable_wake;

    impl->iface = esp_afe_handle_from_config(afe_cfg);
    if (impl->iface == nullptr) {
        ESP_LOGE(TAG, "esp_afe_handle_from_config failed");
        return ESP_ERR_NOT_SUPPORTED;
    }
    impl->data = impl->iface->create_from_config(afe_cfg);
    if (impl->data == nullptr) {
        ESP_LOGE(TAG, "iface->create_from_config failed");
        return ESP_ERR_NO_MEM;
    }

    impl->feed_chunk_bytes  = impl->iface->get_feed_chunksize(impl->data);
    impl->fetch_chunk_bytes = impl->iface->get_fetch_chunksize(impl->data);
    feed_chunk_bytes_  = impl->feed_chunk_bytes;
    fetch_chunk_bytes_ = impl->fetch_chunk_bytes;

    ESP_LOGI(TAG, "AFE init OK: input_format=%s AEC=%d NS=%d VAD=%d Wake=%d",
             input_format, enable_aec, enable_ns, enable_vad, enable_wake);
    ESP_LOGI(TAG, "  feed_chunk=%d bytes, fetch_chunk=%d bytes",
             feed_chunk_bytes_, fetch_chunk_bytes_);

    return ESP_OK;
}

esp_err_t AudioAfeProcessor::feed(const int16_t* in_4ch, size_t samples) {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;
    size_t need_bytes = (size_t)impl->feed_chunk_bytes;
    if (samples * sizeof(int16_t) != need_bytes) {
        ESP_LOGE(TAG, "feed size mismatch: got %zu samples (%zu bytes), need %d bytes",
                 samples, samples * sizeof(int16_t), impl->feed_chunk_bytes);
        return ESP_ERR_INVALID_ARG;
    }
    impl->iface->feed(impl->data, in_4ch);
    return ESP_OK;
}

esp_err_t AudioAfeProcessor::fetch(int16_t* out_pcm, size_t out_buf_size,
                                    size_t* out_samples,
                                    audio_afe_vad_state_t* out_vad)
{
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;

    afe_fetch_result_t* res = impl->iface->fetch_with_delay(impl->data, portMAX_DELAY);
    if (res == nullptr || res->ret_value == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (res->data == nullptr || res->data_size == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t samples = res->data_size / sizeof(int16_t);
    if (samples > out_buf_size) {
        samples = out_buf_size;
    }
    std::memcpy(out_pcm, res->data, samples * sizeof(int16_t));
    *out_samples = samples;
    *out_vad = (res->vad_state == VAD_SPEECH) ? AFE_VAD_SPEECH_P4 : AFE_VAD_SILENCE_P4;
    return ESP_OK;
}

esp_err_t AudioAfeProcessor::fetch_immediate(int16_t* out_pcm, size_t out_buf_size,
                                              size_t* out_samples,
                                              audio_afe_vad_state_t* out_vad)
{
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;

    afe_fetch_result_t* res = impl->iface->fetch(impl->data);
    if (res == nullptr || res->ret_value == ESP_FAIL) {
        return ESP_FAIL;
    }
    if (res->data == nullptr || res->data_size == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t samples = res->data_size / sizeof(int16_t);
    if (samples > out_buf_size) samples = out_buf_size;
    std::memcpy(out_pcm, res->data, samples * sizeof(int16_t));
    *out_samples = samples;
    *out_vad = (res->vad_state == VAD_SPEECH) ? AFE_VAD_SPEECH_P4 : AFE_VAD_SILENCE_P4;
    return ESP_OK;
}

esp_err_t AudioAfeProcessor::enable_aec(bool enable) {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;
    if (enable) impl->iface->enable_aec(impl->data);
    else        impl->iface->disable_aec(impl->data);
    ESP_LOGI(TAG, "AEC %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

esp_err_t AudioAfeProcessor::enable_vad(bool enable) {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;
    if (enable) impl->iface->enable_vad(impl->data);
    else        impl->iface->disable_vad(impl->data);
    ESP_LOGI(TAG, "VAD %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

esp_err_t AudioAfeProcessor::reset() {
    if (impl_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto* impl = (AudioAfeImpl*)impl_;
    impl->iface->reset_buffer(impl->data);
    return ESP_OK;
}

size_t AudioAfeProcessor::get_feed_chunk_samples() const {
    return (size_t)feed_chunk_bytes_ / sizeof(int16_t);
}

size_t AudioAfeProcessor::get_fetch_chunk_samples() const {
    return (size_t)fetch_chunk_bytes_ / sizeof(int16_t);
}
