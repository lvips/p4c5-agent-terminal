# P1: ESP-SR AFE 升级设计 (替换自实现 AEC + RMS Wake)

**日期**: 2026-09-07
**目标**: 用 ESP-SR AFE 替换自实现 NLMS AEC + 自实现 RMS Wake + 自实现 resampler 24→16
**参考**: `/Volumes/ZT-1T/项目开发/ESP32-P4C5/硬件开源资料/ESP32P4C5开发板 酷世DIY/程序例程/仅支持IDF5.5.5编译/04.advanced.xiaozhi_ksdiy-p4c5/`

---

## 一、当前 (W3+W4) 与目标 (P1) 流水线对比

### 1.1 当前 (app_main.cpp:267-340 audio_uplink_task)

```
ES7210 ADC 4ch @ 24kHz (ch0+ch2=mic, ch1=AEC_ref, ch3=unused)
  ↓
audio_mixer (ch0+ch2 平均 → mic_mono_24k)
  ↓
aec_sw (NLMS, taps=128, μ=0.005, leak=0.999) — 自实现 AEC
  ↓
resampler_24_16 (24k→16k 整数重采样) — 自实现
  ↓
opus_encoder (libopus 16kbps, 20ms) — 真硬件
  ↓
wake_word_detector (RMS 自适应阈值, EMA 噪声底) — 自实现 wake
  ↓
dsh_client WS 上行
```

**问题**:
- ❌ NLMS AEC 收敛慢, 双麦情况下 aec_out 仍接近 mic (audio_diag: aec=7, aec/mic=100%)
- ❌ RMS wake 误触率高 (环境噪音超阈值就醒)
- ❌ 没有 NS (噪声抑制), 远处噪声同样触发 wake
- ❌ 24→16 重采样有 aliasing
- ❌ 不能跑命令词识别 (无 MultiNet)

### 1.2 目标 (xiaozhi p4c5 AfeAudioProcessor)

```
ES7210 ADC (codec 抽象层, 同样 4ch 模式)
  ↓
afe_audio_processor (AFE_TYPE_VC / AFE_MODE_HIGH_PERF)
  ├─ AEC  (aec_init=true, AEC_MODE_VOIP_HIGH_PERF)
  ├─ NS   (ns_init=true, ns_model=nsnet1, AFE_NS_MODE_NET)
  ├─ VAD  (vad_init=true, VAD_MODE_0, vad_min_noise_ms=100)
  ├─ AGC  (agc_init=false, 我们不需要)
  └─ I/O  (M-M-M-R 格式: 3 mic + 1 ref)
  ↓
afe_iface_->fetch_with_delay() 阻塞取 16kHz mono PCM (AEC+NS+VAD 处理完)
  ↓
opus_encoder → WS 上行
  ↓
[并行] afe_wake_word (AFE_TYPE_SR + WakeNet9 模型)
  └─ 检测到 "嗨乐鑫" → callback → AudioService 切 LISTENING 状态
```

**改进**:
- ✅ ESP-SR AEC 在 ESP32-P4 上是 Net 模型 (训练好的), 双麦收敛快
- ✅ WakeNet9 神经网络唤醒 (95%+ 准确率), 不误触
- ✅ NSNet1 噪声抑制 (远处噪声被压低)
- ✅ VADNet1 语音活动检测 (准确识别说话起止)
- ✅ 一个 AFE handle 统一管理, 不用自己拼

---

## 二、关键 ESP-SR API (从 xiaozhi 代码)

### 2.1 初始化 (afe_audio_processor.cc:30-68)

```c
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_srmodel.h"

srmodel_list_t *models = esp_srmodel_init("model");  // 从 "model" 分区加载
char* ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
char* vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, NULL);

afe_config_t* afe_config = afe_config_init("MMMR", NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
afe_config->vad_mode = VAD_MODE_0;
afe_config->vad_min_noise_ms = 100;
afe_config->vad_model_name = vad_model_name;
afe_config->ns_init = (ns_model_name != NULL);
afe_config->ns_model_name = ns_model_name;
afe_config->afe_ns_mode = AFE_NS_MODE_NET;
afe_config->agc_init = false;
afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;  // PSRAM 分配
afe_config->aec_init = true;
afe_config->vad_init = false;  // VC 模式 VAD 关闭 (用 fetch_vad_state)

const esp_afe_sr_iface_t *afe_iface = esp_afe_handle_from_config(afe_config);
esp_afe_sr_data_t *afe_data = afe_iface->create_from_config(afe_config);
```

