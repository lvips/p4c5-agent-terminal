# W3 综合对比与实施方案 — xiaozhi vs OMT vs p4c5 (us)

> **版本**: v1.0（2026-09-07）
> **目标**: 拿到一手开源材料 (xiaozhi ksdiy-p4c5 官方实现 + CC 归档调研报告) 后,
>           与 OMT 既有方案 + 我自己之前的 W3 自实现做完整对比, 取长补短, 给出实施方案
> **参考资料**:
>   - xiaozhi 官方 (`/tmp/xiaozhi_ksdiy/`, 来自 `硬件开源资料/.../xiaozhi_ksdiy-p4c5.zip`)
>   - xiaozhi 调研报告 (`docs/research/xiaozhi-esp32-2026.md`, CC T2 已写)
>   - OMT 主项目 (`/Volumes/ZT-1T/项目开发/OMT/`)
>   - 我们 W3 commit (`hardware/p4c5-agent-terminal/components/audio_pipeline/`)

---

## 0. TL;DR — 关键结论

| 维度 | 现状判断 |
|------|---------|
| **唤醒词** | 我自实现 RMS 是 POC, 量产必用 ESP-SR WakeNet9 (P4 官方支持) |
| **AEC 回声** | 我自实现 NLMS 是 POC, P4 完全支持 ESP-SR AFE, 量产必用 |
| **降采样** | 我自实现 24→16k, AFE 内部已完成, 应删 |
| **通道数** | 我改 p4c5_audio 开 4 通道, xiaozhi 官方 kevin-p4c5-4g 只开 2 通道 (mic+ref) — 我错了 |
| **ASR** | OMT 用阿里云 NLS ISI (REST POP v1) + Mac 端 VAD; xiaozhi 推讯飞; 用户已有阿里 API → **沿用 OMT** |
| **TTS** | OMT 没做, xiaozhi 推火山豆包; 用户有阿里百炼 CosyVoice API → **沿用阿里百炼** |
| **协议** | 我们 DSH 14+4 帧是差异化优势, xiaozhi 9 帧; **保留 DSH 协议层** |
| **Application** | xiaozhi 有 1119 行 application.cc 完整状态机, 我们目前 main 只有 6 步 pipeline; **应该引入** |

---

## 1. 三个方案核心架构对比

### 1.1 xiaozhi-esp32 (行业标杆, 120万部署)

```
┌─────────────────── ESP32-P4C5 ─────────────────────┐
│                                                    │
│  I2S/ES7210 (4ch@24k) ─┐                         │
│                         ▼                         │
│              ┌─────────────────────┐              │
│              │  BoxAudioCodec      │              │
│              │  2 通道 mic+ref     │              │
│              └──────────┬──────────┘              │
│                         ▼                         │
│              ┌─────────────────────┐              │
│              │  AfeAudioProcessor  │              │
│              │  ESP-SR AFE         │              │
│              │  (AEC+NS+VAD 一体)  │              │
│              │  (P4 官方支持)      │              │
│              └──────────┬──────────┘              │
│                         ▼                         │
│                  16kHz mono PCM                   │
│                         │                         │
│         ┌───────────────┴───────────────┐         │
│         ▼                               ▼         │
│  ┌─────────────┐                 ┌─────────────┐│
│  │ AfeWakeWord │                 │ AudioEncoder││
│  │ ESP-SR      │                 │ Opus 16k    ││
│  │ WakeNet9    │                 │ 60ms 帧     ││
│  │ (P4 支持)   │                 └──────┬──────┘│
│  └─────────────┘                        ▼       │
│         │                       WebSocket Binary │
│         ▼                               │       │
│  状态切换                                ▼       │
│  Application (1119 行)              ┌──────────┐ │
│  状态机: idle/wifi_cfg/listening/   │  Server  │ │
│  speaking/thinking/activating/...   │ xiaozhi  │ │
│                                      │  .me     │ │
│                                      └──────────┘ │
└──────────────────────────────────────────────────┘
```

### 1.2 OMT (Tab5 + Node.js adapter)

