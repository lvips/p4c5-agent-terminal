# 综合分析报告 — p4c5-agent-terminal 语音识别与信息流方案对比

> **调研日期**: 2026-09-07
> **调研范围**: xiaozhi-esp32 (主流) / OMT (内部) / p4c5-agent-terminal (我们)
> **目标**: 提出 p4c5 项目 W3+ 的合理化建议

---

## 一、3 个方案概览

### 1.1 xiaozhi-esp32 (行业标杆)
- **规模**: 25~29.2K stars, **120 万台部署**, 日均 900 万对话 / 270 亿 Tokens
- **模式**: **本地 WakeNet 唤醒 + 云端 ASR/LLM/TTS + WebSocket Binary Opus 流 + MCP 物联网**
- **音频**: Opus 16kHz/60ms 上行, 24kHz/60ms 下行 (强制, 行业事实标准)
- **协议**: WebSocket + JSON (14 种) + Binary (3 个版本演进)
- **推荐**: 火山引擎豆包双向流式 TTS (0.317s 首音)

### 1.2 OMT (Tab5, 内部项目, 主线 A)
- **硬件**: M5Stack Tab5 (ESP32-P4 + SIM-A7680C 4G)
- **模式**: **ESP32 端只做录音 + VAD + OPUS 编码**, Mac adapter 调**阿里云 DashScope 流式 ASR**
- **音频**: 4ch 48kHz → 1ch 16kHz OPUS 16kbps 20ms 帧 → WebSocket Binary
- **ASR**: 阿里云 DashScope `qwen3-asr-flash-realtime` (28 种语言, 情绪识别)
- **VAD**: 两层 - ESP32 能量 VAD + Mac adapter RMS VAD + 900ms END_HOLD
- **TTS**: ❌ 暂无 (Tab5 只显示文字)
- **唤醒**: ❌ 暂无 (用 UI 按钮触发录音)

### 1.3 p4c5-agent-terminal (我们, 当前)
- **硬件**: ESP32-P4 + ESP32-C5 (SDIO WiFi) ✅ + ES7210 4 麦 ✅
- **已完成**: Board / PMIC / Display / Audio codec / WiFi / DSH 协议层 (14+4 帧) ✅
- **缺失**: ❌ WakeNet / ❌ ASR / ❌ TTS / ❌ VAD / ❌ OPUS 编码
- **DSH 协议层**: WebSocket + JSON-only (14+4 帧), 无 Binary 通道
- **当前数据流**: 纯文本 user_input (W2 测试用) — 没有真实音频流

---

## 二、关键差异对比表

| 维度 | xiaozhi | OMT | p4c5 (我们) |
|---|---|---|---|
| **唤醒** | ✅ WakeNet9 (本地 ESP-SR) | ❌ UI 按钮 | ❌ 无 |
| **VAD** | ✅ Silero VAD (本地 ONNX) | ✅ 两层能量+RMS | ❌ 无 |
| **音频编码** | ✅ Opus 16k/24k 60ms | ✅ Opus 16k 20ms | ❌ 无 (W2 用文本) |
| **传输** | WS Binary + JSON 双通道 | WS Binary + JSON 双通道 | ❌ WS JSON-only |
| **ASR** | 云端 (FunASR/讯飞/火山) | 阿里云 DashScope 流式 | ❌ 无 |
| **TTS** | 云端 (火山豆包推荐) | ❌ 无 | ❌ 无 |
| **MCP 物联网** | ✅ JSON-RPC 2.0 | ❌ 无 | ❌ 无 |
| **麦克风** | 1~2 麦 (INMP441) | 4 麦 (待确认) | ✅ **4 麦 ES7210** ⭐ |
| **AEC 回声** | ✅ AFE 内置 | ✅ 硬件 | ✅ p4c5_audio |
| **服务器** | 云端 (xiaozhi.me) | 本地 PC Adapter (CC 进程) | 本地 PC Adapter (mock_dsh) |
| **延迟** | 云端 1~3s | 本地 <500ms | 本地 <500ms |
| **部署规模** | 120 万台 | 内部测试 | 内部测试 |

