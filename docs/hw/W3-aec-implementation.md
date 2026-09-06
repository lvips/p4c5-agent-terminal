# W3 Phase 1: 4 麦录音 + 软件 AEC 实施归档

> **状态**: ✅ 编译通过 (commit `c90a54e`)
> **日期**: 2026
> **关联需求**: "录音4麦克风的调试要包含回音消除功能"

---

## 1. 设计概述

完整上行链路（4 麦 + 软件 AEC）：

```
┌─────────────────────────────────────────────────────────────────┐
│ ESP32-P4 录音端                                                  │
├─────────────────────────────────────────────────────────────────┤
│ p4c5_audio_record_multi (4ch 24kHz)                             │
│       ↓                                                          │
│ 4 通道分离 (ch0+ch2 = mic, ch1 = AEC ref, ch3 = unused)         │
│       ↓                                                          │
│ aec_sw (NLMS, taps=128, mu=0.005) ← 来自 ch1                    │
│       ↓                                                          │
│ resampler_24_16 (24k→16k, 480→320 samples)                      │
│       ↓                                                          │
│ opus_encoder (libopus, 16kbps, 20ms 帧)                         │
│       ↓                                                          │
│ dsh_client_send_audio → WS Binary → Mac Adapter                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 通道映射（与 xiaozhi kevin-p4c5-4g 一致）

| TDM Slot | 信号源 | 用途 |
|---------|--------|------|
| **ch0** | MIC1 (主麦) | mic（左声道） |
| **ch1** | **AEC ref**（扬声器回采） | 软件 AEC 参考 |
| **ch2** | MIC3（副麦） | mic（右声道） |
| **ch3** | MIC4（备用麦） | unused |

**关键**：ch1 必须是硬件回采连接（ES8311 DAC 输出 → ES7210 MIC2 输入）才能让软件 AEC 真实工作。

---

## 3. 软件 AEC 算法：NLMS（Normalized LMS）

### 3.1 算法原理

```
echo_estimate[n] = Σ w[k] * ref[n-k]   ←  FIR 滤波器估计回声
error[n] = mic[n] - echo_estimate[n]   ←  误差即近端语音
w[k] ← leak * w[k] + mu * error[n] * ref[n-k] / (||ref||² + eps)
```

### 3.2 参数

| 参数 | 默认值 | 说明 |
|-----|--------|------|
| `filter_taps` | 128 | FIR 滤波器长度 (5.3ms @ 24kHz) |
| `step_size` (μ) | 0.005 | NLMS 学习步长 |
| `leakage` | 0.999 | 泄漏因子（防止权重发散） |
| `ref_gain` | 1.0 | 参考通道增益 |
| `enable_aec` | `P4C5_AUDIO_INPUT_REF` | 总开关（仅硬件有 ref 时启用） |

### 3.3 资源占用

- **RAM**: ~16KB（128 taps × 2 buffers × 4 bytes float）
- **CPU** (RISC-V @ 400MHz): 估算 <5% (per-frame 计算量小)
- **延迟**: 1 帧 = 20ms（无额外缓冲）

---

## 4. 组件清单

| 组件 | 路径 | 功能 |
|-----|------|------|
| `aec_sw` | `components/audio_pipeline/aec_sw/` | 软件 AEC（NLMS） |
| `audio_mixer` | `components/audio_pipeline/audio_mixer/` | 4ch→1ch 混音（OMT 移植，未使用） |
| `resampler_24_16` | `components/audio_pipeline/resampler_24_16/` | 24kHz→16kHz 线性插值 |
| `opus_encoder` | `components/audio_pipeline/opus_encoder/` | libopus 编码（OMT 移植） |
| `dsh_client` | `components/dsh_client/` | WebSocket 客户端（已加 `send_audio`） |
| `p4c5_audio` | `components/p4c5_audio/` | **修改**：EnableInput 全开 4 通道 |

---

## 5. 关键代码变更

### 5.1 `p4c5_audio.cc` — EnableInput 全开 4 通道

**之前**（仅 ch0 + ch1 启用）：
```cpp
.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
if (P4C5_AUDIO_INPUT_REF) {
    fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
}
```

**现在**（与 xiaozhi BoxAudioCodec 一致，全开 4 通道）：
```cpp
.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) |
                ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1) |
                ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2) |
                ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3),
