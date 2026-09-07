/**
 * @file app_main.c
 * @brief p4c5-agent-terminal 固件入口 (v0.1.0)
 *
 * 初始化顺序（按硬件依赖）：
 *   Board → PMIC → Display → Audio → 4G → dsh_client
 *
 * M9: 集成 dsh_client（JSON-over-WS 协议层）+ 4G ML307 transport
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "p4c5_board.h"
#include "p4c5_pmic.h"
#include "p4c5_display.h"
#include "p4c5_audio.h"
// #include "p4c5_4g.h"  // W1: 4G 搁置 (需电池), 改用 WiFi
#include "p4c5_ui.h"
#include "dsh_client.h"
#include "wifi_manager.h"
#include "audio_mixer.h"           /* W3: 4ch→1ch 混音 (OMT 移植) */
#include "driver/uart.h"           /* v8 修复: uart_set_pin() */
#include "driver/gpio.h"           /* v9 诊断: gpio_get_level() */
#include "aec_sw.h"                /* W3: 软件 AEC 回声消除 (NLMS, P4 自实现, P1 后保留作为 fallback) */
#include "resampler_24_16.h"       /* W3: 24kHz→16kHz 重采样 (P1 后保留作为 fallback) */
#include "audio_afe_processor.h"   /* P1: ESP-SR AFE (AEC + NS + VAD 神经网络) */
#include "audio_wake_word.h"       /* P1: ESP-SR WakeNet9 唤醒词检测 */
#include "opus_encoder.h"          /* W3: libopus 编码 (16kbps, 20ms 帧) */
#include "tts_player.h"            /* W3: TTS 下行 PCM 播放 (WS Binary → ES8311 DAC) */
#include "wake_word_detector.h"    /* W4: P4 自实现 RMS-based 唤醒检测 */
#include "config.h"
#include <string.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include "esp_console.h"  /* ESP-IDF 官方 console 组件 (v5.5) */
#include "driver/uart_vfs.h"  /* uart_vfs_dev_use_driver (v5.5 新 API, 路径 driver/) */
#include "esp_vfs_dev.h"  /* ESP_LINE_ENDINGS_CR 等常量 (备) */

static const char *TAG = "app_main";

/* W2: 收到的最后一个 tool_call id (用于回 tool_result) */
static char w2_last_tool_id[64] = {0};

/* W5: assistant_text 累积 buffer (assistant_done 时刷到 p4c5_ui_set_message) */
static char s_assistant_text_buf[512] = {0};

/* W3: 录音门控 (POC: 串口命令触发, 类似 OMT `s_app_recording`) */
static volatile bool s_audio_recording = false;

/* W4: 唤醒检测使能 (POC: 串口命令 wake enable/disable) */
static volatile bool s_wake_enabled = false;

/* ── DSH 帧回调 (W2: 处理全部 14 类下行帧) ── */
static void on_dsh_frame(const char *frame_type, cJSON *json, void *user_data)
{
    if (strcmp(frame_type, "session_state") == 0) {
        cJSON *state = cJSON_GetObjectItem(json, "state");
        ESP_LOGI(TAG, "📡 [down] session_state: %s",
                 cJSON_IsString(state) ? state->valuestring : "unknown");

    } else if (strcmp(frame_type, "thinking") == 0) {
        ESP_LOGI(TAG, "🤔 [down] thinking...");

    } else if (strcmp(frame_type, "assistant_text") == 0) {
        cJSON *delta = cJSON_GetObjectItem(json, "content");
        if (cJSON_IsString(delta)) {
            printf("%s", delta->valuestring);
            fflush(stdout);
        } else {
            cJSON *d = cJSON_GetObjectItem(json, "delta");
            if (cJSON_IsString(d)) { printf("%s", d->valuestring); fflush(stdout); }
        }
        /* W5: 累积文字到 s_assistant_text_buf, assistant_done 时刷新到 UI */
        const char *text = NULL;
        cJSON *content_obj = cJSON_GetObjectItem(json, "content");
        if (cJSON_IsString(content_obj)) text = content_obj->valuestring;
        else {
            cJSON *d2 = cJSON_GetObjectItem(json, "delta");
            if (cJSON_IsString(d2)) text = d2->valuestring;
        }
        if (text) {
            size_t cur_len = strlen(s_assistant_text_buf);
            size_t add_len = strlen(text);
            if (cur_len + add_len < sizeof(s_assistant_text_buf) - 1) {
                strcat(s_assistant_text_buf, text);
            }
        }

    } else if (strcmp(frame_type, "assistant_done") == 0) {
        printf("\n");
        ESP_LOGI(TAG, "📝 [down] assistant_done (%u bytes)", strlen(s_assistant_text_buf));
        /* W5: 调 p4c5_ui_set_message 把累积文字推到屏幕 */
        if (strlen(s_assistant_text_buf) > 0) {
            p4c5_ui_set_message(s_assistant_text_buf);
            s_assistant_text_buf[0] = '\0';  // reset for next assistant turn
        }

    } else if (strcmp(frame_type, "tool_call") == 0) {
        cJSON *name = cJSON_GetObjectItem(json, "tool_name");
        if (!cJSON_IsString(name)) name = cJSON_GetObjectItem(json, "name");
        cJSON *id = cJSON_GetObjectItem(json, "id");
        /* W2: 保存 id 用于回 tool_result */
        if (cJSON_IsString(id)) {
            strncpy(w2_last_tool_id, id->valuestring, sizeof(w2_last_tool_id) - 1);
            w2_last_tool_id[sizeof(w2_last_tool_id) - 1] = '\0';
        }
        ESP_LOGI(TAG, "🔧 [down] tool_call: %s (id=%s)",
                 cJSON_IsString(name) ? name->valuestring : "?",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "tool_result") == 0) {
        cJSON *name = cJSON_GetObjectItem(json, "name");
        cJSON *status = cJSON_GetObjectItem(json, "status");
        ESP_LOGI(TAG, "🔧 [down] tool_result: %s status=%s",
                 cJSON_IsString(name) ? name->valuestring : "?",
                 cJSON_IsString(status) ? status->valuestring : "?");

    } else if (strcmp(frame_type, "tool_progress") == 0) {
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *elapsed = cJSON_GetObjectItem(json, "elapsed_seconds");
        ESP_LOGI(TAG, "⏳ [down] tool_progress: id=%s elapsed=%s",
                 cJSON_IsString(id) ? id->valuestring : "?",
                 cJSON_IsNumber(elapsed) ? "yes" : "?");

    } else if (strcmp(frame_type, "file_change") == 0) {
        cJSON *path = cJSON_GetObjectItem(json, "path");
        cJSON *change = cJSON_GetObjectItem(json, "change_type");
        ESP_LOGI(TAG, "📁 [down] file_change: %s (%s)",
                 cJSON_IsString(path) ? path->valuestring : "?",
                 cJSON_IsString(change) ? change->valuestring : "?");

    } else if (strcmp(frame_type, "authorization/requested") == 0) {
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *tool = cJSON_GetObjectItem(json, "tool_name");
        ESP_LOGI(TAG, "🔐 [down] auth_requested: %s (id=%s)",
                 cJSON_IsString(tool) ? tool->valuestring : "?",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "authorization/resolved") == 0) {
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *decision = cJSON_GetObjectItem(json, "decision");
        ESP_LOGI(TAG, "🔐 [down] auth_resolved: %s (id=%s)",
                 cJSON_IsString(decision) ? decision->valuestring : "?",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "question/requested") == 0) {
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *q = cJSON_GetObjectItem(json, "question");
        ESP_LOGI(TAG, "❓ [down] question_requested: %s (id=%s)",
                 cJSON_IsString(q) ? q->valuestring : "?",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "question/resolved") == 0) {
        cJSON *id = cJSON_GetObjectItem(json, "id");
        ESP_LOGI(TAG, "❓ [down] question_resolved (id=%s)",
                 cJSON_IsString(id) ? id->valuestring : "?");

    } else if (strcmp(frame_type, "file_upload") == 0) {
        cJSON *uid = cJSON_GetObjectItem(json, "upload_id");
        cJSON *url = cJSON_GetObjectItem(json, "target_url");
        ESP_LOGI(TAG, "📤 [down] file_upload: %s → %s",
                 cJSON_IsString(uid) ? uid->valuestring : "?",
                 cJSON_IsString(url) ? url->valuestring : "?");

    } else if (strcmp(frame_type, "system_event") == 0) {
        cJSON *event = cJSON_GetObjectItem(json, "event");
        ESP_LOGI(TAG, "⚙️ [down] system_event: %s",
                 cJSON_IsString(event) ? event->valuestring : "?");

    } else {
        ESP_LOGW(TAG, "❓ [down] unknown frame: %s", frame_type);
    }
}