### 我们独家优势 ⭐

| 项 | 详情 |
|---|---|
| **4 麦阵列 (ES7210)** | xiaozhi 用 1~2 麦 INMP441, 我们 4 麦可做波束成形 |
| **本地 PC Adapter** | 无需云端, 隐私好, 延迟低, 离线可用 |
| **DSH 协议层 14+4 帧** | 已验证完整, 比 xiaozhi 9 帧更细分 (多 5 类: file_change/auth/question) |
| **不用支持 171 板型** | 做减法, 聚焦 ESP32-P4+C5 |
| **不用支持多 ASR/TTS 服务** | 选 1-2 个最合适的, 不做兼容性 |

### 我们缺失能力 🔴

| 项 | 影响 | 优先级 |
|---|---|---|
| ❌ **唤醒词** | 用户必须按按钮, 不能"喊一声"激活 | P0 |
| ❌ **VAD** | 录音没起止检测, 无法判断用户说话结束 | P0 |
| ❌ **ASR** | 完全无法听懂用户说话 | P0 |
| ❌ **TTS** | AI 回复只能文字, 不能语音播报 | P1 |
| ❌ **OPUS 编码** | 用 PCM 直传浪费带宽 4-8 倍 | P0 |
| ❌ **WS Binary 通道** | 音频帧必须 base64 嵌入 JSON (+33% 浪费) | P1 |

---

## 三、关键决策点

### 3.1 唤醒方案：本地 vs 云端

| 方案 | 优点 | 缺点 | 推荐 |
|---|---|---|---|
| **本地 WakeNet9** (ESP-SR) | 离线可用, 隐私好, 响应快 (<200ms), 省电 (~80mA 待唤醒) | 占用 flash 2MB, 需训练自定义词 | ✅ **强推** |
| 云端唤醒 (Snowboy / 阿里) | 模型可云端升级 | 永远在线, 隐私差, 依赖网络 | ❌ |
| 不做唤醒 (UI 按钮触发) | 简单 | 必须用手, 不是"语音助手" | ❌ |

**决策**: ✅ **集成 ESP-SR WakeNet9**, 用内置"嗨乐鑫"或自训练"你好小智"

### 3.2 ASR 方案：本地 vs 云端

| 方案 | 中文 WER | 资源占用 | 延迟 | 推荐 |
|---|---|---|---|---|
| **ESP-SR MultiNet7_cn** (端侧) | 97.2% | 1-3MB flash, 200KB RAM | <300ms | 命令词 only |
| ESP-SR + SenseVoice (sherpa-onnx) | 5-8% CER | 230MB (太大) | <300ms | ❌ ESP32-P4 不够 |
| **阿里云 Paraformer (云端流式)** | ≈5% | 0 ESP32 资源 | <500ms | ✅ **首选** |
| **讯飞 WebAPI (云端流式)** | ≈5% | 0 ESP32 资源 | ≈300ms | ✅ 中文最强 |
| **火山引擎 ASR (云端流式)** | 4-6% | 0 ESP32 资源 | <500ms | ✅ 豆包生态 |
| OpenAI Whisper API | ≥10% 中文 | 0 | 非流式 | ❌ 不推荐 |

**决策**: ✅ **Mac Adapter 调阿里云 Paraformer 流式 ASR** (与 OMT 保持一致, 复用阿里云 SDK)

### 3.3 TTS 方案

| 方案 | 延迟 | 中文音色 | 音质 | 推荐 |
|---|---|---|---|---|
| **火山豆包双向流式 TTS** | **0.317s 首音** | 40+ | ⭐⭐⭐⭐ | ✅ **xiaozhi 推荐** |
| 讯飞超拟人 | <300ms | 80+ | ⭐⭐⭐⭐⭐ | ✅ 中文最强 |
| Edge TTS (免费) | 200-500ms | 300+ | ⭐⭐⭐⭐ | ✅ 开发原型 |
| OpenAI TTS | 200-500ms | 6 个英文 | ⭐⭐⭐⭐ | ❌ 中文弱 |
| ESP-SR TTS (端侧) | 实时 | 机械 | ⭐⭐⭐ | ❌ 仅断网应急 |

