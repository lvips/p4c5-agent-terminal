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
#include "aec_sw.h"                /* W3: 软件 AEC 回声消除 (NLMS, P4 自实现) */
#include "resampler_24_16.h"       /* W3: 24kHz→16kHz 重采样 (适配 p4c5_audio 24kHz) */
#include "opus_encoder.h"          /* W3: libopus 编码 (16kbps, 20ms 帧) */
#include "tts_player.h"            /* W3: TTS 下行 PCM 播放 (WS Binary → ES8311 DAC) */
#include "wake_word_detector.h"    /* W4: P4 自实现 RMS-based 唤醒检测 */
#include "config.h"
#include <string.h>

static const char *TAG = "app_main";

/* W2: 收到的最后一个 tool_call id (用于回 tool_result) */
static char w2_last_tool_id[64] = {0};

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

    } else if (strcmp(frame_type, "assistant_done") == 0) {
        printf("\n");
        ESP_LOGI(TAG, "📝 [down] assistant_done");

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

    /* 20ms 帧规格 (适配 p4c5_audio 24kHz) */
    constexpr size_t FRAME_SAMPLES_24K = 480;   /* 24kHz × 0.020s = 480 samples */
    constexpr size_t FRAME_SAMPLES_16K = 320;   /* 16kHz × 0.020s = 320 samples */

    static int16_t s_in4ch_buf[480 * 4];        /* 4ch × 480 = 1920 samples = 3840 bytes */
    static int16_t s_mic_mono[480];             /* ch0+ch2 平均 → mono 24kHz */
    static int16_t s_ref_mono[480];             /* ch1 → AEC ref 24kHz */
    static int16_t s_aec_out[480];              /* AEC 后 mono 24kHz */
    static int16_t s_mono16k[320];              /* resampler 输出 16kHz */
    static uint8_t s_opus_buf[1276];            /* OPUS max packet (RFC 6716) */

    /* ── 初始化 3 组件 + 软件 AEC ── */
    audio::AudioMixerConfig mixer_cfg = audio::audio_mixer_default_config();
    mixer_cfg.in_channels = 4;
    mixer_cfg.sample_rate = 24000;
    ESP_ERROR_CHECK(audio::audio_mixer_init(mixer_cfg));

    audio::AecConfig aec_cfg = audio::aec_sw_default_config();
    aec_cfg.sample_rate     = 24000;
    aec_cfg.filter_taps     = 128;          /* 5.3ms @ 24kHz */
    aec_cfg.step_size       = 0.005f;
    aec_cfg.leakage         = 0.999f;
    aec_cfg.ref_gain        = 1.0f;
    aec_cfg.enable_aec      = P4C5_AUDIO_INPUT_REF;  /* 硬件有 ref 才开 */
    ESP_ERROR_CHECK(audio::aec_sw_init(aec_cfg));

    audio::ResamplerConfig res_cfg = audio::resampler_24_16_default_config();
    ESP_ERROR_CHECK(audio::resampler_24_16_init(res_cfg));

    audio::OpusEncoderConfig opus_cfg = audio::opus_encoder_default_config();
    ESP_ERROR_CHECK(audio::opus_encoder_init(opus_cfg));

    /* W4: 唤醒检测初始化 (自适应阈值: 自动适应环境噪声) */
    audio::WakeDetectorConfig wake_cfg;
    wake_cfg.sample_rate         = 24000;
    wake_cfg.wake_rms_threshold  = 1500;  /* 初始唤醒 RMS 阈值 (会被自适应覆盖) */
    wake_cfg.sleep_rms_threshold = 500;   /* 初始睡眠 RMS 阈值 */
    wake_cfg.wake_hold_frames    = 5;     /* 100ms 持续唤醒 */
    wake_cfg.sleep_hold_frames   = 100;   /* 2s 持续静音后睡眠 */
    wake_cfg.adaptive_threshold  = true;  /* W4+: 启用自适应噪声阈值 */
    wake_cfg.noise_floor_alpha   = 1;     /* EMA 系数 */
    wake_cfg.wake_delta          = 1000;  /* wake 阈值 = noise_floor + 1000 */
    wake_cfg.sleep_delta         = 200;   /* sleep 阈值 = noise_floor + 200 */
    wake_cfg.noise_update_frames = 50;    /* 每 1s 更新一次 noise_floor */
    ESP_ERROR_CHECK(audio::wake_word_detector_init(wake_cfg));

    ESP_LOGI("audio_uplink", "W3+W4 音频上行链路启动");
    ESP_LOGI("audio_uplink", "  4ch@24kHz (ch0+ch2=mic, ch1=AEC_ref, ch3=unused)");
    ESP_LOGI("audio_uplink", "  → 软件 AEC (NLMS, taps=%u, mu=%.4f)",
             aec_cfg.filter_taps, aec_cfg.step_size);
    ESP_LOGI("audio_uplink", "  → 24k→16k 重采样 → opus → WS Binary 上行");
    ESP_LOGI("audio_uplink", "触发方式:");
    ESP_LOGI("audio_uplink", "  - 串口 'audio start/stop' (手动)");
    ESP_LOGI("audio_uplink", "  - 串口 'wake enable' (W4: RMS-based 自动唤醒)");

    uint64_t frame_count  = 0;
    uint64_t sent_count   = 0;
    bool     is_uploading = false;  /* 当前是否在上行 (RECORDING 状态) */

    for (;;) {
        /* 总门控: 必须 audio_recording 或 wake_enabled 任一开启 */
        if (!s_audio_recording && !s_wake_enabled) {
            if (is_uploading) {
                ESP_LOGI("audio_uplink", "🛑 停止上行 (门控关闭)");
                is_uploading = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* 1. 4 通道 24kHz 录音 */
        esp_err_t ret = p4c5_audio_record_multi(s_in4ch_buf, FRAME_SAMPLES_24K);
        if (ret != ESP_OK) {
            if ((frame_count % 250) == 0) {
                ESP_LOGW("audio_uplink", "p4c5_audio_record_multi 失败: %s", esp_err_to_name(ret));
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* 2. 4 通道分离: ch0+ch2 → mic, ch1 → ref, ch3 → unused
         *
         * TDM 帧布局: [ch0_s0, ch1_s0, ch2_s0, ch3_s0, ch0_s1, ch1_s1, ...]
         * mic = (ch0 + ch2) / 2
         * ref = ch1 (扬声器回采)
         */
        for (size_t i = 0; i < FRAME_SAMPLES_24K; i++) {
            int32_t ch0 = s_in4ch_buf[i * 4 + 0];
            int32_t ch1 = s_in4ch_buf[i * 4 + 1];
            int32_t ch2 = s_in4ch_buf[i * 4 + 2];
            /* ch3 unused */

            s_mic_mono[i] = (int16_t)((ch0 + ch2) / 2);
            s_ref_mono[i] = (int16_t)ch1;
        }

        /* 3. 软件 AEC (NLMS): mic - estimated_echo → out
         *
         * 限制:
         *   - 仅 P4C5_AUDIO_INPUT_REF=true 时有效
         *   - 硬件需真有 AEC ref 回采 (ES8311 输出 → ES7210 MIC2)
         */
        ret = audio::aec_sw_process(s_mic_mono, s_ref_mono,
                                     s_aec_out, FRAME_SAMPLES_24K);
        if (ret != ESP_OK) {
            frame_count++;
            continue;
        }

        /* W4: 唤醒检测 - 仅在 wake_enabled 且非手动 audio_recording 时检测
         *    手动 audio_recording 跳过 wake (强制上行)
         */
        if (s_wake_enabled && !s_audio_recording) {
            audio::WakeEvent evt = audio::wake_word_detector_feed(
                s_aec_out, FRAME_SAMPLES_24K);
            if (evt == audio::WakeEvent::WAKE && !is_uploading) {
                is_uploading = true;
                ESP_LOGI("audio_uplink", "🌟 唤醒! 开始上行 (W4 auto)");
            } else if (evt == audio::WakeEvent::SLEEP && is_uploading) {
                is_uploading = false;
                ESP_LOGI("audio_uplink", "💤 睡眠, 停止上行 (W4 auto)");
            }
        } else {
            /* 手动模式: 总是上行 */
            is_uploading = s_audio_recording;
        }

        /* 不上行时, 只录音不上行 (节省带宽 + 等待唤醒) */
        if (!is_uploading) {
            frame_count++;
            continue;
        }

        /* 4. 24kHz → 16kHz 重采样 (480 → 320 samples) */
        size_t out_len = 0;
        ret = audio::resampler_24_16_process(s_aec_out, s_mono16k, &out_len);
        if (ret != ESP_OK || out_len != FRAME_SAMPLES_16K) {
            frame_count++;
            continue;
        }

        /* 5. Opus 编码 */
        size_t opus_len = sizeof(s_opus_buf);
        ret = audio::opus_encoder_encode(s_mono16k, FRAME_SAMPLES_16K,
                                          s_opus_buf, &opus_len);
        if (ret != ESP_OK) {
            frame_count++;
            continue;
        }

        /* 6. WS Binary 帧上行 */
        esp_err_t send_ret = dsh_client_send_audio(s_opus_buf, opus_len);
        if (send_ret == ESP_OK) {
            sent_count++;
            if ((sent_count % 250) == 0) {
                ESP_LOGI("audio_uplink", "已发送 %llu 帧 (opus %u bytes/帧, AEC=%s, wake=%s)",
                         (unsigned long long)sent_count, (unsigned)opus_len,
                         aec_cfg.enable_aec ? "ON" : "OFF",
                         s_wake_enabled ? "AUTO" : "MAN");
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
            uint32_t mic_rms = calc_rms(s_mic_mono, FRAME_SAMPLES_24K);
            uint32_t ref_rms = calc_rms(s_ref_mono, FRAME_SAMPLES_24K);
            uint32_t aec_rms = calc_rms(s_aec_out,   FRAME_SAMPLES_24K);
            ESP_LOGI("audio_diag",
                     "frame=%llu mic=%u ref=%u aec=%u aec/mic=%u%% "
                     "(期望: ref>>mic, aec<<mic 表示 AEC 收敛)",
                     (unsigned long long)frame_count,
                     (unsigned)mic_rms, (unsigned)ref_rms, (unsigned)aec_rms,
                     (unsigned)((aec_rms * 100) / (mic_rms ? mic_rms : 1)));
        }
    }
}

/* ── 串口命令处理 (POC: 解析 audio start/stop + wake enable) ── */
static void handle_audio_uart_cmd(const char *line)
{
    if (strncmp(line, "audio start", 11) == 0) {
        s_audio_recording = true;
        ESP_LOGI(TAG, "🎤 [W3] audio recording ON (4ch@24k + AEC + opus 上行链路启用)");
    } else if (strncmp(line, "audio stop", 10) == 0) {
        s_audio_recording = false;
        ESP_LOGI(TAG, "🔇 [W3] audio recording OFF (上行链路暂停)");
    } else if (strncmp(line, "audio status", 12) == 0) {
        ESP_LOGI(TAG, "🎤 [W3] audio recording = %s, wake = %s",
                 s_audio_recording ? "ON" : "OFF",
                 s_wake_enabled ? "ENABLED" : "DISABLED");
    } else if (strncmp(line, "tts stats", 9) == 0) {
        audio::tts_player_print_stats();
    } else if (strncmp(line, "wake enable", 11) == 0) {
        s_wake_enabled = true;
        s_audio_recording = false;  /* wake 模式自动控制 */
        ESP_LOGI(TAG, "🌟 [W4] wake detection ENABLED (喊一声激活, 静音 2s 自动停止)");
    } else if (strncmp(line, "wake disable", 12) == 0) {
        s_wake_enabled = false;
        ESP_LOGI(TAG, "💤 [W4] wake detection DISABLED");
    } else if (strncmp(line, "wake status", 11) == 0) {
        ESP_LOGI(TAG, "🌟 [W4] wake = %s, audio_recording = %s",
                 s_wake_enabled ? "ENABLED" : "DISABLED",
                 s_audio_recording ? "ON" : "OFF");
        audio::wake_word_detector_print_stats();
    }
}

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
    xTaskCreate(audio_uplink_task, "audio_uplink", 16384, NULL, 5, NULL);

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