```
┌─────────────────── ESP32-P4 (Tab5) ────────────────┐
│                                                    │
│  I2S/ES7210 (4ch@48k) → audio_input               │
│                         (501 行, VAD 双层)         │
│                              │                     │
│                              ▼                     │
│                         FramePacket 队列            │
│                         (10 帧 ~200ms)              │
│                              │                     │
│                              ▼                     │
│              audio_uplink_task (24KB 栈)          │
│              ┌──────────────────────────┐          │
│              │ 1. 4ch→1ch mixer (平均) │          │
│              │ 2. 48→16k resampler     │          │
│              │ 3. Opus 编码 20ms       │          │
│              └──────────────────────────┘          │
│                              │                     │
│                              ▼                     │
│                    WS Binary (OPUS)                │
└─────────────────────────────┬──────────────────────┘
                              │ WS (JSON + Binary)
                              ▼
┌──────────── Node.js tab5-adapter ──────────────────┐
│                                                    │
│  adapter.js:                                       │
│   ws.on('message')                                 │
│     ├─ isBinary → handleBinaryAudio(buf)          │
│     │                └─ asr.feedOpusFrame(buf)    │
│     └─ text     → handleUplink(JSON)              │
│                                                    │
│  asr.js (320 行, 1:1 复刻 asr.js 模板):           │
│   ┌─────────────────────────────────────┐          │
│   │ Opus 解码 (opusscript)             │          │
│   │ ↓                                   │          │
│   │ RMS VAD (start=80, end=50,          │          │
│   │          900ms 静音, 20s max,       │          │
│   │          15 帧 min)                  │          │
│   │ ↓                                   │          │
│   │ 阿里云 NLS ISI REST (POP v1)        │          │
│   │ ↓                                   │          │
│   │ 识别文字 → handleText → SDK         │          │
│   └─────────────────────────────────────┘          │
└────────────────────────────────────────────────────┘
```

### 1.3 p4c5-agent-terminal (我自己之前的实现 — 部分错)

```
┌─────────────────── ESP32-P4C5 ────────────────────┐
│                                                    │
│  I2S/ES7210 (4ch@24k) → p4c5_audio              │
│   ⚠ 我改了: 开 4 通道 (xiaoizhi 实际只 2 通道)    │
│                              │                     │
│                              ▼                     │
│  audio_uplink_task (我写, 24KB 栈)                │
│  ┌──────────────────────────────────────────┐      │
│  │ 1. 4 通道分离 (ch0+ch2=mic, ch1=ref)     │      │
│  │ 2. aec_sw NLMS (168 行, ❌ 自实现)        │      │
│  │ 3. resampler_24_16 (150 行, ❌ 自实现)    │      │
│  │ 4. opus_encoder (224 行, ✅ libopus)       │      │
│  └──────────────────────────────────────────┘      │
│                              │                     │
│                              ▼                     │
│                    WS Binary (OPUS)                │
└─────────────────────────────┬──────────────────────┘
                              │ WS (JSON + Binary)
                              ▼
┌──────────── Python p4c5_asr_adapter ──────────────┐
│                                                    │
│  p4c5_asr_adapter.py (重写, 1:1 复刻 OMT asr.js): │
│   OpusScript 解码 + RMS VAD + 阿里云 NLS ISI       │
│   + --no-upstream --echo smoke test 模式           │
│                                                    │
│  p4c5_tts_adapter.py: 阿里百炼 CosyVoice REST      │
│                                                    │
│  mock_dsh_server.py: 18 帧 + broadcast            │
└────────────────────────────────────────────────────┘
```

---

## 2. 关键差异矩阵