**决策**: ✅ **Mac Adapter 调火山豆包双向流式 TTS** (与 xiaozhi 推荐一致)

### 3.4 音频编码

| 编码 | 16kHz mono 码率 | 延迟 | ESP32-P4 CPU | 推荐 |
|---|---|---|---|---|
| PCM | 256 kbps | 0 | 0% | ❌ 浪费带宽 |
| **Opus 16-64 kbps** | **16 kbps** | **2.5-60ms** | **20-35%** | ✅ **首选** |
| MP3 | 64-128 kbps | 高 | 45-60% | ❌ 专利+CPU |
| AAC-LC | 32-96 kbps | 中 | 40-55% | 备选 |
| G.711 | 64 kbps | 低 | 15-25% | 对接 PBX |

**决策**: ✅ **Opus 16kHz mono 60ms 上行, 24kHz 60ms 下行** (xiaozhi 强制, 行业标准)

### 3.5 传输协议

| 方案 | 延迟 | 带宽 | 复杂度 | 推荐 |
|---|---|---|---|---|
| WS + JSON (DSH 当前) | 中 | 高 (base64 +33%) | 低 | 控制消息 |
| **WS + Binary Opus (xiaozhi)** | **低** | **低** | 中 | **实时语音首选** |
| gRPC + Protobuf | 低 | 低 | 高 | 服务端内部 |
| MQTT + JSON | 高 | 中 | 中 | IoT 传感器 |
| HTTP + SSE | 中 (单向) | 中 | 低 | 文本流式 |

**决策**: ✅ **演进到 WS Binary + JSON 双通道**, 参考 xiaozhi 14 帧设计

---

## 四、合理化建议

### 4.1 演进路线 (3 阶段)

#### **Phase 1 (W3): 音频编码 + VAD + 传输** (1-2 周)

**目标**: 把 W2 协议层扩展为支持真实音频流

**任务**:
1. ESP32 端集成 libopus (16kHz mono 60ms 帧)
2. p4c5_audio 4ch 48kHz → 1ch 16kHz (重采样)
3. 加 VAD 任务 (ESP32 端基于能量阈值)
4. 录音按钮触发 (`s_app_recording` 模式)
5. WS Binary 帧上行 Opus 数据
6. PC Adapter 端: asr.js 复用 OMT 实现 (接收 Opus → 调阿里云 ISI/Paraformer)
7. DSH 协议层扩展: 加 `client/audio` 上行帧 + `server/transcript` 下行帧

**复用工件**:
- OMT 的 `audio_input` / `opus_encoder` / `audio_mixer` / `resampler_48_16` 组件 (如可移植)
- OMT 的 `asr.js` (Mac adapter) - 已经是 VAD + 阿里云集成

**风险**:
- OMT 组件用 BSP 抽象 (`bsp/m5stack_tab5`), 移植需改引脚
- Opus 在 ESP32-P4 上 CPU 占用可能 20-35%, 需压测

#### **Phase 2 (W4): WakeNet 唤醒** (1-2 周)

**目标**: 用户喊"嗨乐鑫"自动激活录音

**任务**:
1. 加 `espressif/esp-sr` 依赖
2. 选择模型: WakeNet9s (无 PSRAM 也能跑) 或 WakeNet9 (需 PSRAM)
3. 集成 AFE (AEC + NS + VAD 一体) - 替换我们当前的硬件 AEC
4. 唤醒后触发录音 (类似 OMT 的 `s_app_recording`)
5. 可选: 自定义唤醒词训练 (需 ESP-SR 训练工具)

**风险**:
- ESP-SR ~2MB flash, 需评估 binary 大小
- WakeNet 误触发率 (<1次/12h) 测试

#### **Phase 3 (W5): TTS + 完整对话** (2-3 周)

**目标**: AI 回复文字 → 终端语音播报

