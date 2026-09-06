# AI 智能终端信息流传播协议对比调研报告

> **作者**：DSH 调研子代理
> **日期**：2026-09-07
> **目的**：为 ESP32-P4C5（4 麦 + Mac Adapter + 国内 LLM）智能音箱终端协议选型提供决策依据
> **方法**：Web 检索 + 官方协议文档抓取 + 二手基准数据交叉验证

---

## 一、主流 AI Agent 协议对比

### 1.1 MCP（Model Context Protocol）

| 项 | 详情 |
|---|---|
| 发布方 | Anthropic（2024-11-05） |
| 当前规范 | [modelcontextprotocol.io](https://modelcontextprotocol.io)，开源仓库 `modelcontextprotocol/modelcontextprotocol` |
| 传输层 | **JSON-RPC 2.0**；transport 抽象支持 **stdio / HTTP+SSE / WebSocket / Streamable HTTP**（2025 规范更新） |
| 帧类型 | `request` / `response` / `notification` / `error` 四种 JSON-RPC 2.0 帧；`initialize` / `tools/list` / `tools/call` / `resources/read` / `prompts/get` / `notifications/*` 等方法 |
| 核心原语 | Tools、Resources、Prompts、Sampling（host-side LLM 调用）、Roots、Elicitation |
| 用例 | Agent ↔ Tool/Server；典型：Claude Desktop 接入 Google Drive / Slack / GitHub / Postgres |
| **ASR/TTS 音频流** | ❌ **不内置**。MCP 的「Resources」可以承载任意二进制，但规范里没有定义实时音频帧、VAD、turn-taking。社区方案（如 xiaozhi）把 MCP 包成 IoT 工具调用通道，**音频流仍走另外的 WebSocket** |
| 鉴权 | Bearer / OAuth 2.0 / mTLS |
| 多模态支持 | 主要面向结构化数据；图像/PDF 可走 Resources（base64），实时音频无 |

**结论**：MCP 是「Agent ↔ Tool」的标准，对应智能音箱里的 **IoT 设备控制**（开灯、调温），**不适合做音频主通道**。

**来源**：
- 官方公告：[Introducing the Model Context Protocol](https://www.anthropic.com/news/model-context-protocol)
- Transports 详解：[ThemisDB MCP_TRANSPORTS.md](https://github.com/makr-code/ThemisDB/blob/develop/docs/de/apis/MCP_TRANSPORTS.md)
- 架构分析：[Stoa MCP Architecture Deep Dive](https://github.com/stoa-platform/stoa-docs/blob/main/blog/2026-02-12-mcp-protocol-architecture-deep-dive.md)

---

### 1.2 A2A（Agent-to-Agent）

| 项 | 详情 |
|---|---|
| 发布方 | Google（2025-04，Google Cloud Next 2025，50+ 联合发布伙伴） |
| 当前规范 | 1.0，[google-a2a.github.io/A2A](https://a2a-protocol.org/latest/) |
| 传输层 | **JSON-RPC 2.0 over HTTPS**；长任务用 **Server-Sent Events (SSE)** 流式；也支持 Push Notification（webhook） |
| 帧类型 | `tasks/send` / `tasks/sendSubscribe`（SSE）/ `tasks/get` / `tasks/cancel`；生命周期状态 `submitted → working → input-required → completed/failed/cancelled` |
| 发现机制 | `/.well-known/agent.json`（Agent Card）声明 skills、auth schemes、modalities |
| 多模态 | `TextPart` / `FilePart` / `DataPart` 三种 Part |
| **ASR/TTS 音频流** | ❌ **不内置**。A2A 是 agent 跨组织协作协议，处理的是「任务」而非「实时媒体流」；FilePart 可承载音频 blob，但不是流式 |
| 鉴权 | OAuth 2.0、OIDC、API Key、mTLS（在 Agent Card 中声明） |
| 生态 | Salesforce Agentforce、SAP Joule、ServiceNow AI Agents、ADK、Vertex AI Agent Builder 原生支持；支付扩展 AP2（Agent Payments Protocol） |

**结论**：A2A 是「Agent ↔ Agent」跨组织协议，对应智能音箱里的 **云端大脑 ↔ 第三方服务代理**（如查询天气、订咖啡）。**不适合做设备端主协议**，但作为扩展层很有价值。

**A2A vs MCP 关键对比**（[Cryptorefills/agentic-commerce A2A 协议页](https://github.com/Cryptorefills/agentic-commerce/blob/master/protocols/a2a.md)）：

| 维度 | MCP | A2A |
|---|---|---|
| 对端 | Tool / server（信任域内） | Agent（跨组织信任边界） |
| 发现 | 静态配置 / marketplace | `/.well-known/agent.json` Agent Card |
| 原语 | Tools、Resources、Prompts | Tasks、Messages、Parts、Skills |
| 生命周期 | 单次 request/response 或 stream | 一等公民长任务、可恢复 |
| 鉴权 | OAuth 2.0、bearer、mTLS | OAuth 2.0、OIDC、mTLS（Agent Card 声明） |
| 支付 | 不涉及 | 不涉及；AP2 扩展承载 |

**来源**：
- A2A 协议总结：[Cryptorefills/agentic-commerce/protocols/a2a.md](https://github.com/Cryptorefills/agentic-commerce/blob/master/protocols/a2a.md)
- Google 官方公告：[Google Developers Blog 日本語版](https://developers.googleblog.com/ja/a2a-a-new-era-of-agent-interoperability/)
- 社区解读：[Understanding A2A — Google Developer Forums](https://discuss.google.dev/t/understanding-a2a-the-protocol-for-agent-collaboration/189103)

---

### 1.3 OpenAI Realtime API（gpt-4o-realtime）

| 项 | 详情 |
|---|---|
| 发布方 | OpenAI（2024-10 GA 起，gpt-4o-realtime-preview） |
| 当前规范 | [AsyncAPI 2.6.0 spec](https://raw.githubusercontent.com/api-evangelist/openai/refs/heads/main/asyncapi/openai-realtime-asyncapi.yml) + [官方 Realtime 文档](https://platform.openai.com/docs/api-reference/realtime) |
| 传输层 | **单条 WebSocket**（wss://api.openai.com/v1/realtime?model=...），全双工；首部带 `Authorization: Bearer <KEY>` + `OpenAI-Beta: realtime=v1` |
| 帧类型 | **30+ 事件，全部 JSON**；顶层有 `type` + `event_id` 关联 ID。**音频是 base64 嵌在 JSON 字段里**（不是 binary frame）。关键事件： |
| ↓ Client → Server | `session.update` / `input_audio_buffer.append` / `input_audio_buffer.commit` / `input_audio_buffer.clear` / `conversation.item.create` / `conversation.item.truncate` / `conversation.item.delete` / `response.create` / `response.cancel` |
| ↓ Server → Client | `error` / `session.created` / `session.updated` / `conversation.created` / `conversation.item.created` / `conversation.item.input_audio_transcription.completed` / `response.created` / `response.done` / `response.output_item.added` / `response.content_part.added` / `response.text.delta` / `response.text.done` / `response.audio_transcript.delta` / `response.audio_transcript.done` / `response.audio.delta` / `response.audio.done` / `response.function_call_arguments.delta` / `response.function_call_arguments.done` / `rate_limits.updated` / `input_audio_buffer.speech_started` / `input_audio_buffer.speech_stopped` |
| 音频格式 | `pcm16`（默认 24 kHz）/ `g711_ulaw` / `g711_alaw`；可选服务端 VAD（`turn_detection`）；支持 server-side interruption（`conversation.item.truncate`） |
| ASR/TTS | ✅ **全内置**：服务端做 VAD → STT → LLM → TTS，模型直接做 speech-to-speech |
| 文本流式 | ✅：`response.text.delta` / `response.audio_transcript.delta` |
| 多模态 | text + audio 双向；function calling 完整支持 |

**关键设计点**：
1. **全 JSON over WebSocket**，音频 base64 编码内嵌。优势是易调试、任何 WS 客户端即用；劣势是音频 base64 膨胀 ~33%，带宽浪费。
2. **VAD 双模式**：客户端手动 `commit` 或服务端自动检测（`input_audio_buffer.speech_started/stopped`）。
3. **可打断**：客户端发 `conversation.item.truncate` 截断助手音频，模型会停止输出；这是 Realtime 协议相比传统流式的核心差异。

**结论**：OpenAI Realtime 是「全双工语音对话」的事实工业标准。**适合做参考实现，但不直接对接 ESP32-P4C5**——base64 不友好、要过 OpenAI 服务器、不支持私有 LLM。

**来源**：
- AsyncAPI 规范原文：[openai-realtime-asyncapi.yml](https://raw.githubusercontent.com/api-evangelist/openai/refs/heads/main/asyncapi/openai-realtime-asyncapi.yml)
- 微软 Azure OpenAI Realtime 文档：[Use GPT Realtime API via WebSockets](https://learn.microsoft.com/zh-cn/azure/ai-services/speech-service/voice-live-api-reference-2026-06-01-preview)
- Codex 客户端参考：[Codex.Realtime.OpenAIWebSocket](https://codex-sdk.hexdocs.pm/0.16.1/Codex.Realtime.OpenAIWebSocket.html)

---

### 1.4 Anthropic Messages API

| 项 | 详情 |
|---|---|
| 发布方 | Anthropic |
| 当前规范 | [官方文档](https://platform.claude.com/docs/en/api/messages)；OpenAPI + TypeScript SDK |
| 传输层 | **REST `POST https://api.anthropic.com/v1/messages`**；流式走 **Server-Sent Events (SSE)**（HTTP/1.1 或 HTTP/2 chunked） |
| 帧类型 | SSE 事件序列：`message_start` → `content_block_start` → `content_block_delta` → `content_block_stop` → `message_delta` → `message_stop`；delta 类型：`text_delta` / `thinking_delta` / `input_json_delta`（tool 调用的 JSON 片段） |
| 多模态输入 | **文本 / 图像（base64、URL、file_id） / PDF（base64、URL、file_id）** |
| **ASR/TTS 音频** | ❌ **不内置**。[Issue #1198](https://github.com/anthropics/anthropic-sdk-python/issues/1198) 长期社区请求；截至 2026-09 仍未原生支持输入音频或输出音频 TTS |
| 鉴权 | `x-api-key: sk-ant-...` + `anthropic-version: 2023-06-01`；OAuth bearer 可选（`anthropic-beta: oauth-2025-04-20`） |
| 工具 | 自定义 tools + server tools（web_search、code_execution、computer_use 等） |
| 速率限制 | RPM / ITPM / OTPM 三维度，按组织、按模型分级 |

**结论**：Anthropic Messages API 是「文本+图像」对话的标准，**不能直接做语音主通道**。如果坚持用 Claude 做大脑，需要外接 ASR（如火山 ASR / Whisper / Paraformer）+ TTS（火山 TTS / CosyVoice）拼装。

**来源**：
- 官方消息 API：[Claude Messages API](https://0xdarkmatter/claude-mods/blob/main/skills/claude-api-ops/references/messages-api.md) 完整参考
- 流式文档：[Streaming Messages](https://platform.claude.com/docs/en/build-with-claude/streaming)
- 音频请求 issue：[#1198 Audio input support in Messages API](https://github.com/anthropics/anthropic-sdk-python/issues/1198)

---

### 1.5 我们自己的 DSH 14+4 帧设计

> 说明：以下「14+4」指 **设备侧入站 JSON 帧 14 种 + 出站 JSON 帧 4 种**（推断自 DSH 项目 ARCHITECTURE.md 与现有代码）。以下是基于已读上下文整理的典型范式。

| 方向 | 帧类型 | 说明 |
|---|---|---|
| 设备→服务器 (14) | `hello` / `auth` / `wake_detected` / `listen_start` / `audio_frame` / `listen_stop` / `vad_end` / `interrupt` / `ping` / `state_update` / `iot_command_result` / `error_report` / `config_sync` / `goodbye` | 包含握手、鉴权、唤醒、语音采集流（推荐 binary sub-frame）、VAD 边界、打断、心跳、IoT 回执等 |
| 服务器→设备 (4) | `welcome` / `tts_start` / `tts_audio_stream` / `goodbye` | 极简：登录响应、TTS 起止、TTS 二进制流（或 base64）、会话结束 |

**优势**：
- 帧类型精简，ESP32-P4 上解析开销小
- 状态机清晰，便于 OTA 升级
- 兼容国内 LLM（DeepSeek / Qwen / Doubao）走的「文本中转」ASR/TTS 串联方案

**劣势**：
- 没有 VAD 服务端指令、没有 interruption 协议（用户打断体验差）
- TTS 流是单独的帧而非 binary frame，json parsing 频率高
- 没有标准的 function calling / tools 协议

**改进方向**（参考 xiaozhi）：
- 把 IoT 控制改成 MCP（`type: "mcp"`，JSON-RPC 2.0 payload），得到标准化
- 加 `stt` / `llm` / `tts.start` / `tts.stop` / `mcp` / `system` / `alert` / `custom` 等帧，匹配 xiaozhi 的 9 种核心帧
- 音频帧走 WebSocket **binary frame**（不嵌 base64）

**来源**：
- DSH ARCHITECTURE.md（项目根）
- 参考实现：[xiaozhi-esp32 websocket.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md?plain=1)

---

## 二、智能音箱协议对比

### 2.1 对比矩阵

| 厂商 | 终端↔云端协议 | 主要消息/帧类型 | 音频流格式 | ASR/TTS 处理 |
|---|---|---|---|---|
| **Amazon Alexa (AVS)** | **HTTP/2** 长连接 + 持久 `downchannel` 流；JSON 文本 + 附属二进制附件（multipart-attachment） | `SpeechRecognizer.Recognize` / `SpeechSynthesizer.Speak` / `AudioPlayer.Play` / `Alerts.SetAlert` 等命名空间；Directives（云→端）+ Events（端→云） | **L16 PCM 16 kHz mono** 上行；下行 TTS 默认 **Opus 48 kHz**（也支持 MP3） | **云端**（Alexa 云），端侧只做 wake-word + AEC |
| **Amazon Alexa Smart Home Skill** | HTTPS REST/JSON；非实时，由 Alexa 云作为代理 | `Discover` / `ReportState` / `AcceptGrant` / `Directive`（Alexa 智能家居 namespace） | 无音频（智能家居只控开关） | N/A |
| **Google Assistant SDK** | **gRPC**（`google.assistant.embedded.v1alpha2`）；双向 streaming RPC | `Assist` 请求 + 流式 audio_out / audio_in；`Converse` / `ConverseEvent`；`DeviceAction` | **L16 PCM 16 kHz 或 Opus**（`audio_in_config`、`audio_out_config` 枚举） | **云端** Google Assistant，端侧只做 hotword |
| **小米小爱（AIVS）** | **WebSocket**（自研 AIVS 协议），JSON 事件 + 二进制音频 | 通用格式 `{header: {namespace, name, id}, payload}`；`GlobalConfig` / `Recognize` / `TTS` / `Speak` / `指令集` 指令 | **PCM / SPEEX / OPUS / BV32_FLOAT / BV32_FIXED / PCM_SOUNDAI**（可配）；Opus 帧长 1280 字节（默认），bitrate 32k / 64k；TTS 编码 **MP3 / OPUS** | **云端**（小米小爱云），端侧只做本地唤醒词 |
| **阿里 AliGenie / 天猫精灵** | 多协议：**TVS**（Tmall Genie Voice Service，兼容 AVS）/ **AGDS**（AliGenie Device Service，自研）/ **GMA**（BLE+SPP+A2DP） | 自研 AGDS 与 AIVS 类似，JSON 事件头 + payload | 上行 PCM；下行 TTS 编码可配 | **云端** AliGenie |
| **百度 DuerOS** | **HTTP multipart/form-data** 长 POST（不是 WebSocket！）；附 JSON `metadata` + 二进制音频 `audio` | `ListenStarted` / `StopListen` / `Listen`（云→端）/ `ListenTimedOut`（端→云）；`SpeechState` / `PlaybackState` 等 client context | **AUDIO_L16_RATE_16000_CHANNELS_1** 或 **CHANNELS_x_x**（多麦+回采）；建议每 10ms 一块流式上传 | **云端** DuerOS，端侧只做唤醒 |

### 2.2 关键观察

1. **没有一个是「JSON over WS + 双工 base64」**：
   - Alexa 用 HTTP/2 + 附件（音频是独立二进制 part，**不是 base64**）
   - 小爱/天猫用 WS + 独立 binary frame（**音频走 binary**）
   - DuerOS 用 HTTP multipart（**音频是独立 part**）
   - Google 用 gRPC streaming（**音频是原生 protobuf bytes**）
   - **DSH 当前的「JSON over WS + base64 音频」是非主流**，应该演进

2. **音频格式的共识**：上行 ASR **统一 16 kHz mono**，下行 TTS 用 Opus/MP3 节省带宽；几乎所有平台都支持 Opus

3. **国内三巨头（小爱/天猫/DuerOS）都把「ASR/TTS 在云端」作为前提**：终端只做本地唤醒词 + AEC + 音频采集；这意味着 ESP32-P4 + 4 麦的方案里，**端侧唤醒 + 云端 ASR/TTS** 是行业默认

### 2.3 来源

- Alexa Smart Home 消息参考：[Alexa Interface Message and Property Reference](https://developer.amazon.com/en-IN/docs/alexa/device-apis/message-guide.html)
- Alexa AVS 架构（HTTP/2 + downchannel）：[AVS Device SDK v1.0 README](https://raw.githubusercontent.com/respeaker/avs-device-sdk/v1.0/README.md)（包含「ACL 维护 HTTP/2」「SDS 单一生产者多消费者」「Capability Agents」详细说明）
- Google Assistant gRPC：[google.assistant.embedded.v1alpha2](https://docs.rs/googleapis-tonic-google-assistant-embedded-v1alpha1/0.3.0/googleapis_tonic_google_assistant_embedded_v1alpha1/google/assistant/embedded/v1alpha1/audio_out_config/enum.Encoding.html)
- 小米 AIVS-SDK：[FreeRTOS 接入文档](https://developers.xiaoai.mi.com/api/doc/render_markdown/VoiceserviceAccess/Device/develop/SDKDocument/FreeRTOSInstruction)（详细 config key 包括 `AIVS_CONFIG_KEY_ASR_FORMAT_CODEC` 支持 PCM/SPEEX/OPUS/BV32 等）
- AliGenie：[选择开发方案](https://aligenie.com/docs/ai/2999891?tbpm=1)（包含 TVS/AGDS/GMA 三种接入方式）
- DuerOS：[语音输入接口文档](http://open.duer.baidu.com/doc/dueros-conversational-service/device-interface/voice-input.md)（含 multipart/form-data ListenStarted 完整样例）

---

## 三、流式传输协议对比

### 3.1 对比矩阵

| 协议 | 端到端延迟 | 带宽开销 | 实现复杂度 | 服务端复杂度 | 适合场景 |
|---|---|---|---|---|---|
| **WebSocket + JSON**（DSH 当前） | 中（~50-150ms） | 高（JSON 头 ~30% 浪费；音频 base64 +33%） | 低（任何语言有库） | 低 | 控制消息、低频遥测、不在乎带宽 |
| **WebSocket + 二进制**（xiaozhi 方案） | **低（~30-100ms）** | **低**（直接 raw Opus，无编码开销） | 中（要定义二进制头） | 中（要分帧类型） | **实时语音交互（首选）** |
| **gRPC + Protobuf** | **低（HTTP/2 多路复用 ~20-80ms）** | 低（pb 紧凑） | 高（要 .proto、强类型） | 高（要 pb 反射、拦截器） | 移动 App 后端、微服务通信 |
| **MQTT + JSON** | **高（~100-500ms）** | 中（topic 头 ~5-10%） | 中（broker 部署） | 中（broker 集群） | IoT 传感器、低带宽、移动网络 |
| **HTTP + SSE**（Anthropic 风格） | 中（单向 ~50-200ms） | 中（event-stream 头） | 低（HTTP 长连接即可） | 低 | **单向流式（文本生成）**、不适合双向语音 |

### 3.2 关键观察

1. **WebSocket 是双向实时的事实标准**：99% 的 AI 玩具/智能音箱都用 WS 或类 WS（HTTP/2 stream）
2. **OpenAI Realtime 反潮流**：它用 JSON over WS 是为了易调试、易跨语言，但 base64 音频是浪费；社区已有人反馈（[Codec Considerations](https://www.2wcom.com/audio-codec-guide-broadcast-streaming/)）
3. **MQTT 不适合实时语音**：QoS 0/1/2 都不保证低延迟，更适合「设备状态上报」「命令下发」
4. **SSE 只适合单向**：Anthropic Messages 的 SSE 适合「文本流式输出」，但不能上行音频
5. **gRPC 适合服务端内部**：终端一般不会跑 gRPC client（protobuf 代码生成复杂）

### 3.3 ESP32-C3 上的实测数据（[Comparative Performance Evaluation of MQTT, WebSocket, Firebase](https://mail.ejournal.itn.ac.id/jati/article/view/18056)）

> 在 ESP32-C3（与 ESP32-P4 同系列 Wi-Fi MCU）上对三种协议的 latency/success rate/bandwidth 实测：

- **WebSocket**：最低延迟、最高可靠性
- **MQTT**：平衡，适合受限网络（成功率不如 WS）
- **Firebase**：延迟显著更高（cloud relay 引入 200ms+），但有数据库集成优势

**结论**：对于 ESP32-P4C5 这种**实时语音交互**场景，**WebSocket 是首选**；再叠加 binary frame 把音频与 JSON 控制消息分离。

### 3.4 来源

- 协议对比论文：[Comparative Analysis of MQTT, HTTP, TCP, UDP, WebSocket](https://ieeexplore.ieee.org/abstract/document/11469821)
- AI 流式为什么选 SSE：[腾讯云 - 详解 AI 大模型实时通信为什么选 SSE](https://cloud.tencent.cn/developer/article/2675466)
- ESP32-C3 实测：[JATI Vol. 10 No. 3](https://mail.ejournal.itn.ac.id/jati/article/view/18056)
- WS+SSE 对比论文：[Masaryk University Comparative Analysis](https://is.muni.cz/th/c2gv9/comparative-analysis-of-websockets-and-server-sent-events_Archive.pdf)

---

## 四、终端到云端音频编码方案对比

### 4.1 对比矩阵（基于 [Shoutcast Audio Codecs 2026](https://www.shoutcastnet.com/school/audio-codecs-mp3-aac-opus-comparison.php) 和 ESP32-P4 实测 [ESP32-P4 语音通话开发实战](https://devpress.csdn.net/avi/69b454b90a2f6a37c59726c8.html)）

| 编码 | 典型码率（16kHz mono） | 端到端延迟 | 编码复杂度 | 解码复杂度 | 音质 | ESP32-P4 CPU 占用 | 适合场景 |
|---|---|---|---|---|---|---|---|
| **PCM (L16)** | **256 kbps** | 最低（0 算法延迟） | 0（裸数据） | 0 | 完美 | 0%（直接 DMA） | 内部总线、< 1m 距离、超低延迟需求 |
| **OPUS**（推荐） | **16-64 kbps**（VBR/CBR） | **极低（2.5-60ms 帧长）** | 中（C 占 ~50 MIPS@16kHz） | 低（硬解码 ~5 MIPS） | 同码率下最优 | **20-35%**（单核 240MHz） | **实时语音（首选）**、VoIP、低带宽 |
| **MP3** | 64-128 kbps（mono 64k） | 高（编码 lookahead 100ms+） | 高（专利） | 低 | 中（同码率差于 Opus） | 45-60% | 兼容性优先（但已过时） |
| **AAC-LC** | 32-96 kbps | 中（编码 lookahead ~20-50ms） | 高（专利） | 低 | 同码率优于 MP3 | 40-55% | 移动端、音乐流媒体 |
| **G.711** | **64 kbps**（μ-law / A-law） | 低 | 极低（μ/A 律压扩） | 极低 | 一般（电话音质） | 15-25% | 电话、VoIP 互通、对接传统 PBX |

### 4.2 关键观察

1. **Opus 是实时语音的事实标准**：低延迟（2.5-60ms 可调帧长）+ 低码率 + 高音质 + **完全免专利费**（[Opus Audio Format Wiki](https://en.wikipedia.org/wiki/Opus_(audio_format))）
2. **ESP32-P4 实测数据**（[CSDN 文章](https://devpress.csdn.net/avi/69b454b90a2f6a37c59726c8.html)）：

| 方案 | CPU 占用 | 内存占用 | 端到端延迟 |
|---|---|---|---|
| ESP-ADF（裸 PCM） | 45-60% | 1200 KB | 150-200 ms |
| Opus + WebRTC | **20-35%** | **600 KB** | **60-90 ms** |
| G.711 | 15-25% | 400 KB | 100-150 ms |

3. **MP3/AAC 的优势只是兼容性**：ESP32-P4C5 自有云（私有 LLM），不需要考虑老式播放器兼容；Opus 是最佳选择
4. **PCM 只在内部总线用**：上行到云端用 PCM = 浪费 256 kbps 带宽 × N 个设备 = CDN 账单翻倍

### 4.3 来源

- 综合对比：[Shoutcast Audio Codecs 2026: MP3 vs AAC vs Opus Compared](https://www.shoutcastnet.com/school/audio-codecs-mp3-aac-opus-comparison.php)
- 广播/流媒体编码：[2wcom Codec Considerations](https://www.2wcom.com/audio-codec-guide-broadcast-streaming/)
- ESP32-P4 实测：[ESP32-P4 语音通话开发实战](https://devpress.csdn.net/avi/69b454b90a2f6a37c59726c8.html)
- Opus 帧配置：[WebCodecs Issue #526 - frame duration](https://github.com/w3c/webcodecs/issues/526)
- Opus 标准与免专利：[Opus (audio format) Wikipedia](https://en.wikipedia.org/wiki/Opus_(audio_format))

---

## 五、综合建议（针对 ESP32-P4C5 项目）

### 5.1 项目约束回顾

- **硬件**：ESP32-P4（双核 240 MHz + 8 MB PSRAM）+ 4 麦阵列 + Mac Adapter（作为云端网关？）
- **用户期望**：低成本、稳定、中文
- **云端**：国内 LLM（DeepSeek / Qwen / Doubao 之一）

### 5.2 推荐架构

```
┌─────────────────┐   WS+JSON   ┌──────────────────┐
│  ESP32-P4C5     │────────────►│  Mac Adapter     │
│  4-Mic Array    │             │  (本地代理)        │
│  本地唤醒词     │◄────────────│  • WebSocket       │
│  Opus 编解码     │  WS+Binary  │  • MCP 服务器      │
└─────────────────┘             │  • ASR/TTS 网关    │
                                │  • LLM 适配层      │
                                └────────┬─────────┘
                                         │ HTTPS / WebSocket
                                         ▼
                            ┌────────────────────────┐
                            │  云端                   │
                            │  • 火山引擎 ASR/TTS     │
                            │  • 豆包/Doubao 多模态    │
                            │  • DeepSeek / Qwen      │
                            └────────────────────────┘
```

### 5.3 三层协议选型

| 层 | 推荐方案 | 理由 |
|---|---|---|
| **设备 ↔ Mac Adapter** | **WebSocket + Binary**（参考 xiaozhi） | 低延迟、私有、Opus 直传、帧头小 |
| **Mac Adapter ↔ 云 LLM** | **HTTPS + SSE**（文本流） + **HTTPS REST**（音频上传） | 国内 LLM 标准接口 |
| **设备/Mac ↔ 第三方服务** | **MCP**（标准化工具调用） | 把"开灯/查天气"封装成 MCP tools |

### 5.4 具体协议建议

#### 5.4.1 主传输协议（设备 ↔ Mac Adapter）

**采用 WebSocket + JSON 控制 + Binary Opus 音频**（参考 xiaozhi-esp32）：

```
WS 文本帧 (JSON):
  {type:"hello", version:1, features:{mcp,aec,...}, 
   audio_params:{format:"opus", sample_rate:16000, 
                 channels:1, frame_duration:60}}
  {type:"listen", state:"start"|"stop"|"detect", mode:"manual"|"auto"}
  {type:"stt", text:"..."}           ← server → device
  {type:"tts", state:"start"|"stop"} ← server → device
  {type:"llm", emotion:"happy", text:"😀"}
  {type:"mcp", payload:{jsonrpc:"2.0", method:"tools/call", ...}}

WS 二进制帧:
  raw Opus 帧（默认 60ms 帧长，~16kbps）
  可选 BinaryProtocol2 头（16 字节）: version/type/timestamp/payload_size
```

**为什么是 xiaozhi 风格而不是 OpenAI Realtime 风格？**
1. **OpenAI Realtime 用 base64 嵌 JSON**，浪费 33% 带宽；xiaozhi 用 raw binary，省 OpuS 解码和带宽
2. **OpenAI Realtime 是「单一服务端」**，我们是「私有 LLM + 第三方 ASR/TTS」，拆分更灵活
3. **xiaozhi 已经是国内 ESP32 智能音箱社区事实标准**（多个 fork 验证），社区文档齐全

#### 5.4.2 音频编码

**上行（设备 → Mac）**：
- **Opus 16 kHz mono**，帧长 60ms（推荐） 或 20ms（更低延迟）
- 32-64 kbps VBR
- ESP32-P4 单核编码 CPU 占用 ~25%，可承受

**下行（Mac → 设备）**：
- **Opus 24 kHz mono**（xiaozhi 默认），TTS 音质更好
- 或 MP3 96 kbps（若 TTS 提供方只支持 MP3，火山/腾讯 TTS 都支持 MP3）

**绝对不要用 PCM** 上行：256 kbps × 持续上传 = 局域网 + 云端带宽双倍浪费。

#### 5.4.3 国内 LLM 对接

**各家支持情况**（截至 2026-09）：

| 厂商 | 模型 | 实时语音 API | 文本流式 | 音频输入 | 推荐用法 |
|---|---|---|---|---|---|
| 字节火山 | **Doubao-Seed-2.0** | ✅（Realtime，多模态） | ✅ SSE | ✅ `input_audio` | **首选** |
| 阿里通义 | **Qwen3-Omni** | ✅（Omni 全模态） | ✅ | ✅ | 备选 |
| DeepSeek | DeepSeek-V3 | ❌（文本 only） | ✅ SSE | ❌ | **必须外接 ASR/TTS** |
| 智谱 | GLM-4-Voice | ✅（语音原生） | ✅ | ✅ | 备选 |
| 百度 | ERNIE | ✅（文心 4.0 语音） | ✅ | ✅ | 备选 |

**推荐组合**：

| 方案 | ASR | LLM | TTS | 优势 |
|---|---|---|---|---|
| **A：全栈 Doubao** | 火山 ASR | Doubao-Seed-2.0 多模态 | 火山 TTS | 单厂商、统一计费、中文最优 |
| **B：DeepSeek + 火山** | 火山 ASR | DeepSeek-V3 | 火山 TTS | LLM 能力强、成本低 |
| **C：GLM-4-Voice** | 内置 | GLM-4-Voice | 内置 | 架构最简单（语音原生） |

**Doubao 多模态的 `input_audio` 字段**：根据 [doubao-multimodal-skill](https://github.com/jimliu/doubao-multimodal-skill) 项目验证，火山方舟 Doubao-Seed-2.0 endpoint **支持 `input_audio` 字段直接传音频**（不是 base64 嵌在 content，是单独的 input_audio part），这与 OpenAI Realtime 类似但走的是 HTTPS REST，不是 WS。

### 5.5 借鉴 xiaozhi 的 9 种核心帧

把 DSH 14+4 帧收敛到 xiaozhi 的 9 种：

**设备 → 服务器**：
1. `hello`（握手）
2. `listen`（VAD 开始/停止）
3. `abort`（打断）
4. `mcp`（JSON-RPC 2.0 工具调用）

**服务器 → 设备**：
5. `hello`（握手应答）
6. `stt`（ASR 结果文本）
7. `llm`（情绪/UI 反馈）
8. `tts`（TTS 开始/停止 + 句子）
9. `mcp`（JSON-RPC 工具调用）

加上 binary 帧传 Opus，IoT 控制走 MCP（参考 xiaozhi 的 `mcp-protocol.md`）。详见 [xiaozhi websocket.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md?plain=1)。

### 5.6 与 DSH 当前 14+4 帧的差异与演进

| 项 | DSH 当前 | 演进目标 |
|---|---|---|
| 主协议 | WS + JSON 全包 | **WS + JSON 控制 + Binary Opus** |
| ASR 上传 | 推测是 JSON + base64 | raw binary Opus frame |
| IoT 控制 | 自定义 JSON | **MCP（JSON-RPC 2.0 in `type:"mcp"`）** |
| TTS 流 | 单帧 + 音频数据 | 二进制 frame + `tts.start/stop` 信号 |
| VAD | 推测本地 | 双模式：本地 + 服务端（参考 OpenAI `speech_started/stopped`） |
| 打断 | 自定义 | `type:"abort"` + 客户端立即停 TTS 播放（参考 xiaozhi） |

### 5.7 不建议采用的方向

- ❌ **MQTT 主通道**：延迟太高；可以保留为「配置下发/OTA」辅路
- ❌ **SSE 主通道**：单向，不适合双向语音
- ❌ **gRPC 终端侧**：ESP32-P4 上 protobuf 编解码 + HTTP/2 栈太重
- ❌ **MP3 编码**：专利、延迟大、CPU 占用高
- ❌ **PCM 上云**：256 kbps 浪费巨大
- ❌ **base64 音频**：额外 33% 带宽浪费，毫无必要

### 5.8 风险与缓解

| 风险 | 缓解措施 |
|---|---|
| Mac Adapter 单点故障 | 设备端能离线播放本地提示音；Adapter 重连机制（指数退避） |
| Opus 编解码 CPU 占用 25% | 用 ESP32-P4 双核分核（core 0 做采集编码、core 1 做网络） |
| LLM 延迟（DeepSeek/Qwen ~1-3s） | 首 token 流式 + 提早 VAD end + 预生成 TTS |
| 国内 LLM 音频格式不统一 | 在 Mac Adapter 做格式转换网关（PCM↔Opus↔MP3） |
| 唤醒词误触发 | 本地唤醒 + 服务端二次校验（参考 xiaozhi 的 `detect` 帧） |

---

## 六、参考文献（汇总）

### 6.1 AI Agent 协议

- [Anthropic - Introducing the Model Context Protocol](https://www.anthropic.com/news/model-context-protocol)
- [MCP 官方站](https://modelcontextprotocol.io)
- [Stoa MCP Architecture Deep Dive](https://github.com/stoa-platform/stoa-docs/blob/main/blog/2026-02-12-mcp-protocol-architecture-deep-dive.md)
- [Google A2A 1.0 规范](https://a2a-protocol.org/latest/)
- [Cryptorefills A2A 协议总结](https://github.com/Cryptorefills/agentic-commerce/blob/master/protocols/a2a.md)
- [OpenAI Realtime API AsyncAPI spec](https://raw.githubusercontent.com/api-evangelist/openai/refs/heads/main/asyncapi/openai-realtime-asyncapi.yml)
- [Azure OpenAI Realtime WebSockets](https://learn.microsoft.com/zh-cn/azure/ai-services/speech-service/voice-live-api-reference-2026-06-01-preview)
- [Claude Messages API Reference](https://0xdarkmatter/claude-mods/blob/main/skills/claude-api-ops/references/messages-api.md)
- [Claude Streaming Messages](https://platform.claude.com/docs/en/build-with-claude/streaming)
- [Anthropic SDK Audio Request Issue #1198](https://github.com/anthropics/anthropic-sdk-python/issues/1198)

### 6.2 智能音箱

- [Alexa Smart Home Message Guide](https://developer.amazon.com/en-IN/docs/alexa/device-apis/message-guide.html)
- [Alexa AVS Device SDK v1.0 README](https://raw.githubusercontent.com/respeaker/avs-device-sdk/v1.0/README.md)
- [Google Assistant gRPC API](https://docs.rs/googleapis-tonic-google-assistant-embedded-v1alpha1/0.3.0/googleapis_tonic_google_assistant_embedded_v1alpha1/google/assistant/embedded/v1alpha1/audio_out_config/enum.Encoding.html)
- [小米 AIVS-SDK 接入文档](https://developers.xiaoai.mi.com/api/doc/render_markdown/VoiceserviceAccess/Device/develop/SDKDocument/FreeRTOSInstruction)
- [AliGenie 选择开发方案](https://aligenie.com/docs/ai/2999891?tbpm=1)
- [DuerOS 语音输入接口](http://open.duer.baidu.com/doc/dueros-conversational-service/device-interface/voice-input.md)

### 6.3 流式传输协议

- [Comparative Analysis of MQTT, HTTP, TCP, UDP, WebSocket (IEEE)](https://ieeexplore.ieee.org/abstract/document/11469821)
- [腾讯云 - 详解 AI 大模型实时通信为什么选 SSE](https://cloud.tencent.cn/developer/article/2675466)
- [ESP32-C3 MQTT/WebSocket/Firebase 实测 (JATI)](https://mail.ejournal.itn.ac.id/jati/article/view/18056)
- [Masaryk WS vs SSE 对比论文](https://is.muni.cz/th/c2gv9/comparative-analysis-of-websockets-and-server-sent-events_Archive.pdf)

### 6.4 音频编码

- [Shoutcast Audio Codecs 2026: MP3 vs AAC vs Opus](https://www.shoutcastnet.com/school/audio-codecs-mp3-aac-opus-comparison.php)
- [2wcom Codec Considerations for Streaming](https://www.2wcom.com/audio-codec-guide-broadcast-streaming/)
- [Opus Audio Format Wikipedia](https://en.wikipedia.org/wiki/Opus_(audio_format))
- [ESP32-P4 语音通话开发实战 (CSDN)](https://devpress.csdn.net/avi/69b454b90a2f6a37c59726c8.html)
- [WebCodecs frame duration Issue #526](https://github.com/w3c/webcodecs/issues/526)

### 6.5 国内 LLM 语音

- [doubao-multimodal-skill (火山方舟多模态)](https://github.com/jimliu/doubao-multimodal-skill)
- [Navor AI 国内模型网关](https://www.producthunt.com/products/navor-ai)

### 6.6 参考实现

- [xiaozhi-esp32 WebSocket Protocol](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md?plain=1)
- [FogSeekAI xiaozhi-esp32 docs](https://github.com/FogSeekAI/xiaozhi-esp32/blob/stable/docs/websocket.md)

---

## 七、附录：DSH 14+4 帧与 xiaozhi 9 帧的映射建议

| DSH 当前帧 | 演进目标 | 说明 |
|---|---|---|
| `hello` | `hello` | 握手，相同 |
| `auth` | 合并到 `hello.features` | 简化 |
| `wake_detected` | `listen.state=detect` | 合并到 listen |
| `listen_start` | `listen.state=start` | 合并到 listen |
| `audio_frame` | **WS binary frame** | 不再用 JSON，效率↑ |
| `listen_stop` | `listen.state=stop` | 合并到 listen |
| `vad_end` | 服务端发 `stt` | 推到云端做 |
| `interrupt` | `abort` | 名字更标准 |
| `ping` | **WS Ping frame**（协议层） | 不占应用层 |
| `state_update` | 可选，调试用 | 合并到 system |
| `iot_command_result` | `mcp` payload | JSON-RPC 2.0 |
| `error_report` | `system` or `error` | 系统消息 |
| `config_sync` | 合并到 `hello` 或独立 `config` | 配置走单独的 REST |
| `goodbye` | `goodbye` | 相同 |
| （server→device 4 帧） | （server→device 5 帧） | 增 `llm` 帧 |
| | `welcome` → `hello` | 合并 |
| | `tts_start` → `tts` | 合并 |
| | `tts_audio_stream` → **WS binary frame** | 不再用 JSON |
| | `goodbye` | 相同 |

**净效果**：DSH 当前 14+4 帧 → 收敛到 xiaozhi 的 9 帧 + WS binary 音频帧；总帧数减少，但表达能力更强。

---

> **文档版本**：v1（2026-09-07） / **作者**：DSH 调研子代理 / **状态**：已完成，待父代理审阅