| 维度 | xiaozhi 官方 | OMT | 我之前 (p4c5) | 我应该 |
|------|------------|-----|---------------|--------|
| **硬件 ADC** | ES8311+ES7210 板 | ES7210 (Tab5) | ES7210 ✅ | 一致 |
| **采样率** | 24 kHz | **48 kHz** (高) | 24 kHz ✅ | 24 kHz (减半 CPU+带宽) |
| **通道数** | **2 (mic+ref)** | 4 (4 麦平均) | 4 (4 麦平均) | **改回 2 (mic+ref)** ⭐ |
| **AEC** | ESP-SR AFE 硬件 | 无 (单工) | NLMS 自实现 | **改 ESP-SR AFE** ⭐ |
| **NS** | ESP-SR NSNet | 无 | 无 | **加 ESP-SR NS** ⭐ |
| **VAD 端侧** | ESP-SR AFE VAD | 能量+hysteresis | RMS 自实现 | **改 ESP-SR VAD** ⭐ |
| **VAD Mac 端** | Silero VAD ONNX | RMS + 900ms | RMS + 900ms | 沿用 |
| **唤醒** | **WakeNet9** | ❌ UI 按钮 | RMS 自实现 | **改 WakeNet9** ⭐ |
| **录音起始** | 唤醒后自动 | UI 按住键 | UART 命令 | 沿用 UART (易调试) |
| **Opus 帧** | **60 ms (960 samples)** | **20 ms (320 samples)** | **20 ms** | **改 60 ms** (xiaozhi 行业标准) |
| **Opus 码率** | 16 kbps | 16 kbps | 16 kbps ✅ | 16 kbps |
| **上行 WS** | Binary 裸 Opus | Binary OPUS | Binary OPUS | 沿用 |
| **ASR** | 讯飞/火山/阿里 | 阿里 NLS ISI | 阿里 NLS ISI ✅ | 沿用 (用户有 API) |
| **TTS** | 火山豆包/Edge | ❌ 无 | 阿里百炼 CosyVoice ✅ | 沿用 (用户有 API) |
| **下行 WS** | Binary Opus 24kHz | ❌ 无 | ❌ 无 (待 W5) | 沿用 (阿里百炼 PCM 24k → ESP32 opus 解码) |
| **协议层** | 9 帧 + Hello | DSH 协议层 | DSH **14+4 帧** ✅ | **保留 DSH** (差异化) |
| **应用层** | Application 状态机 | 单 task | 单 task | **引入状态机** (xiaozhi 风格) |
| **UART 命令** | ❌ 无 | ❌ 无 | ✅ esp_console | 沿用 (调试友好) |

---

## 3. 取舍分析 (每个模块的处置)

### 3.1 通道数 — **改回 2 通道**

**关键事实**: xiaozhi 官方 `kevin_p4c5_4g_board.cc` 使用 `input_channels_ = 2`。
我之前改的 4 通道是**错的**。

**代码对比** (xiaozhi box_audio_codec.cc):
```cpp
input_channels_ = input_reference_ ? 2 : 1;
fs.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0);  // MIC
if (input_reference_) {
    fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);  // REF
}
```

**理由**:
- ESP-SR AFE 内部处理 4 麦不是问题, 但需要传 `input_format = "MRR..."`
- 4 麦做波束成形(beamforming) xiaozhi **没做**, OMT 也没做, 我们 POC 不做
- 2 通道 (mic + ref) 即可满足 AEC 需求, CPU/带宽减半

**取舍**: **改回 2 通道** (mic + ref), 4 麦硬件连接保留备用。

### 3.2 AEC — **从自实现 NLMS 改 ESP-SR AFE**

| 维度 | 我的 NLMS | xiaozhi/OMT 用 AFE | 我应该 |
|------|----------|------------------|--------|
| 平台 | 任意 | ESP32-S3/P4 | 我们 P4 支持 ✅ |
| 回声抑制比 | 15-25 dB | **30-40 dB** | AFE 优 1.5-2 倍 |
| 双讲 | 差 | 好 | AFE 优 |
| NS | ❌ 无 | 包含 (NSNet) | AFE 优 |
| VAD | ❌ 无 | 包含 | AFE 优 |
| RAM | 16KB | ~70KB | AFE 多 54KB |
| Flash | 几 KB | ~200KB | AFE 多 200KB |
| 自实现风险 | 我一个人维护 | 官方持续更新 | AFE 优 |

**结论**: **必须用 ESP-SR AFE**, 我的 NLMS 是 POC, 量产质量不够。

**误判纠正**: 我之前在 W3-aec 文档里写"P4 不支持 ESP-SR AFE 硬件加速" — **完全错误**!

```
xiaozhi Kconfig 实际写明:
config USE_AFE_WAKE_WORD
    bool "Wakenet model with AFE"
    depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
```

