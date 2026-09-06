# W3 计划 — ESP-SR 集成 (语音识别)

> **目标**: 补齐 ESP32-P4C5 终端的语音识别/语音转文字能力
> **前置**: W1 (WiFi) ✅ + W2 (DSH 协议层) ✅
> **状态**: 🚧 计划中

---

## 🎯 W3 验收标准

### 功能验收

| 能力 | 当前状态 | 目标 |
|---|---|---|
| WakeNet 唤醒词 | ❌ 无 | "小智小智" / 自定义唤醒词 |
| ASR 语音转文字 | ❌ 无 | 中文普通话 99% 准确 |
| VAD 语音活动检测 | ❌ 无 | 自动检测用户说话开始/结束 |
| AEC 回声消除 | ✅ 硬件 | 与 esp-sr AFE 集成 |
| 录音 → 文本 → DSH 客户端 | ❌ 无 | 一键完成, 文本通过 W2 协议层上行 |

### 性能验收

| 指标 | 目标 |
|---|---|
| 唤醒响应延迟 | < 200ms |
| 唤醒准确率 | > 95% (3m 内) |
| ASR 准确率 | > 90% (中文普通话) |
| ASR 延迟 | < 1s (说完 → 文字输出) |
| 待机功耗 | < 100mA (仅 WakeNet) |

---

## 📋 ESP-SR 评估

### esp-sr (Espressif 官方方案)

| 项 | 详情 |
|---|---|
| 组件名 | `espressif/esp-sr` |
| 最新版本 | 1.7.x (2024+) |
| 文档 | https://github.com/espressif/esp-sr |
| WakeNet 模型 | `wakenet_model` (内置中文唤醒词 "嗨乐鑫") |
| MultiNet 模型 | `mn5q8_cn" / `mn6_cn` (中文命令词) |
| AFE (Audio Front-End) | AEC + VAD + NS (噪声抑制) 一体化 |
| Flash 占用 | WakeNet ~500KB, MultiNet ~3MB, AFE ~200KB |
| PSRAM 占用 | 模型运行时 ~2MB |

### 集成挑战

1. **PSRAM 限制**: ESP32-P4 有 32MB PSRAM, 模型可以放 PSRAM
2. **CPU 占用**: AFE + WakeNet 单核即可, ASR 需要较高算力
3. **录音格式**: AFE 要求 16kHz 16bit mono (vs p4c5_audio 当前 24kHz TDM 4ch)
4. **需要降采样**: 4 ch 24kHz → 1 ch 16kHz
5. **需要选择模型**: WakeNet (唤醒) + MultiNet (命令词) vs 完整中文 ASR

---

## 📋 实施步骤

### Step 1: 调研 + POC (1-2 天)

**目标**: 验证 esp-sr 在 ESP32-P4 上能否运行, 模型大小 / 内存占用

```bash
# 1. 加依赖
idf.py add-dependency "espressif/esp-sr^1.7.0"

# 2. 测试编译, 看 flash/PSRAM 占用
idf.py size-components

# 3. 跑 esp-sr 官方例程 (esp-sr/tools/keyword_detection)
idf.py -p /dev/cu.usbmodem1301 flash monitor
```

### Step 2: 录音 + AFE 集成 (2-3 天)

**目标**: 把 p4c5_audio 4 ch 24kHz TDM → esp-sr 1 ch 16kHz

```c
// 1. 启动 p4c5_audio 录音 (4 ch 24kHz)
p4c5_audio_record_start();

// 2. 后处理: 取 ch0, 24k → 16k 降采样 (1/3 + 1/2 抽样)
void* ch0_buffer = p4c5_audio_get_input_buffer();
int16_t* sr_buffer = downsample_24k_to_16k(ch0_buffer, len);

// 3. 喂给 esp-sr AFE
esp_afe_feed(afe_data, sr_buffer, sr_samples);
```

### Step 3: WakeNet 集成 (1-2 天)

**目标**: 检测到唤醒词触发 DSH 录音流程

```c
while (1) {
    afe_fetch_result_t* res = esp_afe_fetch(afe_data);
    if (res->wakeup_state == WAKENET_DETECTED) {
        ESP_LOGI(TAG, "Wake word detected!");
        // 触发录音流程 → user_input 上行
        start_recording_to_dsh();
    }
}
```

### Step 4: 端到端测试 (1-2 天)

**测试流程**:
1. 用户说"小智小智"
2. WakeNet 触发
3. 启动录音
4. 用户说"今天天气怎么样"
5. ASR 转文字 "今天天气怎么样"
6. 通过 DSH 协议层上行 (`client/user_input`)
7. PC Adapter 收到文字, LLM 推理
8. 响应文本下行 (`assistant_text`)
9. ESP32 收到, **目前只能文字显示, TTS 待 W4**

---

## 🛡️ 风险

| 风险 | 缓解 |
|---|---|
| 模型太大超过 flash | 选择 lite 模型 (WakeNet + MultiNet 命令词), 不做完整 ASR |
| 降采样性能不够 | 用硬件 I2S 重配置 16kHz 直采 (避免软件降采样) |
| 唤醒词不识别 | 提供"嗨乐鑫"内置词测试, 再训练自定义词 |
| PC Adapter 协议层不支持音频流 | 已确认 W2 协议层用文本 user_input, ASR 在 ESP32 端做, 输出仍是文本 |
| 与 xiaozhi 冲突 | xiaozhi 也是 esp-sr 用户, 直接借鉴其 AFE 配置 |

---

## 🔄 替代方案对比

### 方案 A: ESP-SR 本地 ASR (推荐)
- **优点**: 离线可用, 隐私好, 响应快
- **缺点**: 模型大 (~3MB), 中文需训练或用内置命令词
- **资源**: flash ~4MB, PSRAM ~2MB

### 方案 B: 云端 ASR (OpenAI Whisper 等)
- **优点**: 准确率高, 不占 ESP32 资源
- **缺点**: 网络依赖, 延迟高, 隐私差
- **资源**: 0 (云端处理)

### 方案 C: 简化方案 (只 WakeNet + 命令词)
- **优点**: 资源占用最小 (~500KB)
- **缺点**: 只能识别"打开灯"等命令, 不能识别任意中文
- **资源**: flash ~500KB

### 我的建议

**Phase 1**: 方案 C (WakeNet + MultiNet 命令词) — 快速验证
**Phase 2**: 方案 A (完整 ASR) — 体验升级
**Phase 3**: 视情况补方案 B (云端 ASR) — 备用

---

## 📚 参考

- [esp-sr GitHub](https://github.com/espressif/esp-sr)
- [esp-sr 文档](https://docs.espressif.com/projects/esp-sr/en/latest/index.html)
- [xiaozhi 集成示例](https://github.com/78/xiaozhi-esp32) (参考 AFE 配置)
- [W2-completion-summary.md](W2-completion-summary.md) - 协议层验证完成