/* ── DSH 状态回调 ── */
static void on_dsh_state(dsh_client_event_t event, void *user_data)
{
    switch (event) {
        case DSH_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🟢 DSH connected");
            break;
        case DSH_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "🔴 DSH disconnected");
            break;
        case DSH_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ DSH error");
            break;
        case DSH_EVENT_RECONNECTING:
            ESP_LOGW(TAG, "🔄 DSH reconnecting...");
            break;
    }
}

/* ─ DSH 状态提供者（电池 + WiFi RSSI） ── */
static void status_provider(int *battery, int *rssi, void *user_data)
{
    *battery = (int)p4c5_pmic_get_battery_level();
    // *rssi = p4c5_4g_get_rssi_dbm();  // W1: 4G 搁置
    *rssi = -99;  // TODO: 从 wifi_manager 获取 RSSI
}

/* ── 4G 事件回调 (W1: 4G 搁置，暂时注释) ── */
/*
static void on_4g_event(p4c5_4g_event_t event, const char *data, void *user_data)
{
    switch (event) {
        case P4C5_4G_EVENT_MODEM_DETECTING:
            ESP_LOGI(TAG, "📱 Modem detecting...");
            break;
        case P4C5_4G_EVENT_MODEM_FOUND:
            ESP_LOGI(TAG, "📱 Modem found: %s", p4c5_4g_get_module_revision());
            break;
        case P4C5_4G_EVENT_REGISTERING:
            ESP_LOGI(TAG, "📱 Registering network...");
            break;
        case P4C5_4G_EVENT_NETWORK_READY:
            ESP_LOGI(TAG, "📱 Network ready! Carrier=%s, IMEI=%s",
                     p4c5_4g_get_carrier(), p4c5_4g_get_imei());
            break;
        case P4C5_4G_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "📱 Network disconnected");
            break;
        case P4C5_4G_EVENT_NO_SIM:
            ESP_LOGE(TAG, "📱 No SIM card!");
            break;
        case P4C5_4G_EVENT_REG_DENIED:
            ESP_LOGE(TAG, "📱 Registration denied");
            break;
        case P4C5_4G_ERROR_INIT_FAILED:
            ESP_LOGE(TAG, "📱 Modem init failed");
            break;
        case P4C5_4G_ERROR_TIMEOUT:
            ESP_LOGW(TAG, "📱 Network timeout");
            break;
    }
}
*/

/* ══════════════════════════════════════════════════════════
 * W3 Phase 1: 音频上行链路 (OMT 等同功能) - 含软件 AEC
 *
 *   链路: p4c5_audio_record_multi (4ch 24kHz)
 *       → 4 通道分离: ch0+ch2 → mic, ch1 → AEC ref, ch3 → unused
 *       → aec_sw (NLMS 回声消除, ch1 作为参考)  [POC: 简化版]
 *       → audio_mixer (若需要, 单声道已分好)
 *       → resampler_24_16 (24k→16k)
 *       → opus_encoder_encode (16kbps, 20ms 帧)
 *       → dsh_client_send_audio (WS Binary 帧)
 *
 *   触发: s_audio_recording = true (通过串口命令 audio start/stop)
 *
 *   AEC 限制 (诚实):
 *     - ESP32-P4 不支持 ESP-SR AFE 硬件加速 (仅 ESP32-S3 可用)
 *     - 自实现 NLMS 是 POC 简化版, 质量 < ESP-SR AFE
 *     - 仅当硬件真有 AEC ref 回采 (ES8311 输出 → ES7210 MIC2) 时有效
 *     - 否则 ch1 实际是 MIC2, AEC 处理无意义
 *
 *   参考开源案例:
 *     - xiaozhi ESP-SR AFE: github.com/espressif/esp-sr
 *     - speexdsp AEC: 嵌入式首选 (但 ~50KB ROM, P4 资源有限)
 *     - WebRTC APM: 最佳但 ~200KB RAM, P4 不适合
 * ══════════════════════════════════════════════════════════ */