### 2.2 feed + fetch

```c
size_t feed_size = afe_iface->get_feed_chunksize(afe_data);    // 字节
size_t fetch_size = afe_iface->get_fetch_chunksize(afe_data);  // 字节 (16kHz mono 30ms)

// 喂入 4ch PCM (M-M-M-R 格式)
afe_iface->feed(afe_data, in_4ch_pcm);

// 阻塞取 AFE 处理后 16kHz mono + VAD state
afe_iface->fetch_with_delay(afe_data, portMAX_DELAY);
// res->data = 16kHz mono PCM (int16_t)
// res->data_size = 字节数
// res->vad_state = VAD_SPEECH / VAD_SILENCE
// res->wakeup_state = 唤醒词状态
```

### 2.3 WakeNet9 集成 (afe_wake_word.cc:55-78)

```c
srmodel_list_t *models = esp_srmodel_init("model");
// 自动找 ESP_WN_PREFIX 开头的模型
char* wakenet_model = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
char* wake_words = esp_srmodel_get_wake_words(models, wakenet_model);
// 例如: "hilexin" (嗨乐鑫) / "nihaoxiaozhi" (你好小智)

afe_config_t* wake_config = afe_config_init("MMMR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
wake_config->aec_init = true;  // SR 模式 AEC 开
wake_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
wake_config->afe_perferred_core = 1;  // 跑在 CORE 1 (避开 wifi_manager)
wake_config->afe_perferred_priority = 1;
wake_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

const esp_afe_sr_iface_t *wake_iface = esp_afe_handle_from_config(wake_config);
esp_afe_sr_data_t *wake_data = wake_iface->create_from_config(wake_config);

// 在 task 里阻塞取 wake event
while (running) {
    auto res = wake_iface->fetch_with_delay(wake_data, portMAX_DELAY);
    if (res->wakeup_state == WAKEUP_DETECTED) {
        // 检测到唤醒词!
        callback(wake_words);
    }
}
```

---

## 三、partition 表 (必须改)

### 当前 partitions (W3+W4):
```
nvs,      data, nvs,     0x9000,    0x6000
otadata,  data, ota,     0xf000,    0x2000
phy_init, data, phy,     0x11000,   0x1000
ota_0,    app,  ota_0,   0x20000,   0x3f0000
ota_1,    app,  ota_1,   ,          0x3f0000
storage,  data, spiffs,  0x800000,  0x700000  (7MB spiffs)
```

### P1 新 partitions (要加 "model" 分区, ESP-SR 模型放这里):
```
nvs,      data, nvs,     0x9000,    0x6000
otadata,  data, ota,     0xf000,    0x2000
phy_init, data, phy,     0x11000,   0x1000
ota_0,    app,  ota_0,   0x20000,   0x3f0000
ota_1,    app,  ota_1,   ,          0x3f0000
model,    data, spiffs,  0x800000,  0x400000  ← 新增 4MB (WakeNet9+NSNet1+VADNet1 ≈ 2.5MB)
storage,  data, spiffs,  0xC00000,  0x300000  (3MB spiffs)
```

> ⚠️ 分区名必须是 `model`, 因为 `esp_srmodel_init("model")` 硬编码读这个名

---

## 四、ESP-SR 模型烧录

### 4.1 下载模型 (从 Espressif 官方)

ESP-SR 模型不在 idf_component 里, 需要单独下载放到 `model` 分区:

```bash
# 模型来源: https://github.com/espressif/esp-sr/tree/master/model
# 需下载:
#   - wakenet9/wn9_hilexin    (嗨乐鑫 - 默认中文唤醒词, 1.4MB)
#   - nsnet1                  (噪声抑制, 0.6MB)
#   - vadnet1_medium          (VAD 模型, 0.5MB)
```

### 4.2 打包 model 分区