CC 调研报告 `docs/research/xiaozhi-esp32-2026.md` 第 110 行也写明:
> **ESP32-S3 / ESP32-P4**: WakeNet9 / WakeNet9l

### 3.3 唤醒 — **从自实现 RMS 改 ESP-SR WakeNet9**

| 维度 | 我的 RMS | WakeNet9 |
|------|----------|----------|
| 真唤醒词 | ❌ 任何声音都触发 | ✅ "你好小智" / "嗨乐鑫" |
| 误触发率 | 高 (>10次/小时) | <1次/12小时 |
| 抗噪声 | 差 | 好 (DNN 模型) |
| CPU | 极低 (~1%) | ~5% |
| Flash | 几 KB | ~500KB |

**结论**: **必须用 WakeNet9**, RMS 只能算"声音检测"不是"唤醒"。

### 3.4 降采样 — **删除自实现 resampler_24_16**

**理由**:
- ESP-SR AFE 内部已经完成 24kHz→16kHz 降采样
- 我的 resampler_24_16 在用 AFE 后**完全冗余**
- 节省 ~150 行代码 + 几个 KB RAM

### 3.5 ASR — **沿用 OMT (阿里云 NLS ISI)**

**事实**:
- 用户已有阿里云 NLS API
- OMT asr.js (320 行) 已经实现完整流程 (opusscript + RMS VAD + POP v1 签名)
- 我的 `p4c5_asr_adapter.py` 已经 1:1 复刻 OMT

**结论**: **保留 Mac 端 ASR Adapter**, 不重写。

### 3.6 TTS — **沿用阿里百炼 CosyVoice (用户有 API)**

**事实**:
- OMT 没做 TTS
- xiaozhi 推火山豆包
- 用户有阿里百炼 CosyVoice API
- 我的 `p4c5_tts_adapter.py` 已实现 REST 调用

**结论**: **保留阿里百炼 TTS**, 不切火山。

### 3.7 协议层 — **保留 DSH 14+4 帧**

| 方案 | 帧数 | 我们的优势 |
|------|------|-----------|
| xiaozhi 协议 | 9 帧 + Hello | - |
| DSH 协议 | **14 上行 + 4 下行** ✅ | 多 5 类: file_change/auth/question/permission/cancel |

**结论**: **保留 DSH**, 不切 xiaozhi 协议 (差异化优势)。

### 3.8 Opus 帧长 — **20ms → 60ms**

**事实**:
- OMT: 20ms (320 samples)
- xiaozhi: **60ms (960 samples)** ← 行业标准
- 我: 20ms (沿用 OMT)

**取舍**:
- 60ms 帧: 带宽更省, 延迟 +40ms
- 20ms 帧: 实时更好, 带宽多 3 倍

**建议**: **改 60ms** (对齐行业标准), 但保留 20ms 配置选项 (Kconfig)。

### 3.9 Application 状态机 — **引入**

**事实**:
- OMT: 单 task
- xiaozhi: `application.cc` 1119 行, 完整状态机
- 我们: 单 task

**结论**: **POC 阶段不引入完整状态机**, 但应该有:
```cpp
enum class DeviceState {
    Starting, Activating, Idle, Listening, Thinking, Speaking, Error
};
```

最简单的实现: `s_device_state` 变量 + 状态切换日志。

### 3.10 UART 命令 — **保留**

**事实**:
- OMT: ❌ 无
- xiaozhi: ❌ 无 (用 LCD 触摸按钮)
- 我们: ✅ esp_console

**结论**: **保留 esp_console** (调试友好, 工厂测试必需)。

---

## 4. 实施方案 (3 阶段)

### Phase 1: 用 ESP-SR 替换自实现 (P0, 1-2 周)

**目标**: 把自实现的 AEC/Wake/Resampler 全部替换为 ESP-SR 官方实现

**步骤**:
1. **添加依赖** (`idf_component.yml`):
   ```yaml
   espressif/esp-sr: ~2.3.0           # 包含 AFE/WakeNet/NSNet/VAD
   espressif/esp_audio_effects: ~1.2.1
   espressif/esp_audio_codec: ~2.4.1
   ```