static void audio_uplink_task(void *arg)
{
    (void)arg;

    /* P1 帧规格 (ESP-SR AFE 1MIC 期望 16kHz × 32ms × 2ch layout = 1024 samples)
     *
     * 我们的硬件: ES7210 输出 4ch 24kHz TDM
     *   - 32ms @ 24kHz = 768 samples/ch (3072 samples total)
     *
     * 我们手动:
     *   1. 提取 ch0 (mic) + ch1 (ref) → 2ch 24kHz 1536 samples
     *   2. resample 24k→16k: 1536 → 1024 samples (2ch × 16kHz × 32ms)
     *   3. 喂 AFE: 1024 samples (16kHz × 32ms × 2ch [mic,ref])
     *   4. fetch AFE: 512 samples (16kHz × 32ms × 1ch mono)
     *   5. Opus encode → WS send
     */
    constexpr size_t FRAME_SAMPLES_24K = 768;       /* 24kHz × 32ms = 768 samples/ch */

    static int16_t s_in4ch_buf[768 * 4];            /* 4ch × 768 = 3072 samples */
    static int16_t s_2ch_16k_buf[1024];             /* AFE 输入: mic+ref 16kHz 2ch */
    static int16_t s_out_16k_mono[1024];            /* AFE 输出: 16kHz mono */
    static uint8_t s_opus_buf[1276];                /* OPUS max packet */
    static int16_t s_pcm_acc[1024];                 /* PCM 累积 buffer (opus 60ms = 960 samples) */
    static size_t  s_pcm_acc_n = 0;                 /* 已累积 samples 数 */

    /* ── P1: 初始化 ESP-SR AFE (1MIC 模式, mic+ref 2ch layout) ── */
    AudioAfeProcessor afe;
    esp_err_t afe_ret = afe.init(
        "MR",                                       /* input_format: mic + ref, 2 channels */
        "model",                                    /* srmodels 分区名 */
        P4C5_AUDIO_INPUT_REF,                       /* AEC 开 (有 ref) */
        true,                                       /* NS 开 (NSNet2 噪声抑制) */
        true,                                       /* VAD 开 (VADNet1) */
        false                                       /* Wake 不在 AFE_VC, 走 audio_wake_word 组件 */
    );
    if (afe_ret != ESP_OK) {
        ESP_LOGE("audio_uplink", "❌ ESP-SR AFE init failed: %s, abort",
                 esp_err_to_name(afe_ret));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI("audio_uplink", "  ✅ ESP-SR AFE ready (AEC=ON, NS=ON, VAD=ON)");

    /* ── Opus 编码器 (P1: AFE 输出 512 samples / 32ms, 用 60ms frame 累积编码) ── */
    audio::OpusEncoderConfig opus_cfg = audio::opus_encoder_default_config();
    opus_cfg.frame_ms = 60;   /* 60ms = 960 samples (累积 2 次 AFE fetch = 1024 samples) */
    ESP_ERROR_CHECK(audio::opus_encoder_init(opus_cfg));

    /* esp-sr get_feed_chunksize 返回 aec_frame_size (per-channel samples @ 16kHz)
     * AFE 期望 input layout: [mic0, ref0, mic1, ref1, ...]
     * total samples per feed = aec_frame_size × input_channels (mic + ref = 2)
     *
     * fetch_chunksize × 1 ch (mono output) */
    constexpr size_t AFE_FEED_SAMPLES  = 512 * 2;  /* 16kHz × 32ms × 2ch (mic+ref) = 1024 */
    constexpr size_t AFE_FETCH_SAMPLES = 512;      /* 16kHz × 32ms × 1ch mono */

    /* ── P1: 初始化 ESP-SR WakeNet9 (替代自实现 RMS wake_word_detector) ── */
    /* P1 v3: 临时禁用 WakeNet (吃太多内部 RAM, 导致 LVGL + esp_hosted OOM)
     * 设为 1 启用 WakeNet9; 设为 0 用旧 RMS wake (W4) */
#define P4C5_USE_WAKE_NET9 0
    AudioWakeWord wake_word;
#if P4C5_USE_WAKE_NET9
    bool wake_init_ok = (wake_word.init("MR", "model", nullptr) == ESP_OK);
#else
    bool wake_init_ok = false;
    ESP_LOGW("audio_uplink", "  ⚠️ WakeNet9 disabled (P4C5_USE_WAKE_NET9=0, 用 W4 RMS wake)");
#endif
    if (wake_init_ok) {
        wake_word.on_detected([](const std::string& word) {
            ESP_LOGW("audio_uplink", "🌟 WakeNet9 检测到唤醒词: '%s'", word.c_str());
            /* TODO: 唤醒时切到 RECORDING 状态, 启动 mic 上行 */
        });
        wake_word.start();
        ESP_LOGI("audio_uplink", "  ✅ WakeNet9 ready (检测 '嗨乐鑫' 触发唤醒)");
    } else {
        ESP_LOGW("audio_uplink", "  ⚠️ WakeNet9 init failed, 上行仅手动模式");
    }

    ESP_LOGI("audio_uplink", "P1 音频上行链路启动 (ESP-SR AFE + WakeNet9)");
    ESP_LOGI("audio_uplink", "  4ch@24kHz TDM (ch0=mic, ch1=AEC_ref, ch2/ch3=unused)");
    ESP_LOGI("audio_uplink", "  → ch0+ch1 提取 (2ch) → 24k→16k resample (1536→1024)");
    ESP_LOGI("audio_uplink", "  → ESP-SR AFE (AEC + NS + VAD 神经网络) → 16k mono");
    ESP_LOGI("audio_uplink", "  → opus → WS Binary 上行");
    ESP_LOGI("audio_uplink", "触发方式:");
    ESP_LOGI("audio_uplink", "  - 串口 'audio start/stop' (手动)");
    ESP_LOGI("audio_uplink", "  - '嗨乐鑫' 唤醒词 (P1: WakeNet9)");

    uint64_t frame_count  = 0;
    uint64_t sent_count   = 0;
    bool     is_uploading = false;

    for (;;) {
        /* 总门控: 必须 audio_recording 或 wake_enabled 任一开启 */
        if (!s_audio_recording && !s_wake_enabled) {
            if (is_uploading) {
                ESP_LOGI("audio_uplink", "🛑 停止上行 (门控关闭)");
                p4c5_audio_enable_input(false);  /* v11: 释放 ADC */
                is_uploading = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* v11 修复: 启动录音前必须 enable ADC input (否则 ESP_ERR_INVALID_STATE).
         * 之前 audio_uplink_task 只调 record_multi() 但 s_input_enabled=false,
         * 导致 codec_dev 没 open, 一直返回 INVALID_STATE.
         */
        if (!is_uploading) {
            esp_err_t en_ret = p4c5_audio_enable_input(true);
            if (en_ret != ESP_OK) {
                ESP_LOGE("audio_uplink", "enable_input(true) failed: %s",
                         esp_err_to_name(en_ret));
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            ESP_LOGI("audio_uplink", "🎙️ 启动上行 (ADC 4ch enabled)");
            is_uploading = true;
        }

        /* 1. 4 通道 24kHz 录音 (P1: 32ms = 768 samples/ch) */
        esp_err_t ret = p4c5_audio_record_multi(s_in4ch_buf, FRAME_SAMPLES_24K);
        if (ret != ESP_OK) {
            if ((frame_count % 250) == 0) {
                ESP_LOGW("audio_uplink", "p4c5_audio_record_multi 失败: %s", esp_err_to_name(ret));
            }
            vTaskDelay(pdMS_TO_TICKS(32));
            continue;
        }

        /* 2. P1: 通道提取 ch0 (mic) + ch1 (ref) → 2ch 24kHz 1536 samples
         *    TDM 帧布局: [ch0_s0, ch1_s0, ch2_s0, ch3_s0, ch0_s1, ch1_s1, ...] */
        for (size_t i = 0; i < FRAME_SAMPLES_24K; i++) {
            int16_t mic_s = s_in4ch_buf[i * 4 + 0];   /* ch0 = mic */
            int16_t ref_s = s_in4ch_buf[i * 4 + 1];   /* ch1 = AEC ref */
            s_2ch_16k_buf[i * 2 + 0] = mic_s;          /* mic (interleaved, 2ch) */
            s_2ch_16k_buf[i * 2 + 1] = ref_s;          /* ref */
        }

        /* 3. P1: 24kHz → 16kHz 重采样 (1536 → 1024 samples, 线性插值)
         *    简单线性插值: out[i] = in[i * 24/16] = in[i * 3/2] */
        /*    1024 output samples × 2 channels = 2048 samples
         *    从 1536 × 2 = 3072 input samples
         *    resample 后放到 s_2ch_16k_buf (overwrite) */
        static int16_t s_2ch_16k_resampled[2048];     /* 1024 frames × 2ch */
        for (size_t i = 0; i < 1024; i++) {
            /* 24→16 resample: 16kHz 1024 samples 对应 24kHz 1536 samples
             * ratio = 24/16 = 1.5
             * 16kHz sample i 对应 24kHz sample i * 1.5 */
            size_t idx_24k = (i * 24) / 16;            /* = i * 1.5 */
            if (idx_24k >= FRAME_SAMPLES_24K) idx_24k = FRAME_SAMPLES_24K - 1;
            s_2ch_16k_resampled[i * 2 + 0] = s_2ch_16k_buf[idx_24k * 2 + 0];  /* mic */
            s_2ch_16k_resampled[i * 2 + 1] = s_2ch_16k_buf[idx_24k * 2 + 1];  /* ref */
        }

        /* 4. P1: 喂 ESP-SR AFE (2ch 16kHz, 32ms = 1024 samples) */
        esp_err_t feed_ret = afe.feed(s_2ch_16k_resampled, AFE_FEED_SAMPLES);
        if (feed_ret != ESP_OK) {
            ESP_LOGW("audio_uplink", "AFE feed failed: %s", esp_err_to_name(feed_ret));
            frame_count++;
            continue;
        }

        /* P1: WakeNet9 同时检测唤醒词 */
        if (wake_init_ok && s_wake_enabled && !s_audio_recording) {
            int16_t wake_pcm[AFE_FETCH_SAMPLES];
            size_t  wake_n = 0;
            audio_afe_vad_state_t wake_vad = AFE_VAD_SILENCE_P4;
            if (afe.fetch_immediate(wake_pcm, AFE_FETCH_SAMPLES, &wake_n, &wake_vad) == ESP_OK) {
                if (wake_n > 0) {
                    wake_word.feed(wake_pcm, wake_n);
                }
            }
        }

        /* 5. 不上行时, 只录音不上行 (节省带宽 + 等待唤醒) */
        if (!is_uploading && !s_audio_recording) {
            frame_count++;
            continue;
        }

        /* 6. fetch AFE 输出 (16kHz mono 32ms = 512 samples) */
        size_t out_samples = 0;
        audio_afe_vad_state_t vad_state = AFE_VAD_SILENCE_P4;
        esp_err_t fetch_ret = afe.fetch_immediate(
            s_out_16k_mono, sizeof(s_out_16k_mono) / sizeof(int16_t),
            &out_samples, &vad_state);
        if (fetch_ret != ESP_OK || out_samples == 0) {
            frame_count++;
            continue;
        }

        /* 7. 累积 PCM 到 60ms (opus 60ms frame = 960 samples)
         *    2 次 fetch (512 each) 凑齐 1024 > 960, 编码 960 留 64 继续累积 */
        if (s_pcm_acc_n + out_samples < 960) {
            /* 还不满 1 个 opus frame */
            memcpy(s_pcm_acc + s_pcm_acc_n, s_out_16k_mono, out_samples * sizeof(int16_t));
            s_pcm_acc_n += out_samples;
            frame_count++;
            continue;
        }

        /* 凑齐 960 samples: 编码前 960 samples, 留剩下的 */
        size_t need       = 960;
        size_t take_from_acc = (s_pcm_acc_n >= need) ? need : s_pcm_acc_n;
        memcpy(s_pcm_acc + s_pcm_acc_n, s_out_16k_mono, (need - take_from_acc) * sizeof(int16_t));
        s_pcm_acc_n = take_from_acc;  /* 累积数 = take_from_acc (因为加了 need-take_from_acc) */
        size_t encode_n = need;

        /* 8. Opus 编码 60ms */
        size_t opus_len = sizeof(s_opus_buf);
        ret = audio::opus_encoder_encode(s_pcm_acc, encode_n,
                                          s_opus_buf, &opus_len);
        if (ret != ESP_OK) {
            frame_count++;
            continue;
        }

        /* 把剩下的 samples 移到 buffer 头 */
        if (s_pcm_acc_n > encode_n) {
            memmove(s_pcm_acc, s_pcm_acc + encode_n, (s_pcm_acc_n - encode_n) * sizeof(int16_t));
            s_pcm_acc_n -= encode_n;
        } else {
            s_pcm_acc_n = 0;
        }

        /* 8. WS Binary 帧上行 */
        esp_err_t send_ret = dsh_client_send_audio(s_opus_buf, opus_len);
        if (send_ret == ESP_OK) {
            sent_count++;
            if ((sent_count % 250) == 0) {
                ESP_LOGI("audio_uplink", "已发送 %llu 帧 (opus %u bytes/帧, VAD=%s, wake=%s)",
                         (unsigned long long)sent_count, (unsigned)opus_len,
                         vad_state == AFE_VAD_SPEECH_P4 ? "SPEECH" : "SILENCE",
                         s_wake_enabled ? "WN9" : "MAN");
            }
        }

        /* 7. 实时音频诊断 - 每 50 帧 (~1s) 输出 mic/ref/aec RMS
         *    便于调试 AEC 收敛 (期望 mic_RMS ≈ aec_RMS << ref_RMS 时回声消除成功)
         */
        frame_count++;
        if ((frame_count % 50) == 0) {
            auto calc_rms = [](const int16_t *buf, size_t n) -> uint32_t {
                uint64_t sum_sq = 0;
                for (size_t i = 0; i < n; i++) {
                    int32_t v = buf[i];
                    sum_sq += (uint64_t)(v * v);
                }
                uint32_t mean = (uint32_t)(sum_sq / n);
                /* 整数平方根 (牛顿法) */
                uint32_t x = mean;
                uint32_t y = (x + 1) >> 1;
                while (y < x) {
                    x = y;
                    y = (x + mean / x) >> 1;
                }
                return x;
            };
            uint32_t mic_rms = calc_rms(s_in4ch_buf, FRAME_SAMPLES_24K * 4);
            uint32_t out_rms = calc_rms(s_out_16k_mono, out_samples);
            ESP_LOGI("audio_diag",
                     "frame=%llu mic4ch=%u out16k=%u VAD=%s "
                     "(P1: ESP-SR AFE AEC/NS/VAD 全开)",
                     (unsigned long long)frame_count,
                     (unsigned)mic_rms, (unsigned)out_rms,
                     vad_state == AFE_VAD_SPEECH_P4 ? "SPEECH" : "SILENCE");
        }
    }
}

/* ── 串口命令处理 (POC: 解析 audio start/stop + wake enable)
 *
 * v2 修复: 改用 ESP-IDF 官方 esp_console 组件
 *   - 参考: esp-idf-v5.5.5/components/console/esp_console.h
 *   - 参考: esp-idf-v5.5.5/examples/system/console/basic/main/console_example_main.c
 *
 * 之前的 select + fcntl O_NONBLOCK + fgetc 方案不可靠
 * (VFS UART 默认行缓冲, select 在某些条件下返回 false negative).
 *
 * 现在:
 *   - esp_console_init() 初始化 linenoise + arg parser
 *   - esp_console_cmd_register() 注册每条命令
 *   - 主循环读取 UART 行 → esp_console_run() 执行
 */
static int cmd_audio_start(int argc, char **argv)
{
    s_audio_recording = true;
    s_wake_enabled = false;  /* 手动模式关掉 wake */
    ESP_LOGI(TAG, "audio recording ON");
    return 0;
}

static int cmd_audio_stop(int argc, char **argv)
{
    s_audio_recording = false;
    ESP_LOGI(TAG, "audio recording OFF");
    return 0;
}

/* W5: UI 公开 API - 让 p4c5_ui.c 的按钮回调触发 audio_uplink */
extern "C" void p4c5_ui_btn_talk_press(void)
{
    s_audio_recording = true;
    s_wake_enabled = false;
    ESP_LOGI(TAG, "🎙 [UI] Hold to Talk PRESSED → audio recording ON");
}

extern "C" void p4c5_ui_btn_talk_release(void)
{
    s_audio_recording = false;
    ESP_LOGI(TAG, "🎙 [UI] Hold to Talk RELEASED → audio recording OFF");
}

static int cmd_audio_status(int argc, char **argv)
{
    ESP_LOGI(TAG, "audio=%s wake=%s",
             s_audio_recording ? "ON" : "OFF",
             s_wake_enabled ? "EN" : "DIS");
    return 0;
}

static int cmd_tts_stats(int argc, char **argv)
{
    audio::tts_player_print_stats();
    return 0;
}

static int cmd_wake_enable(int argc, char **argv)
{
    s_wake_enabled = true;
    s_audio_recording = false;  /* wake 模式自动控制 */
    ESP_LOGI(TAG, "wake ENABLED");
    return 0;
}

static int cmd_wake_disable(int argc, char **argv)
{
    s_wake_enabled = false;
    ESP_LOGI(TAG, "wake DISABLED");
    return 0;
}

static int cmd_wake_status(int argc, char **argv)
{
    ESP_LOGI(TAG, "wake=%s audio=%s",
             s_wake_enabled ? "EN" : "DIS",
             s_audio_recording ? "ON" : "OFF");
    audio::wake_word_detector_print_stats();
    return 0;
}

/* 注册所有串口命令 (参考 esp_console 官方 API) */
static void register_uart_commands(void)
{
    esp_console_register_help_command();   /* 'help' 命令 */

    const esp_console_cmd_t audio_cmds[] = {
        {.command = "audio", .help = "Audio subcommands: start/stop/status",
         .func = NULL, .func_w_context = NULL},  /* 占位 */
    };
    (void)audio_cmds;

    /* audio start */
    const esp_console_cmd_t cmd_a_start = {
        .command = "audio_start",
        .help = "Start audio recording (4ch@24k + AEC + opus uplink)",
        .func = cmd_audio_start,
    };
    esp_console_cmd_register(&cmd_a_start);

    /* audio stop */
    const esp_console_cmd_t cmd_a_stop = {
        .command = "audio_stop",
        .help = "Stop audio recording",
        .func = cmd_audio_stop,
    };
    esp_console_cmd_register(&cmd_a_stop);

    /* audio status */
    const esp_console_cmd_t cmd_a_status = {
        .command = "audio_status",
        .help = "Show audio recording / wake state",
        .func = cmd_audio_status,
    };
    esp_console_cmd_register(&cmd_a_status);

    /* tts stats */
    const esp_console_cmd_t cmd_tts = {
        .command = "tts_stats",
        .help = "Show TTS player queue stats",
        .func = cmd_tts_stats,
    };
    esp_console_cmd_register(&cmd_tts);

    /* wake enable */
    const esp_console_cmd_t cmd_w_enable = {
        .command = "wake_enable",
        .help = "Enable W4 wake detection (auto recording on speech)",
        .func = cmd_wake_enable,
    };
    esp_console_cmd_register(&cmd_w_enable);

    /* wake disable */
    const esp_console_cmd_t cmd_w_disable = {
        .command = "wake_disable",
        .help = "Disable W4 wake detection",
        .func = cmd_wake_disable,
    };
    esp_console_cmd_register(&cmd_w_disable);

    /* wake status */
    const esp_console_cmd_t cmd_w_status = {
        .command = "wake_status",
        .help = "Show wake detection state + stats",
        .func = cmd_wake_status,
    };
    esp_console_cmd_register(&cmd_w_status);

    ESP_LOGI(TAG, "UART cmds: audio_/wake_/tts_/help");
}

/* 兼容旧 API 的 wrapper (保留给将来 debug 用, 当前未调用) */
static void handle_audio_uart_cmd(const char *line)
{
    int ret;
    esp_err_t err = esp_console_run(line, &ret);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "命令 '%s' 解析失败 (%s)", line, esp_err_to_name(err));
    }
}

/* 注意: v7 已删除手写的 uart_command_task, 改用 esp_console_new_repl_uart() +
 *      esp_console_start_repl() 创建独立 REPL task (官方 esp_claw 做法, 10KB 栈).
 *      见主循环内 s_uart_repl_started 分支. */

/* ── W3: WS Binary 帧回调 (TTS 下行 PCM 24kHz mono) ── */
static void on_dsh_binary_pcm(const uint8_t *data, size_t len, void *user_data)
{
    (void)user_data;
    esp_err_t err = audio::tts_player_feed_pcm(
        reinterpret_cast<const int8_t *>(data), len);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "🔇 [W3] tts_player_feed_pcm failed: %s", esp_err_to_name(err));
    }
}