**任务**:
1. PC Adapter 端: tts.js 调火山豆包双向流式 TTS
2. Opus 24kHz mono 60ms 帧下行
3. ESP32 端: Opus 解码 → ES8311 DAC 播放 (复用 p4c5_audio)
4. UI 状态: 听 / 说 / 思考 三种状态切换
5. 完整端到端测试

**风险**:
- 火山引擎 API key 申请
- 中文音色选择
- 流式 TTS 缓冲策略

### 4.2 资源评估

#### Flash 占用
| 项 | 大小 |
|---|---|
| 当前二进制 | ~1.5MB |
| + ESP-SR WakeNet9 + MultiNet | +2-3MB |
| + Opus 编码器 | +200KB |
| **总计** | **~4-5MB** (P4 16MB flash 充裕) |

#### PSRAM 占用
| 项 | 大小 |
|---|---|
| 当前 PSRAM | <500KB (LCD framebuffer) |
| + Opus 编码缓冲 | +128KB |
| + AFE 缓冲 | +200KB |
| **总计** | **~1MB** (P4 32MB PSRAM 充裕) |

#### CPU 占用
| 模块 | CPU |
|---|---|
| 当前空闲 | ~10% |
| + WakeNet (待唤醒) | +5% |
| + Opus 编码 | +20-35% (录音时) |
| + Opus 解码 + 播放 | +10-15% (播放时) |
| **峰值** | **~60% (双核, 可接受)** |

### 4.3 成本估算 (假设 1000 次对话/月)

#### 用户使用模式
- 每次对话: 用户 15s + AI 30s = 45s
- 1000 次对话: 45000s = 750 分钟 ≈ 12.5 小时

#### 方案 A: 阿里云 Paraformer + 火山豆包 TTS (企业首选)
| 项 | 价格 | 月费用 |
|---|---|---|
| ASR 阿里云流式 | $1.40/小时 | ¥90 (12.5h × ¥7.2) |
| TTS 火山豆包 | ¥150/音色/年 + 字数 | ¥150 (≈1万字) |
| LLM DeepSeek V3 | ¥2/百万token | ¥50 |
| **合计** | | **¥290/月** |

#### 方案 B: 讯飞 ASR + Edge TTS (开发原型)
| 项 | 价格 | 月费用 |
|---|---|---|
| ASR 讯飞流式 | ¥0.006/分 | ¥75 |
| TTS Edge | **免费** (商用禁) | **¥0** ⚠️ |
| LLM DeepSeek V3 | ¥2/百万token | ¥50 |
| **合计** | | **¥125/月** ⚠️ Edge 不可商用 |

#### 方案 C: 端侧 + 云端混合 (推荐 MVP)
| 项 | 价格 | 月费用 |
|---|---|---|
| ESP-SR 唤醒 (本地) | 免费 | ¥0 |
| Opus 编码 (本地) | 免费 | ¥0 |
| ASR 阿里云 Paraformer (云端) | $1.40/小时 | ¥90 |
| TTS 火山豆包 (云端) | 按字数 | ¥150 |
| LLM DeepSeek V3 | ¥2/百万token | ¥50 |
| **合计** | | **¥290/月** |

### 4.4 差异化策略

**我们应做加法** ⭐:
- ✅ **4 麦波束成形 + 增强降噪** - 利用 ES7210 4 麦独家优势
- ✅ **本地 PC Adapter** - 延迟 <500ms, 隐私好, 离线可用
- ✅ **唤醒 + ASR + TTS 全链路** - 与 xiaozhi 同级体验
- ✅ **DSH 协议层 14+4 帧** - 比 xiaozhi 9 帧更细分

**我们应做减法** ❌:
- ❌ 不支持 171 板型 - 只做 ESP32-P4+C5
- ❌ 不做多 ASR/TTS 服务适配 - 选 1-2 个最合适的
- ❌ 不做 MCP 物联网 - 当前用 DSH 14+4 帧已够
- ❌ 不做声纹 / 视觉 / 多模态 - 聚焦语音助手

---

## 五、立即可执行的下一步

### 选项 A (推荐): 按 Phase 1 → Phase 2 → Phase 3 顺序推进