ESP-SR 提供工具 `srmodels_bin.py`, 把模型打包成可在 SPIFFS 烧录的 bin:

```bash
cd components/esp-sr/tools
python3 gen_srmodels.py \
  --model_dir components/esp-sr/model/wakenet9 \
  --model_dir components/esp-sr/model/nsnet1 \
  --model_dir components/esp-sr/model/vadnet1 \
  --output p4c5_srmodels.bin
```

然后用 `idf.py storage` 或 `parttool.py` 烧录到 `model` 分区。

### 4.3 简化方案: 内嵌模型 (推荐)

不用 SPIFFS 分区, 直接把模型 C 数组编译进 firmware:

```c
// model_data.c
const unsigned char wn9_hilexin_model[] = { ... };  // 1.4MB
const unsigned char nsnet1_model[] = { ... };       // 0.6MB
const unsigned char vadnet1_model[] = { ... };      // 0.5MB

// 改用 esp_srmodel_init_from_buffer() 替代 esp_srmodel_init("model")
```

> ⚠️ 但这会让 firmware 体积增大 2.5MB+ (当前 firmware 0x24ff80 ≈ 2.4MB, 加上模型会超 4.9MB)
> 当前 ota_0 分区 0x3f0000 = 4.13MB. 加模型后**会超**. 需要把 ota_0 改大.

---

## 五、实施步骤

### Step 1: 添加依赖 (idf_component.yml)

```yaml
espressif/esp-sr: ~2.3.0   # ESP32-P4 + WakeNet9 支持
```

### Step 2: 改 partitions (partitions.csv)

新增 `model` 分区 (4MB).

### Step 3: 创建 audio_afe_processor component

参考 xiaozhi `afe_audio_processor.cc` 简化版:

```cpp
// components/audio_pipeline/audio_afe_processor/
class AudioAfeProcessor {
public:
    esp_err_t init(int channels, int ref_channels);  // 4 mic + 1 ref → "MMMMR"? no, "MMMR" (3 mic + 1 ref)
    esp_err_t feed(int16_t* pcm_4ch, size_t samples);
    esp_err_t fetch(int16_t** out_pcm, size_t* out_samples, vad_state_t* out_vad);
    void enable_aec(bool enable);
    void enable_vad(bool enable);
private:
    const esp_afe_sr_iface_t* afe_iface_;
    esp_afe_sr_data_t* afe_data_;
};
```

### Step 4: 创建 audio_wake_word component (WakeNet9)

```cpp
// components/audio_pipeline/audio_wake_word/
class AudioWakeWord {
public:
    esp_err_t init();
    esp_err_t feed(int16_t* pcm_16k_mono, size_t samples);
    void on_detected(std::function<void(const char* wake_word)> cb);
private:
    srmodel_list_t* models_;
    const esp_afe_sr_iface_t* wake_iface_;
    esp_afe_sr_data_t* wake_data_;
};
```

### Step 5: 重构 audio_uplink_task

```cpp
// 替换 audio_mixer + aec_sw + resampler_24_16 + wake_word_detector
// → audio_afe_processor + audio_wake_word

// init
audio_afe_processor_init(AFE_TYPE_VC, "MMMR");  // 3 mic + 1 ref
audio_wake_word_init(AFE_TYPE_SR, "MMMR");

// task loop
for (;;) {
    int16_t in_4ch[480 * 4];  // 24kHz, 20ms
    p4c5_audio_record_4ch(in_4ch, 480);
    
    // 喂入 AFE
    audio_afe_processor_feed(in_4ch, 480 * 4);
    
    // 取 AFE 处理后 16kHz mono + VAD
    int16_t* out_16k_mono;
    size_t out_samples;
    vad_state_t vad;
    audio_afe_processor_fetch(&out_16k_mono, &out_samples, &vad);
    
    // OPUS 编码
    opus_encode(out_16k_mono, out_samples, opus_buf, &opus_len);
    
    // 上行
    dsh_client_send_audio(opus_buf, opus_len);
    
    // 唤醒检测 (独立 task)
    audio_wake_word_feed(out_16k_mono, out_samples);
}
```

### Step 6: Kconfig 选项