/* ══════════════════════════════════════════════════════════ */

/* app_main 是从 C 代码 (app_startup.c) 调用的, 必须用 extern "C" 避免 C++ mangling */
extern "C" void app_main(void)
{
    /* ── Watchdog ──────────────────────────────────────────────
     * 系统在 app_main 之前已经初始化了 TWDT（5s 超时，panic 模式）。
     * 尝试 reconfigure 为 30s；若失败（旧版 IDF 不支持）则继续
     * 使用系统默认配置，靠喂狗维持。
     * ─────────────────────────────────────────────────────────── */
    /* C++ 严格字段顺序: 按声明顺序 (timeout_ms → idle_core_mask → trigger_panic) */
    esp_task_wdt_config_t wdt_cfg;
    wdt_cfg.timeout_ms = 30000;
    wdt_cfg.idle_core_mask = 0;   /* 不监控 idle task */
    wdt_cfg.trigger_panic = true;
    esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (wdt_err != ESP_OK) {
        ESP_LOGW(TAG, "TWDT reconfigure failed (%s), will feed existing WDT",
                 esp_err_to_name(wdt_err));
    } else {
        ESP_LOGI(TAG, "TWDT reconfigured to 30s");
    }
    /* 确保当前 main task 已注册到 WDT */
    esp_task_wdt_add(NULL);

    ESP_LOGI(TAG, "=== p4c5-agent-terminal v%s ===", P4C5_BOARD_VERSION);

    /* [0] Board init */
    ESP_LOGI(TAG, "[0/5] Board init (I2C)...");
    esp_err_t err = p4c5_board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board init failed: %s", esp_err_to_name(err));
        return;
    }
    esp_task_wdt_reset();   /* 喂狗 */

    /* [1] PMIC */
    ESP_LOGI(TAG, "[1/5] PMIC init (AXP2101)...");
    err = p4c5_pmic_init(p4c5_board_get_i2c_bus());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_pmic_print_adc();  /* T15 自测电源 */
    esp_task_wdt_reset();

    /* [2] Display */
    ESP_LOGI(TAG, "[2/5] Display init (ST7102 480x800)...");
    err = p4c5_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(err));
        return;
    }
    p4c5_display_bl_set(80);
    esp_task_wdt_reset();

    /* [3] Audio */
    ESP_LOGI(TAG, "[3/5] Audio init (ES8311+ES7210)...");
    p4c5_audio_set_i2c_bus(p4c5_board_get_i2c_bus());
    err = p4c5_audio_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Audio init failed (non-fatal): %s", esp_err_to_name(err));
    }
    esp_task_wdt_reset();

    /* [4] WiFi (ESP32-C5 via SDIO) — W1: 4G 搁置，改用 WiFi */
    ESP_LOGI(TAG, "[4/6] WiFi init (esp_hosted + wifi_manager)...");

    /* NVS 初始化 (WiFi 配置持久化) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* W1-stable: SDIO transport 已 link, 启用 WiFi */
    ESP_LOGI(TAG, "[4/6] WiFi init (esp_hosted SDIO via ESP32-C5)...");

    /* WiFi manager 初始化 */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 启动 WiFi (硬编码 SSID 测试) */
    wifi_manager_config_t wifi_cfg = {
        .sta_ssid = "ZTE-SONG-2.4G",
        .sta_password = "51UPSONG99",
        .ap_ssid_prefix = "p4c5-agent",
        .ap_ssid = NULL,
        .ap_password = NULL,
        .ap_behavior = "keep",
        .ap_channel = 1,
        .ap_max_conn = 4,
        .max_retry = 5,
    };
    ESP_ERROR_CHECK(wifi_manager_start(&wifi_cfg));

    /* 等待 STA 连接 (最多 20s — 必须 < TWDT 30s timeout) */
    ESP_LOGI(TAG, "Waiting for WiFi STA connection (max 20s)...");
    esp_err_t wifi_ret = wifi_manager_wait_connected(20000);
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi STA connected ✅");
    } else {
        ESP_LOGW(TAG, "WiFi STA connection timeout (fallback to AP mode)");
    }
    esp_task_wdt_reset();  /* TWDT reset (WiFi wait 不能超过 TWDT) */

    /* [5] DSH Client — WiFi transport */
    ESP_LOGI(TAG, "[5/6] DSH client init...");
    dsh_client_config_t dsh_cfg = {
        .url = CONFIG_P4C5_DSH_WEBSOCKET_URL,
        .device_id = CONFIG_P4C5_DSH_DEVICE_ID,
        .auth_token = (strlen(CONFIG_P4C5_DSH_AUTH_TOKEN) > 0)
                      ? CONFIG_P4C5_DSH_AUTH_TOKEN : NULL,
    };

    dsh_client_init(&dsh_cfg);
    dsh_client_register_event_callback(on_dsh_frame, NULL);
    dsh_client_register_state_callback(on_dsh_state, NULL);
    dsh_client_set_status_provider(status_provider, NULL);
    dsh_client_set_binary_callback(on_dsh_binary_pcm, NULL);  /* W3: TTS 下行 PCM */

    /* 连接 DSH via WiFi */
    err = dsh_client_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DSH connect via WiFi failed (will retry): %s", esp_err_to_name(err));
    }

    /* W3: 启动音频上行任务 (POC: 串口命令触发) */
    xTaskCreate(audio_uplink_task, "audio_uplink", 32768, NULL, 5, NULL);  // v11: 16KB 不够, 升 32KB

    /* W3: 启动 TTS 下行播放任务 */
    err = audio::tts_player_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "tts_player_init failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  All subsystems initialized");
    ESP_LOGI(TAG, "  v%s ready", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "  W3 提示: 串口输入 'audio start' 开始录音上行");
    ESP_LOGI(TAG, "  W3 提示: Mac Adapter 下行 PCM 自动播放");
    ESP_LOGI(TAG, "=========================================");

    /* M7 → T14: 测试图 → LVGL 真 UI */
    err = p4c5_ui_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LVGL UI init failed (non-fatal): %s", esp_err_to_name(err));
    }

    /* 主循环 */
    while (1) {
        esp_task_wdt_reset();

        /* W3/W4 修复: 读取 UART 命令
         *
         * v4 重大修复: 用独立 task 处理 UART 命令, 避免主 task 栈溢出!
         *
         * 之前 v3 (esp_console_init + uart_vfs_dev_use_driver 在主循环里) 触发:
         *   Guru Meditation Error: Core 0 panic'ed (Stack protection fault)
         *   Stack bounds: 0x4ff2ae08 - 0x4ff2bd80 = 仅 4KB 栈空间
         *
         * 原因: linenoise + esp_console 在主循环里 init 会占大栈, 主 task 默认 4KB 不够.
         *
         * 修复 (参考 ESP-IDF esp_console REPL 模式架构):
         *   - 启动独立 task "uart_cmd" (8KB 栈, priority 3)
         *   - task 内部 init esp_console + 注册命令 + select/fgetc 循环
         *   - 主循环不参与 UART 处理, 继续跑 10s 心跳
         *
         * 这是 ESP-IDF 官方推荐做法: 让 console 用自己的 task
         * 参考: components/console/esp_console_repl.c (REPL 内部就是这么做)
         */
        {
            static bool s_uart_repl_started = false;
            if (!s_uart_repl_started) {
                s_uart_repl_started = true;
                /* v7 重写: 完全对齐 esp_claw 官方实现 (cap_cli.c / app_claw_cli.c)
                 *
                 * 之前 v3-v6 失败根因: 我手动实现 esp_console REPL (esp_console_init + fgetc + select)
                 *   - 8KB 栈: panic 在 linenoise prompt render
                 *   - 16KB 栈: panic 在 vfprintf 格式化字符串
                 *   - 32KB 栈: panic 在 vfprintf (栈底擦痕)
                 *
                 * 官方 esp_claw 用 esp_console_new_repl_uart() + esp_console_start_repl()
                 *   task_stack_size=10240 (10KB) 就能跑!
                 *
                 * 参考: /tmp/esp_claw_p4c5/components/common/app_claw/app_claw_cli.c
                 *   esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
                 *   repl_config.prompt = "app> ";
                 *   repl_config.task_stack_size = 10240;
                 *   repl_config.max_cmdline_length = 512;
                 *   esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
                 *   esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
                 *   esp_console_register_help_command();
                 *   register_cap_cli_commands();
                 *   ...
                 *   esp_console_start_repl(repl);  // 阻塞但会创建 REPL task
                 */
                esp_console_repl_t *repl = NULL;
                esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
                repl_config.prompt = "p4c5> ";
                repl_config.task_stack_size = 10240;     // 10KB - 官方 esp_claw 值
                repl_config.max_cmdline_length = 512;     // 官方 esp_claw 值

                /* 关键修复 v10: 全部错了!  /dev/cu.usbmodem1301 不是 CH343P,
                 *   而是 ESP32-P4 **native USB-Serial-JTAG** (Espressif vendor 0x303A).
                 *   证据: ioreg 显示 USB JTAG/serial debug unit @ /dev/cu.usbmodem1301.
                 *   因此 REPL 必须用 esp_console_new_repl_usb_serial_jtag,
                 *   不能用 esp_console_new_repl_uart (UART0/GPIO38/37).
                 *
                 * CH343P 在 KSDIY_P4C5 board 上**根本没接线**或没安装 (或者我接到
                 *   另一个 USB hub 端口). macOS 上看不到任何 CH343P vendor (0x1a86).
                 *   所以 p4c5-pins.csv 写的 GPIO4/GPIO37 那张表是**未来的接线**,
                 *   实际板子用 USB-Serial-JTAG.
                 *
                 * v1-v9 都用 esp_console_new_repl_uart → 字符永远到不了 ESP32 (字符去了 UART0 GPIO38/37,
                 *   但 macOS 通过 USB-Serial-JTAG device 发字符, USB-Serial-JTAG 在 ESP32 内部
                 *   是另一条独立的 RX/TX channel, 跟 GPIO38/37 UART0 没关系).
                 */
                esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
                esp_err_t repl_err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
                ESP_LOGI(TAG, "REPL 通道: USB-Serial-JTAG (vs UART0 的 GPIO37/38, 那个走不通)");
                if (repl_err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_console_new_repl_uart failed: %s", esp_err_to_name(repl_err));
                } else {
                    ESP_LOGI(TAG, "REPL 创建成功");
                }
                if (repl_err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_console_new_repl_uart failed: %s", esp_err_to_name(repl_err));
                } else {
                    esp_console_register_help_command();
                    register_uart_commands();
                    ESP_LOGI(TAG, "Starting esp_console REPL (task_stack=10240)");
                    esp_err_t start_err = esp_console_start_repl(repl);
                    if (start_err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_console_start_repl failed: %s", esp_err_to_name(start_err));
                    }
                    ESP_LOGI(TAG, "REPL 已启动 - 试着在另一个终端用 miniterm/pyserial 发 'help\\r'");
                    vTaskDelay(pdMS_TO_TICKS(2000));   /* 给 REPL task 时间 print prompt */
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10000));

        /* W1-stable: heartbeat 显示 bat + WiFi 状态 */
        wifi_manager_status_t wifi_st = {0};
        wifi_manager_get_status(&wifi_st);
        ESP_LOGI(TAG, "💓 bat=%u%% wifi=%s ip=%s dsh=%s", p4c5_pmic_get_battery_level(),
                 wifi_st.sta_connected ? "✅ connected" : "❌ disconnected",
                 wifi_st.sta_ip ? wifi_st.sta_ip : "0.0.0.0",
                 dsh_client_is_connected() ? "✅" : "❌");

        /* W2: 每 20s 触发一次, 验证 mock server 完整对话流
           (thinking → assistant_text → assistant_done → tool_call) */
        static int w2_user_input_count = 0;
        if (dsh_client_is_connected() && ++w2_user_input_count % 2 == 0) {
            ESP_LOGI(TAG, "📤 [W2] 发送 client/user_input (#%d)", w2_user_input_count / 2);
            dsh_client_send_user_input("W2 测试: 验证 mock server 完整对话流");
        }

        /* W2: 收到 tool_call 后自动回 client/tool_result (验证上行帧) */
        if (dsh_client_is_connected() && w2_last_tool_id[0] != '\0') {
            ESP_LOGI(TAG, "📤 [W2] 发送 client/tool_result (id=%s)", w2_last_tool_id);
            cJSON *result = cJSON_CreateObject();
            cJSON_AddStringToObject(result, "battery", "85%");
            cJSON_AddStringToObject(result, "wifi_rssi", "-45dBm");
            dsh_client_send_tool_result(w2_last_tool_id, result);
            cJSON_Delete(result);
            w2_last_tool_id[0] = '\0';  /* 单次回执 */
        }
#if 0  /* W1 disabled: dsh_client_is_connected 触发 lwIP */
        wifi_manager_status_t wifi_st = {0};
        wifi_manager_get_status(&wifi_st);
        ESP_LOGI(TAG, "💓 bat=%u%% dsh=%s wifi=%s (IP=%s mode=%s)",
                 p4c5_pmic_get_battery_level(),
                 dsh_client_is_connected() ? "✅" : "❌",
                 wifi_st.sta_connected ? "connected" : "disconnected",
                 wifi_st.sta_ip ? wifi_st.sta_ip : "0.0.0.0",
                 wifi_st.mode ? wifi_st.mode : "off");

        /* 每 30s 扫描一次附近 AP 看实际能看到什么 */
        static int scan_count = 0;
        if (++scan_count % 3 == 0) {
            wifi_manager_scan_record_t records[10];
            uint16_t count = 0;
            if (wifi_manager_scan_aps(records, 10, &count) == ESP_OK && count > 0) {
                ESP_LOGI(TAG, "📡 Found %d APs:", count);
                for (int i = 0; i < count && i < 10; i++) {
                    ESP_LOGI(TAG, "  [%d] SSID='%s' RSSI=%d auth=%d ch=%d",
                             i, records[i].ssid, records[i].rssi,
                             records[i].authmode, records[i].primary);
                }
            } else {
                ESP_LOGW(TAG, "📡 Scan failed or no AP found");
            }
        }
#endif  /* W1 disabled */
    }
}