**当前状态**: W1 ✅ W2 ✅ W3 plan 已有, 即将进入

**W3 Phase 1** (1-2 周):
1. 集成 libopus (ESP32 端编码)
2. 写 Opus → WS Binary 上行通道
3. PC Adapter 端: 写 asr.js 调阿里云 (复用 OMT)
4. 验证: 录音 → ASR → 文字 → DSH 协议层 → mock server

**W4 Phase 2** (1-2 周):
1. 集成 ESP-SR WakeNet9
2. AFE 替换硬件 AEC
3. 验证: 喊"嗨乐鑫" → 录音 → ASR → 文字

**W5 Phase 3** (2-3 周):
1. 集成 TTS (火山豆包)
2. Opus 解码 + 播放
3. 完整端到端测试

### 选项 B: 跳过 Phase 1, 直接做 WakeNet (更激进)

**理由**: WakeNet 体验提升最大 (无需按按钮)
**风险**: 没有 Opus 编码, ASR 仍需补

### 选项 C: 只做本地 ASR (不依赖网络)

**理由**: 离线可用, 隐私最好
**代价**: 中文准确率有限, 模型大 (~3MB), 资源占用高

---

## 六、我的最终建议

### 🎯 强烈推荐: 选项 A 顺序推进

**理由**:
1. **Phase 1 风险最低** - OPUS + WS Binary 是行业标准, 工具链成熟
2. **复用 OMT 经验** - 不重复踩坑
3. **每阶段可独立验证** - 即使后续取消, Phase 1 也有价值
4. **总成本可控** - ¥290/月 合理

### 📅 时间线 (建议)

| 周 | 阶段 | 任务 | 状态 |
|---|---|---|---|
| W1 | WiFi | SDIO transport + DSH | ✅ |
| W2 | 协议层 | 14+4 帧验证 | ✅ |
| **W3** | **音频编码** | **Opus + WS Binary + ASR** | **🚧 即将开始** |
| W4 | 唤醒 | WakeNet9 + AFE | 待 |
| W5 | TTS | 火山豆包 + Opus 解码 | 待 |
| W6+ | 优化 | 4 麦波束成形 + UI 优化 | 待 |

### ❓ 需要您决策

1. **方案 A/B/C 选哪个?**
   - 我建议 A (推荐)
2. **W3 Phase 1 立即开始?**
   - 预计 1-2 周完成, 我可以立即开始
3. **是否接受 ¥290/月的云端 ASR+TTS 成本?**
   - 如果不接受, 只能用方案 C (本地 ASR) 或暂不实现

---

## 七、引用源汇总

### xiaozhi 调研 (subagent f3c87456)
- https://github.com/78/xiaozhi-esp32
- https://github.com/xinnan-tech/xiaozhi-esp32-server
- https://raw.githubusercontent.com/78/xiaozhi-esp32/main/docs/websocket.md
- https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/wake_word_engine/README.html

### ASR/TTS 对比 (subagent 970dad61)
- 阿里云: https://nls-portal.console.aliyun.com/
- 火山引擎: https://www.volcengine.com/docs/6561/1329505
- 讯飞: https://www.xfyun.cn/services/voicedictation
- Edge TTS: https://github.com/rany2/edge-tts
- ESP-SR: https://github.com/espressif/esp-sr

### 信息流协议 (subagent c39340ec)
- MCP: https://modelcontextprotocol.io/
- A2A: https://google.github.io/A2A/
- OpenAI Realtime: https://platform.openai.com/docs/guides/realtime
- Alexa AVS: https://developer.amazon.com/en-US/alexa/alexa-skills-kit
- Google Assistant: https://developers.google.com/assistant/sdk

### OMT 本地资料
- /Volumes/ZT-1T/项目开发/OMT/hardware/tab5-adapter/asr.js
- /Volumes/ZT-1T/项目开发/OMT/hardware/tab5-agent-terminal/docs/audio/阿里云ASR接入方案.md
- /Volumes/ZT-1T/项目开发/OMT/hardware/tab5-agent-terminal/main/app_main.cpp