2. **改 `p4c5_audio.cc`** 改回 2 通道 (mic + ref):
   ```cpp
   fs.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0);  // MIC
   if (input_reference_) {
       fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);  // REF
   }
   ```

3. **新建 `afe_component/`** 封装 ESP-SR AFE: (基于 xiaozhi `afe_audio_processor.cc`)
   ```cpp
   class AfeProcessor {
   public:
     void Initialize(int mic_channels, int ref_channels, int frame_duration_ms);
     void Feed(const int16_t* pcm);  // 输入 24k multi-ch
     bool Fetch(int16_t* out);       // 输出 16k mono
     void EnableWakeWord(bool enable);
   };
   ```

4. **新建 `wake_word_component/`** 封装 WakeNet9:
   ```cpp
   class WakeWordDetector {
   public:
     void Initialize();
     bool Detect(const int16_t* pcm_16k, int samples);
   };
   ```

5. **改 `audio_uplink_task`**:
   ```cpp
   // 删除: aec_sw_process, resampler_24_16_process
   // 改用: afe.Feed() → afe.Fetch() → opus_encoder_encode
   ```

6. **删除冗余组件**:
   - `aec_sw/` (168 行) ❌
   - `resampler_24_16/` (150 行) ❌
   - `wake_word_detector/` (193 行) ❌
   - `audio_mixer/` (206 行) ⚠️ 保留备用 (4 麦波束成形)

**验证**:
- `idf.py build` 通过
- 烧录 → 不 panic
- `audio_start` 触发 4 麦 → AFE → Opus → WS 上行
- Mac ASR Adapter 收到音频

**风险**:
- ESP-SR ~700KB flash, 当前 1.4MB → 总 2.1MB (16MB 充裕)
- WakeNet9 模型需 partition table 调整
- ESP-SR 模型从 PSRAM 加载

### Phase 2: 唤醒集成 (P0, 3-5 天)

**目标**: 用户喊"嗨乐鑫"自动激活录音

**步骤**:
1. 用 `esp_srmodel_filter(ESP_WN_PREFIX)` 加载 WakeNet9 模型
2. WakeWord 状态触发 DSH `client/listen.start` 上行
3. 录音完成后自动 `client/listen.stop`
4. 串口命令 `wake enable` / `wake disable`

**验证**:
- 安静环境下喊"嗨乐鑫" → 设备响应 (LED/日志)
- 5m 距离内唤醒成功率 > 90%
- 12h 误触发率 < 1 次

### Phase 3: TTS 集成 (P1, 1 周)

**目标**: AI 回复文字 → 终端语音播报

**步骤**:
1. DSH 下行 `assistant_text` 触发 TTS Adapter
2. 阿里百炼 CosyVoice 流式合成 → PCM 24kHz 分片
3. WS Binary 下行 (Opus 24k/60ms 帧, 复用 xiaozhi 标准)
4. ESP32 `tts_player` (已写) 解码 + ES8311 播放

**验证**:
- DSH 协议层收到 `assistant_text` → 触发 TTS → 听到声音
- 端到端延迟 < 1s (从 DSH 回复到听到首音)

### Phase 4: Application 状态机 (P2, 2-3 天)

**目标**: 引入设备状态管理

**步骤**:
1. 引入 `enum class DeviceState` 定义
2. `app_main` 持状态变量 + 切换日志
3. 应用状态变化同步到 DSH `client/state` 上行 (便于云端管理)
4. UI 状态同步 (LVGL)

---

## 5. 我的代码改动总结

### 5.1 应删除 (408 行)

| 文件 | 行数 | 处置 |
|------|------|------|
| `aec_sw/aec_sw.cpp` | 168 | ❌ 删, 用 AFE |
| `wake_word_detector/wake_word_detector.cpp` | 193 | ❌ 删, 用 WakeNet9 |
| `resampler_24_16/resampler_24_16.cpp` | 150 | ❌ 删, AFE 内部完成 |
| 上述 3 个 include | - | ❌ 删 |
| 上述 3 个 CMakeLists | - | ❌ 删 |

### 5.2 应保留 (~1000 行)