```kconfig
menu "Audio Front-End (ESP-SR AFE)"

config AFE_TYPE
    default "AFE_TYPE_VC"  # Voice Communication (AEC + NS + VAD)
    
config USE_ESPRESSIF_WAKE_NET
    default y
    help "Use WakeNet9 for wake word detection"
    
config WAKE_NET_MODEL
    default "wn9_hilexin"
    help "Default wake word: '嗨乐鑫'"
```

---

## 六、保留 vs 删除的代码

### 保留:
- `opus_encoder` (真硬件 OPUS, ESP-SR 也用 OPUS 编码但我们自己这层可用)
- `tts_player` (TTS 下行, ESP-SR 不管)
- `audio_mixer` (简化版, ESP-SR 要求 "MMMR" 格式但我们的实际是 4ch, 可以直接 feed 4ch 不需 mixer)

### 删除 (替换):
- `aec_sw` ← ESP-SR AEC
- `resampler_24_16` ← AFE 内部 24→16
- `wake_word_detector` ← WakeNet9

### 改动:
- `p4c5_audio.cc` `record_multi()` 返回的 4ch PCM 直接喂给 AFE
- `app_main.cpp` `audio_uplink_task` 用 AFE API 替换 4 个旧组件
- `partitions.csv` 新增 model 分区

---

## 七、风险评估

| 风险 | 严重度 | 缓解 |
|---|---|---|
| firmware 体积超 ota_0 (4MB) | 高 | 把 model 用 SPIFFS 分区烧录, 不内嵌 |
| ESP-SR 2.3.0 在 IDF 5.5.5 上崩溃 | 中 | 用 2.4.5+ (修过 P4 崩溃) |
| ESP32-P4 SRAM 不够 (PSRAM 慢) | 中 | 设 `AFE_MEMORY_ALLOC_MORE_PSRAM` |
| WakeNet9 模型太大 (1.4MB) | 低 | 用 wn9l (light, 800KB) |
| 4G 模块 (ML307C) 影响 AFE | 低 | AFE 跑 core 1, 4G 跑另一 task |

---

## 八、验证步骤

1. ✅ 编译通过 (idf.py build)
2. ✅ Flash OK, ESP-SR 模型烧到 model 分区
3. ✅ ESP32 boot log 显示 ESP-SR 模型加载成功 (srmodel_init OK, 模型名打印)
4. ✅ AEC 收敛 (audio_diag: aec << mic, aec/mic < 50%)
5. ✅ VAD 准确 (说话时 vad_state=SPEECH, 静音时 SILENCE)
6. ✅ WakeNet9 检测 "嗨乐鑫" → 触发回调 → 进入 RECORDING 状态
7. ✅ mock_dsh 真阿里云 ASI 识别 (W5 已通过)

---

## 九、参考资料

| 来源 | 文件 |
|---|---|
| xiaozhi p4c5 AFE 集成 | `/tmp/xiaozhi_p4c5/main/audio/processors/afe_audio_processor.cc` (201 行) |
| xiaozhi p4c5 WakeNet 集成 | `/tmp/xiaozhi_p4c5/main/audio/wake_words/afe_wake_word.cc` |
| xiaozhi p4c5 AudioService | `/tmp/xiaozhi_p4c5/main/audio/audio_service.cc` |
| xiaozhi p4c5 依赖清单 | `/tmp/xiaozhi_p4c5/main/idf_component.yml` (espressif/esp-sr ~2.3.0) |
| xiaozhi p4c5 partitions | `/tmp/xiaozhi_p4c5/partitions/v2/16m.csv` |
| ESP-SR 官方文档 | https://docs.espressif.com/projects/esp-sr/ |
| ESP-SR 组件注册 | https://components.espressif.com/components/espressif/esp-sr |

---

## 十、提交记录 (待实现)

预计 commits:
1. `chore(partition): 加 model 分区给 ESP-SR 模型`
2. `feat(audio): 引入 audio_afe_processor 组件`
3. `feat(audio): 引入 audio_wake_word 组件 (WakeNet9)`
4. `refactor(audio_uplink): 用 AFE 替换自实现 AEC + resampler + wake`
5. `test(afe): 真物理验证 AEC 收敛 + VAD + WakeNet9`