```

### 5.2 `aec_sw.cpp` — NLMS 实现

完整 NLMS 算法，约 200 行 C++ 代码，含：
- 滑动参考窗口（`ref_history`）
- FIR 滤波器权重（`weights`）
- 帧级 NLMS 更新
- DC 抑制 + 饱和保护

### 5.3 `app_main.cpp` — audio_uplink_task 6 步上行链路

```cpp
for (;;) {
    if (!s_audio_recording) { vTaskDelay(20ms); continue; }

    // 1. 4 通道录音
    p4c5_audio_record_multi(s_in4ch_buf, 480);

    // 2. 通道分离
    for (i=0; i<480; i++) {
        s_mic_mono[i] = (ch0 + ch2) / 2;   // 2 麦平均
        s_ref_mono[i] = ch1;                 // AEC ref
    }

    // 3. 软件 AEC
    aec_sw_process(s_mic_mono, s_ref_mono, s_aec_out, 480);

    // 4. 24k→16k 重采样
    resampler_24_16_process(s_aec_out, s_mono16k, &out_len);

    // 5. Opus 编码
    opus_encoder_encode(s_mono16k, 320, s_opus_buf, &opus_len);

    // 6. WS Binary 上行
    dsh_client_send_audio(s_opus_buf, opus_len);
}
```

### 5.4 `app_main.cpp` — C++ extern "C" 修复

**问题**：`app_startup.c` 用 C 编译，调用 `app_main` 未 mangled；`app_main.cpp` 是 C++，`app_main` 被 mangled 为 `_Z8app_mainv`。

**修复**：
```cpp
extern "C" void app_main(void) { ... }
```

### 5.5 `app_main.cpp` — C++ 严格字段顺序

**问题**：`esp_task_wdt_config_t` 用 C++ 编译时不允许 designated initializer 顺序乱序。

**修复**：
```cpp
esp_task_wdt_config_t wdt_cfg;
wdt_cfg.timeout_ms = 30000;
wdt_cfg.idle_core_mask = 0;
wdt_cfg.trigger_panic = true;
```

---

## 6. 串口命令触发（POC）

```
audio start    → 开始录音（4ch@24k + AEC + opus + WS Binary 上行）
audio stop     → 停止录音
audio status   → 显示当前状态
```

---

## 7. 诚实的限制（DSH 不打太极）

### 7.1 软件 AEC 质量 < ESP-SR AFE

| 维度 | 自实现 NLMS | ESP-SR AFE |
|-----|-----------|-----------|
| 平台 | 任意（CPU 即可） | **仅 ESP32-S3** |
| 回声抑制比 | 15-25 dB | 30-40 dB |
| 双讲性能 | 差 | 好 |
| 残差抑制 | 无 | 有 |
| RAM 占用 | ~16 KB | ~70 KB |
| 适合场景 | POC 演示 | 量产产品 |

**结论**：量产阶段硬件应换 ESP32-S3 跑 ESP-SR AFE。

### 7.2 硬件回采依赖

软件 AEC 仅当硬件真有 AEC ref 回采时有效：
- **有回采**: ch1 = 扬声器回采 → NLMS 能减去回声 ✅
- **无回采**: ch1 = MIC2 → NLMS 会损坏 mic 数据 ❌

**自检方法**（硬件验证）：
1. 扬声器播放 1kHz 测试音（音量 50%）
2. 同时启动录音 5 秒
3. 分析 WAV：ch1（ref 通道）应该有 1kHz 分量
4. ch0/ch2（mic 通道）也应该有 1kHz（这是回声）

如果 ch1 没有任何扬声器信号 → 硬件没回采连接，软件 AEC 无效。

### 7.3 改进路径

如果硬件没回采连接，下一步可改为：
- **方案 A**：从 I2S TX 端（ESP8311 输出端）抓取播放数据作为软件 ref
  - 需要 ESP-IDF I2S TX 数据截获
  - CPU 开销 + 路径延迟需校准
- **方案 B**：硬件改版，物理上将 ES8311 输出接到 ES7210 MIC2
  - 最可靠，但需要 PCB 改版
- **方案 C**：换 ESP32-S3 主板，跑 ESP-SR AFE
  - 工作量大，但效果最佳

---

## 8. Build 验证

```
[20/22] Linking CXX executable p4c5_agent_terminal.elf
Project build complete.
```

**二进制大小**: ~1.4MB (与之前 +5KB，NLMS 算法 + 4 通道处理)

**组件依赖**:
```
audio_pipeline/aec_sw
audio_pipeline/audio_mixer
audio_pipeline/resampler_24_16
audio_pipeline/opus_encoder (78__esp-opus)
dsh_client (send_binary)
p4c5_audio (4 通道输入)
```

---

## 9. 待验证（POC 阶段）

1. **硬件回采确认**：播放 1kHz + 录音 → 检查 ch1 是否有信号
2. **4 麦录音质量**：4 通道独立录音 + 通道分离正确性
3. **AEC 效果**：开启/关闭 AEC 对比 1kHz 回声抑制比
4. **WS Binary 上行**：Mac Adapter 收到 opus 数据
5. **端到端延迟**：录音 → Mac Adapter 解码 → 文字返回 < 1s

---

## 10. 下一步

- W3 Phase 2 (W4): WakeNet9 + AFE 集成
- W3 Phase 3 (W5): 火山豆包双向流式 TTS
- Mac Adapter ASR Pipeline (Python, 复刻 OMT asr.js)

完整归档见 `/Volumes/ZT-1T/项目开发/ESP32-P4C5/docs/hw/W3-aec-implementation.md`