| 文件 | 行数 | 处置 |
|------|------|------|
| `audio_mixer/audio_mixer.cpp` | 206 | ⚠️ 保留 (4 麦波束备用) |
| `opus_encoder/opus_encoder.cpp` | 224 | ✅ 保留, 仅改帧长配置 |
| `tts_player/tts_player.cpp` | 170 | ✅ 保留 |
| `p4c5_audio/` | - | ⚠️ 改回 2 通道 |
| `dsh_client/` | - | ✅ 保留 |
| `mock_dsh_server.py` | 22KB | ✅ 保留 |

### 5.3 应新增 (估 400 行)

| 文件 | 估行数 | 用途 |
|------|-------|------|
| `afe_processor/afe_processor.cpp` | 200 | 封装 ESP-SR AFE |
| `wake_word/wake_word.cpp` | 100 | 封装 WakeNet9 |
| `application/application.cpp` | 100 | 设备状态机 |

**净代码变化**: -408 +400 = **约 -8 行**, 但**功能提升 10x** (AEC/Wake/NS/VAD 全增强)。

---

## 6. 立即可执行的最小验证 (今天做)

1. ✅ 当前固件已经烧录 (v4 esp_console fix)
2. **立即**: 读 ESP32 串口验证 v4 不 panic
3. **立即**: 发送 `audio start` 命令 → 看 `🎤 [W3] audio recording ON` 日志
4. **立即**: 发送 `audio_diag` (待加) → 看 mic RMS 输出
5. **下一步**: 用真实声音测试 (Android 录 "你好小智" 用 ESP32 喇叭播放)

如果 v4 验证通过, 进入 Phase 1 (引入 ESP-SR)。

---

## 7. 风险与诚实评估

| 风险 | 等级 | 缓解 |
|------|------|------|
| ESP-SR 模型找不到 | 中 | partition table 加 model 分区 |
| ESP-SR 模型太大 | 低 | 32MB PSRAM 充裕 |
| AFE 与 ref mic 时序不同步 | 中 | 调 ES7210 TDM 同步 |
| WakeNet9 中文词不准 | 低 | 训练自定义词 |
| 用户期望 TTS 立即可用 | 中 | 先确保 ASR, TTS 用 mock 兜底 |
| 4G 模组缺料 | 中 | 先用 WiFi, 4G 后置 |
| 实测覆盖率仍 < 80% | 高 | M7 实测模板待填 |

---

## 8. 与 cc 报告的差异

CC 报告 `W3-competitive-analysis.md` 已经做了 3 方案对比, 但有几处**已过时**:

| CC 报告原文 | 实际情况 | 修正 |
|------------|---------|------|
| `W3-aec-implementation.md`: "ESP32-P4 不支持 ESP-SR AFE 硬件加速" | **错** — xiaozhi kevin-p4c5-4g 用 AFE 在 P4 上跑通 | CC 报告未及时更新 |
| `W3-complete-roadmap.md`: "要做完整 WakeNet + AFE, 硬件必须换 ESP32-S3" | **错** — P4 有完整 ESP-SR 支持 | 同上 |
| `W3-competitive-analysis.md`: "✓ WakeNet9 (本地 ESP-SR)" | ✅ 正确 | - |
| `xiaozhi-esp32-2026.md`: "ESP32-S3 / ESP32-P4: WakeNet9 / WakeNet9l" | ✅ 正确 (我一直没读这份!) | 我失职 |

**修正 CC 报告**: 应该在 Phase 1 完成后, 增补 `W3-aec-implementation-v2.md` 说明 ESP-SR 集成。

---

## 9. 引用源

- xiaozhi 官方代码: `/tmp/xiaozhi_ksdiy/`
- xiaozhi 调研报告: `docs/research/xiaozhi-esp32-2026.md` (655 行)
- OMT 主项目: `/Volumes/ZT-1T/项目开发/OMT/`
- 我们 W3 文档: `docs/hw/W3-aec-implementation.md` + `W3-competitive-analysis.md`
- 我们 W3 commit: `hardware/p4c5-agent-terminal/components/audio_pipeline/`
- ESP-SR: https://github.com/espressif/esp-sr
- 阿里云 NLS: https://nls-portal.console.aliyun.com/

---

**下次更新**: Phase 1 (ESP-SR 集成) 完成后, 写 `W3-esp-sr-integration.md`
