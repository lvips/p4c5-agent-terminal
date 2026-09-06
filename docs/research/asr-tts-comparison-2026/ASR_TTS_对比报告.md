# 主流语音识别 (ASR) 与语音合成 (TTS) 服务详细对比报告

> **调研日期**：2026 年初 · 适用场景：ESP32 嵌入式终端 + Mac Adapter 混合架构
> **货币换算约定**：$1 ≈ ¥7.2（参考值）。"万字"=10,000 字符；"百万字符"=1,000,000 字符。

---

## 目录

1. [云端 ASR 服务对比（8 家）](#1-云端-asr-服务对比)
2. [端侧 ASR 方案对比（5 家）](#2-端侧-asr-方案对比)
3. [云端 TTS 服务对比（8 家）](#3-云端-tts-服务对比)
4. [端侧 TTS 方案对比（4 家）](#4-端侧-tts-方案对比)
5. [ESP32 + Mac Adapter 混合架构选型建议](#5-esp32--mac-adapter-混合架构选型建议)
6. [参考资料](#6-参考资料)

---

## 1. 云端 ASR 服务对比

### 1.1 总览矩阵

| 服务 | 价格（最低阶梯） | 流式首包延迟 | 中文 WER（参考） | API 模式 | 音频格式 | 单次最大时长 | 综合推荐 |
|---|---|---|---|---|---|---|---|
| **阿里云一句话识别 ISI** | $1.40 / 千次 | <300ms（一次性） | 行业基准 | REST | PCM/WAV/OPUS/MP3，16kHz | ≤60s | ⭐⭐⭐⭐ |
| **阿里云实时语音识别**（Paraformer/Gummy） | $1.40 / 小时 ≈ $0.023/分 | **<500ms**（WebSocket 流式） | ≈5% | WebSocket + REST | PCM/OPUS，16kHz | ≤8h | ⭐⭐⭐⭐⭐ |
| **火山引擎 ASR（豆包）** | 见大模型价格（积分/小时） | <500ms（流式） | ≈4–6%（大模型） | WebSocket | PCM/OPUS，16kHz | 长连接 | ⭐⭐⭐⭐⭐ |
| **讯飞开放平台 ASR**（语音听写） | ¥0.0001/秒 ≈ ¥0.006/分 | ≈300ms（WebAPI） | ≈5% | WebSocket（WebAPI） | PCM/WAV，16kHz | ≤60s（一句话）；流式无上限 | ⭐⭐⭐⭐⭐ |
| **百度智能云 ASR**（短语音/实时） | 短语音 ¥0.0042/次 起；实时 ¥0.006/15s | ≈300–500ms | ≈5–7% | REST + WebSocket | PCM/WAV/AMR/OPUS，16kHz | ≤60s（短语音）；流式长连接 | ⭐⭐⭐⭐ |
| **腾讯云 ASR** | 实时 $1.40/小时；大模型 $2.00/小时 | <500ms | 5–7%（基础）；≈4%（大模型） | WebSocket + REST | PCM/WAV/OPUS，16kHz | 长连接 | ⭐⭐⭐⭐ |
| **OpenAI Whisper API** | **$0.006 / 分钟**（约 ¥0.043/分） | 非流式，文件上传 | 中文 ≥10%（多语言混合训练） | REST（多部分上传） | MP3/MP4/M4A/WAV/WebM/OGG，≤25MB | ≤25MB 文件 | ⭐⭐⭐ |
| **Azure Cognitive Services Speech** | 实时 $1.05/小时 ≈ $0.018/分 | <500ms（Continuous） | ≈5–6% | WebSocket + REST | PCM/WAV/OGG，16kHz | 长连接 | ⭐⭐⭐⭐⭐ |

> 注：所有价格均为官方公布的 **标准价（最低用量阶梯）**，大客户/包年/预付费套餐通常可降至 30–50%。

### 1.2 各服务详细说明

#### 1) 阿里云一句话识别 ISI

- **价格**：$1.40 / 千次（≈ ¥0.01/次）；梯度：5000k 次以上 $0.70/千次。([Alibaba Cloud](https://www.alibabacloud.com/help/zh/isi/product-overview/pricing))
- **API 模式**：REST 一次性 / WebSocket（"一句话"模式 ≤ 60 秒）。([Alibaba Cloud](https://www.alibabacloud.com/help/zh/isi))
- **音频格式**：PCM 16k/8k、OPUS、MP3、WAV，单声道。([Alibaba Cloud](https://www.alibabacloud.com/help/zh/isi))
- **延迟**：客户端一次上传结束后服务端处理 ≈ 200–400ms。
- **中文 WER**：自研 DFSMN 模型，行业基准。
- **优点**：单次成本极低（按"次"计费，与音频长度无关）；免费试用期 2 并发/天；商业版默认 200 并发。
- **缺点**：必须整段上传，不适合长语音流；无中间结果返回。

#### 2) 阿里云实时语音识别（Paraformer / Gummy）

- **价格**：实时识别 $1.40/小时；Gummy 大模型单独立价（详见 [Model Studio 价目表](https://www.alibabacloud.com/help/en/model-studio/model-pricing)）。([Alibaba Cloud](https://www.alibabacloud.com/help/zh/isi/product-overview/pricing))
- **API 模式**：WebSocket 流式 + REST。支持**中间结果（partial）**、**最终结果（final）**。([Alibaba Cloud](https://www.alibabacloud.com/help/zh/isi))
- **音频格式**：PCM 16kHz 16bit 单声道 / OPUS 16k。
- **延迟**：首包 <500ms（边说边返回 partial）。
- **中文 WER**：Gummy 通用模型在多领域中文测试中 ≈ 5%。
- **优点**：流式 + 自带 VAD；商业稳定；SDK 完善（Java/Python/C++/Go/Android/iOS）。
- **缺点**：境外访问速度一般；价格梯度阶梯式，量大才便宜。

#### 3) 火山引擎（字节跳动）语音识别（豆包 ASR）

- **价格**：按大模型"模型单元 / TPM"售卖，详见 [豆包大模型价格清单](https://www.volcengine.com/product/tts) 与 [itheme 报道](https://www.ithome.com/0/769/505.htm)。ASR 大模型通常以"预付 + 后付"双模式，新用户有免费额度。
- **API 模式**：WebSocket 长连接流式（[豆包语音 API](https://www.volcengine.com/product/tts)）。
- **音频格式**：PCM 16kHz 单声道 / OPUS。
- **延迟**：流式首包 <500ms；据 [火山引擎社区实测](https://developer.volcengine.com/articles/7628812815666511908)，豆包 ASR 在对话场景延迟极低。
- **中文 WER**：大模型版本 4–6%（行业头部）。
- **优点**：与豆包 LLM 同生态，LLM→ASR 一条龙；声音复刻、情感控制能力强。
- **缺点**：需先开通火山账号；私有化部署不支持；商务门槛较高。

#### 4) 讯飞开放平台 ASR（语音听写 + 实时语音转写）

- **价格**：流式 WebAPI 按字符计费 ≈ ¥0.0001/秒（约 ¥0.006/分），具体阶梯以 [讯飞计费页](https://www.xfyun.cn/) 为准；免费额度 50000 次（部分服务）。([iFlytek Docs](https://www.xfyun.cn/))
- **API 模式**：WebSocket（WebAPI），长连接流式；支持一句话识别 REST。
- **音频格式**：PCM 16k/8k 16bit 单声道 / WAV / OPUS。
- **延迟**：首包 ≈ 300ms。
- **中文 WER**：中文领域头部，普通话 ≈ 5%；支持方言（粤语、四川话等）。
- **优点**：中文识别最准；流式协议成熟；WebSocket SDK 完善；新近推出 **超拟人 ASR / 音视频同传** 等大模型版本。
- **缺点**：海外节点少；价格梯度复杂（不同服务分开计费）；APPID/Key 申请流程较长。

#### 5) 百度智能云 ASR

- **价格**：短语音识别 ¥0.0042/次；实时长语音 ¥0.0001/秒 起；大模型实时另议。([Baidu Cloud](https://cloud.baidu.com/doc/SPEECH/s/qlcirqhz0))
- **API 模式**：REST 短语音 + WebSocket 实时。
- **音频格式**：PCM/WAV/AMR/OPUS/MP3，8k/16k 单声道。
- **延迟**：短语音 ≈ 300ms（一次性）；流式 ≈ 300–500ms。
- **中文 WER**：普通话 5–7%（基础模型）；大模型 S2S-Token 更低。
- **优点**：端到端 SDK 完善；Android/iOS/Linux 全平台；支持离线/在线混合。
- **缺点**：免费额度较小；价格梯度不如讯飞细致。

#### 6) 腾讯云 ASR

- **价格**：实时识别 $1.40/小时（≈ ¥10/小时）；**大模型 2.0 版本** $2.00/小时（[Tencent 官方计费](https://intl.cloud.tencent.com/document/product/1118/43352)）。梯度：5000h 以上 $0.70/h。
- **API 模式**：WebSocket 长连接（推荐）+ REST。
- **音频格式**：PCM/WAV/OPUS，8k/16k。
- **延迟**：<500ms 流式首包。
- **中文 WER**：基础 ≈ 5–7%；大模型 2.0 宣称 ≈ 4%。
- **优点**：粤语支持好；与腾讯云生态（微信、小程序）天然整合；**实时说话人分离**等增值能力。
- **缺点**：价格高于讯飞；大模型 2.0 商用版能力需单独开通。

#### 7) OpenAI Whisper API

- **价格**：**$0.006 / 分钟**（折合 ≈ ¥0.043/分 或 ≈ ¥2.6/小时）。文件 ≤ 25 MB。([OpenAI Pricing](https://openrouter.ai/openai/whisper-1))
- **API 模式**：REST（`POST /v1/audio/transcriptions`），多部分上传。
- **音频格式**：mp3、mp4、mpeg、mpga、m4a、wav、webm、ogg、flac。
- **延迟**：非流式，1 分钟音频通常 2–5s 返回。
- **中文 WER**：10% 以上（Whisper 多语言混合训练，中文相对弱）。小模型 `whisper-1` 实际后端为 large-v2/v3。
- **优点**：免配置；多语言（99 种）；英文识别顶级。
- **缺点**：**不支持流式**；中文识别弱于国产；上传境外服务器，存在合规/延迟问题。

#### 8) Azure Cognitive Services Speech

- **价格（中国区）**：标准版实时 $1.05/小时（≈ ¥7.5/小时）；免费层每月 5 小时音频。([Azure 中国定价](https://www.azure.cn/en-us/pricing/details/cognitive-services/))
- **API 模式**：WebSocket（推荐）+ REST + SDK（C++/C#/Python/Java/JS）。
- **音频格式**：PCM/WAV/OGG，8k/16k/24k。
- **延迟**：连续识别模式（Continuous）首包 <500ms。
- **中文 WER**：≈ 5–6%（普通话）；支持中英混合。
- **优点**：海外稳定；**自定义语音模型（Custom Speech）** 能力；情绪识别、音素评估等增强功能。
- **缺点**：海外服务，中国区开通复杂；价格梯度细节多。

---

## 2. 端侧 ASR 方案对比

### 2.1 总览矩阵

| 方案 | 支持硬件 | Flash 模型 | RAM（运行时） | 中文准确率 | 包含唤醒词 | 首包延迟 | 活跃度 |
|---|---|---|---|---|---|---|---|
| **ESP-SR**（WakeNet + MultiNet） | ESP32/ESP32-S3/ESP32-P4/ESP32-C3/ESP32-C5 | WakeNet9 ≈1MB + MultiNet 1–3MB | WakeNet ≈ 15–24 KB；MultiNet ≈ 22 KB | 中文命令词 ≈ 97%（3m 静默） | ✅ 内置 50+ 唤醒词 | <200ms | ⭐⭐⭐⭐⭐ Espressif 官方 |
| **Picovoice Porcupine** | ESP32/Raspberry Pi/Linux/Android/iOS/浏览器 | 唤醒词模型 < 1 MB | < 30 MB | 英文为主（中文自定义需企业版） | ✅ 内置英文；中文需训练 | <100ms | ⭐⭐⭐⭐ Picovoice 官方 |
| **Vosk**（Kaldi 端侧） | Linux x86 / Raspberry Pi 3/4 / Android | 中文 small ≈ 42 MB / large ≈ 1.2 GB | 200 MB+（small） | CER 6–8%（small） | ❌ 仅 ASR；唤醒需 snowboy | 0.5–1s（CPU）/ 实时 | ⭐⭐⭐⭐ Alphacephei 官方 |
| **Whisper.cpp**（本地 Whisper） | Linux/Mac/Raspberry Pi 4/5/Jetson | tiny 75 MB / base 142 MB / small 466 MB | 1–4 GB（按模型） | CER 8–12%（tiny） / 5–7%（small） | ❌ 仅 ASR | tiny 实时 / base 0.5x / small 0.1x | ⭐⭐⭐⭐⭐ GitHub 51k stars |
| **Sherpa-onnx** | Linux x86 / ARM / Raspberry Pi / Android / iOS / HarmonyOS | Paraformer-zh ≈ 220 MB；Zipformer-zh 14M ≈ 35 MB；SenseVoice ≈ 230 MB | 200 MB – 1 GB | CER 5–8%（Zipformer/SenseVoice） | ✅ 内置 KWS（唤醒词） | <300ms（流式） | ⭐⭐⭐⭐⭐ k2-fsa 官方，活跃 |

### 2.2 各方案详细说明

#### 1) ESP-SR（Espressif 官方）

- **仓库**：[github.com/espressif/esp-sr](https://github.com/espressif/esp-sr)，最新 v2.5.x。
- **架构**：AFE（音频前端）→ WakeNet（唤醒）→ MultiNet（命令词识别）→ ESP-TTS（合成）。
- **资源占用**（[官方 Benchmark](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/benchmark/README.html)）：
  - AFE：AEC 114KB + NS 27KB + AFE Layer 73KB ≈ 200KB RAM
  - WakeNet5（量化）：15KB RAM，参数 41K
  - WakeNet9 / WakeNet9s：参数 41K–371K，RAM 15–24 KB
  - MultiNet：内部 13.3KB + PSRAM 9KB
  - **ESP-TTS**：Flash 2.2 MB，RAM 20 KB（240MHz 时合成速度 = 实时 × 4.5）
- **中文准确率**：
  - WakeNet9：1m 静默 98% / 3m 静默 98% / SNR=4dB 噪声 96%
  - MultiNet7_cn：命令词识别 ≈ 97.2%（3m 静默）
- **包含唤醒词**：✅ 50+ 内置中文/英文/日/法等（TTS Pipeline V3 已支持中/英/日/法 4 语唤醒词训练）。
- **延迟**：唤醒 < 200ms；命令词 < 300ms（30ms 帧）。
- **活跃度**：⭐⭐⭐⭐⭐，Espressif 官方持续维护，2025–2026 频繁发布 WakeNet9s/l、VADNet 等。

#### 2) Picovoice Porcupine（唤醒词）

- **官网**：[picovoice.ai](https://picovoice.ai/docs/porcupine/)。
- **价格**：开源 SDK **个人免费**；商业授权按设备/月计费（具体需联系 Picovoice 销售，参考 [Porcupine Skill4Agent](https://skill4agent.com/zh/skill/framersai-agentos-skills/porcupine)）。
- **资源占用**：模型 < 1MB，RAM < 30MB（在 Cortex-M 上亦可运行）。
- **支持硬件**：ESP32（含 ESP32-S3）、Raspberry Pi、Android、iOS、浏览器（WebAssembly）。
- **中文支持**：内置英文唤醒词（Picovoice / Porcupine / Alexa / Hey Google 等）；**中文唤醒需企业训练或自建 keyword**。
- **延迟**：<100ms（Cortex-A/M4）。
- **优点**：跨平台 SDK 一致；端侧隐私；可定制唤醒词；**与 Leopard/Cheetah STT 引擎同生态**。
- **缺点**：中文唤醒词生态弱于 ESP-SR；商业授权需报价。

#### 3) Vosk（端侧 Kaldi）

- **仓库**：[github.com/alphacep/vosk-api](https://github.com/alphacep/vosk-api)。
- **支持硬件**：Linux x86/Raspberry Pi 3/4/Android/iOS。
- **模型大小**：中文 small ≈ 42 MB / large ≈ 1.2 GB。
- **中文准确率**：CER 6–8%（small），更准的 large 模型接近商用。
- **包含唤醒词**：❌（需配合 Snowboy/Picovoice 做 KWS）。
- **延迟**：CPU 上 ≈ 0.5–1x 实时（small）；GPU/树莓派4 上实时。
- **优点**：开源 Apache 2.0；支持中文/英文/20+ 语种；Python/Java/C++/Node 多语言。
- **缺点**：模型偏大；无内置唤醒；中文 WER 不如 Whisper large-v3。

#### 4) Whisper.cpp（本地 Whisper）

- **仓库**：[github.com/ggerganov/whisper.cpp](https://github.com/ggerganov/whisper.cpp)（51k+ stars）。
- **模型大小**：tiny 75 MB / base 142 MB / small 466 MB / medium 1.5 GB / large-v3 3.1 GB。
- **中文准确率**：CER 8–12%（tiny） / 5–7%（small） / ≈ 5%（large）。
- **包含唤醒词**：❌（仅 ASR）。
- **延迟**（[树莓派实测](https://www.cnblogs.com/labhub/p/22669565)）：
  - 树莓派 4（4GB）：tiny 实时，base 0.5x，small ≈ 0.1x 实时
  - Mac M1/M2：base 实时，small ≈ 0.7x
- **优点**：完全离线；多语言；与 OpenAI Whisper 同源；GPU/Metal/CoreML 加速。
- **缺点**：模型大；无流式中间结果（需切片）；中文端侧推荐 small+。

#### 5) Sherpa-onnx（中文 ASR 端侧主流）

- **仓库**：[github.com/k2-fsa/sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)。
- **核心模型**：
  - **Paraformer-zh**（阿里达摩院）：≈ 220 MB，中文 CER 5–7%，支持方言。
  - **Zipformer-zh**（k2-fsa）：14M 小模型 ≈ 35 MB，CER 6–8%，**Cortex-A7 可跑**。
  - **SenseVoice**（阿里 FunAudioLLM）：≈ 230 MB，多语种，支持情绪识别。
  - **Moonshine / Whisper / TeleSpeech**：英/俄/日/韩等多语种模型。
- **支持硬件**：Linux x86 / ARM / Raspberry Pi 4/5 / NVIDIA Jetson / RK3588 / VisionFive 2 / Android / iOS / HarmonyOS / WebAssembly。([Sherpa-onnx README](https://github.com/k2-fsa/sherpa-onnx))
- **包含唤醒词**：✅ 内置 KWS（keyword spotting），3M 参数级小模型即可做唤醒。
- **延迟**：流式首包 <300ms；CPU 实时因子 < 1（Zipformer-zh-14M）。
- **优点**：中文端侧首选；模型丰富；社区活跃（**xiaozhi-esp32-server 已支持**）；同时支持 ASR + TTS + VAD + KWS 一站式。
- **缺点**：大模型需 200MB+；配置 ONNX Runtime 较繁琐。

---

## 3. 云端 TTS 服务对比

### 3.1 总览矩阵

| 服务 | 价格（最低阶梯） | 中文音色数 | 流式 | 音频格式 | 首包延迟 | 音质主观评分 |
|---|---|---|---|---|---|---|
| **火山引擎 TTS**（豆包语音 2.0） | **¥150 / 音色 / 年** + 字数包 10 万字起 | 40+ | ✅ WebSocket | MP3/OPUS/PCM/WAV | **<300ms** | ⭐⭐⭐⭐ |
| **微软 Azure TTS**（Neural） | **¥95.4 / 百万字符**（约 ¥0.000095/字） + 免费 0.5M 字符/月 | 50+ | ✅ WebSocket / SSML | MP3/OPUS/WAV/PCM | 300–500ms | ⭐⭐⭐⭐⭐ |
| **阿里云 TTS**（智能语音） | **$1.40 / 千次**（按次，≤300 字符/次） | 60+ | ✅ WebSocket | MP3/PCM/WAV/OPUS | <500ms | ⭐⭐⭐⭐ |
| **腾讯云 TTS**（长文本语音合成） | 长文本 ¥0.20/万字；流式 ¥0.18/万字 | 50+ | ✅ WebSocket | MP3/PCM/WAV | 300–500ms | ⭐⭐⭐⭐ |
| **讯飞 TTS**（超拟人语音合成） | 按字符计费（官方套餐）；并发套餐另议 | **80+**（x6 系列） | ✅ **双向流式 WebSocket** | MP3/PCM/OPUS/Speex，16k/24k | **<300ms**（首响） | ⭐⭐⭐⭐⭐ |
| **Edge TTS**（免费爬虫） | **完全免费**（无 key） | 300+（晓晓/云希/云野 等） | ✅ WebSocket | MP3/OPUS/PCM | 200–500ms | ⭐⭐⭐⭐ |
| **ElevenLabs** | $5/月 起（Starter 30k 字符），$22/月（Creator 100k） | 30+ 多语言（含中文 Multilingual V2） | ✅ WebSocket | MP3/PCM | 300–800ms | ⭐⭐⭐⭐⭐ |
| **OpenAI TTS** | **tts-1：$15/百万字符；tts-1-hd：$30/百万字符** | 6 个语音（Nova/Shimmer/Alloy/Echo/Fable/Onyx） | ✅ 流式 | MP3/OPUS/PCM/WAV | 200–500ms | ⭐⭐⭐⭐ |

### 3.2 各服务详细说明

#### 1) 火山引擎 TTS（豆包语音 2.0）

- **价格**（[火山引擎 TTS 产品页](https://www.volcengine.com/product/tts)）：¥150 / 音色 / 年（企业路线）；字数包 10 万字起售；新用户免费试用。
- **音色**：40+ 中文音色（含流式对话专用音色），支持**声音复刻 2.0（秒级克隆）**。
- **流式**：✅ WebSocket 流式，首包 <300ms（[实测对比](https://developer.volcengine.com/articles/7628812815666511908)）。
- **音频格式**：MP3 / OPUS / PCM / WAV。
- **延迟**：实测 <300ms 流式首包。
- **音质**：⭐⭐⭐⭐，自然度高、情感控制能力强（支持 `<整体情绪：生气>` 等指令式标签）。
- **优点**：与豆包 LLM 同生态；情感控制行业领先；声音复刻质量好。
- **缺点**：按"年/音色"计费对小项目偏贵；需火山账号。

#### 2) 微软 Azure TTS

- **价格**（[Azure 中国定价](https://www.azure.cn/en-us/pricing/details/cognitive-services/)）：Neural 标准 **¥95.4 / 百万字符**（≈ ¥0.095/千字）；免费层每月 50 万字符。
- **音色**：50+ 中文音色（含晓晓、云希、云野、晓伊、晓涵等明星音色）；支持 SSML + 神经风格（cheerful/sad/whisper 等）。
- **流式**：✅ WebSocket / SSML；SDK 全平台。
- **音频格式**：MP3 / OPUS / WAV / PCM，8k/16k/24k/48k。
- **延迟**：300–500ms（区域相关）。
- **音质**：⭐⭐⭐⭐⭐，行业天花板，**神经语音自然度极高**。
- **优点**：多语言/多风格；私有化支持（Connected Container）；与微软 Azure 生态整合。
- **缺点**：海外节点延迟高；中文音色数量不如讯飞。

#### 3) 阿里云 TTS（智能语音）

- **价格**（[阿里云 ISI 定价](https://www.alibabacloud.com/help/zh/isi/product-overview/pricing)）：**$1.40 / 千次**（每 ≤100 字符 1 次）；梯度：5000k 次以上 $0.70/千次。
- **音色**：60+ 中文音色（含客服、客服助理、儿童、明星 IP 等）。
- **流式**：✅ WebSocket（实时语音合成）；支持**超自然语音**模型（Sambert）。
- **音频格式**：MP3 / PCM / WAV / OPUS，8k/16k/24k。
- **延迟**：<500ms（流式）。
- **音质**：⭐⭐⭐⭐。
- **优点**：与 ASR 同账号；商业稳定；价格梯度清晰。
- **缺点**：按"次"计费，长文本成本可能高于按字符计费的服务。

#### 4) 腾讯云 TTS（长文本语音合成 / 流式）

- **价格**：长文本合成 **¥0.20 / 万字**；流式 **¥0.18 / 万字**；新客优惠价更低（[腾讯云语音优惠页](https://cloud.tencent.com.cn/act/pro/yuyin)）。
- **音色**：50+ 中文音色；支持定制声音。
- **流式**：✅ WebSocket（流式版），首包 < 500ms。
- **音频格式**：MP3 / PCM / WAV。
- **延迟**：300–500ms。
- **音质**：⭐⭐⭐⭐。
- **优点**：长文本便宜；与微信小程序生态整合；**腾讯云 ASR 同账号**。
- **缺点**：新音色迭代较慢。

#### 5) 讯飞超拟人语音合成

- **价格**：按字符套餐售卖（[讯飞官网](https://www.xfyun.cn/)），并发套餐按并发数售卖；典型 ¥0.0002/字符 ≈ ¥2/万字。
- **音色**（[讯飞超拟人 TTS 文档](https://www.xfyun.cn/doc/spark/super%20smart-tts.html)）：**x6 系列 80+ 中文音色**，含聆小璇、聆飞逸、聆小玥等"对话专用音色"，方言（粤语/东北话/天津话/台湾话）。
- **流式**：✅ **双向流式 WebSocket**，首响 ≈ 300ms（"流式合成首响"详见官方文档）。
- **音频格式**：MP3（lame）/ PCM（raw）/ Speex / OPUS，16k/24k。
- **延迟**：< 300ms 流式首响。
- **音质**：⭐⭐⭐⭐⭐，业内自然度第一梯队；**情感/口语化（oral_level）控制强**。
- **优点**：中文音色最多；双向流式协议适合 LLM 实时输出；可发音人克隆。
- **缺点**：海外节点少；并发套餐偏贵。

#### 6) Edge TTS（免费）

- **价格**：**完全免费**，无 API key（[github.com/XiangCoder/edge-tts](https://github.com/XiangCoder/edge-tts)）。
- **原理**：逆向调用 Microsoft Edge 浏览器在线 TTS 服务。
- **音色**：300+（含晓晓、云希、云野、晓涵等所有 Azure Neural 音色）。
- **流式**：✅ WebSocket / `communicate` 协议。
- **音频格式**：MP3 / OPUS / PCM / WAV（默认 mp3）。
- **延迟**：200–500ms（视网络）。
- **音质**：⭐⭐⭐⭐（与 Azure 同源）。
- **优点**：零成本；音色丰富；Python 一行启动。
- **缺点**：**非官方 API**，Microsoft 可随时封禁；**不可商用**；并发/稳定性差；地区受限（需能访问微软服务器）。

#### 7) ElevenLabs

- **价格**（[ElevenLabs Pricing 2026](https://texttolab.com/blog/elevenlabs-pricing)）：
  - **Free**：$0 / 月，10k 字符
  - **Starter**：$5 / 月，30k 字符
  - **Creator**：$22 / 月，100k 字符
  - **Pro**：$99 / 月，500k 字符
  - **Scale**：$330 / 月，2M 字符
  - API：超出按 $0.30 / 1k 字符（Multilingual V2）
- **音色**：30+ 多语言（Multilingual V2）；**支持声音克隆**（10–30 秒样本即可）。
- **流式**：✅ WebSocket。
- **音频格式**：MP3 / PCM。
- **延迟**：300–800ms。
- **音质**：⭐⭐⭐⭐⭐，英文/西语全球第一；中文 Multilingual V2 进步大。
- **优点**：声音克隆最快；多语言；超拟人。
- **缺点**：价格偏高；中文支持不如讯飞；境外服务延迟。

#### 8) OpenAI TTS

- **价格**（[OpenAI Pricing](https://developers.openai.com/api/docs/pricing)）：
  - **tts-1**：$15 / 百万字符（≈ ¥0.011 / 千字）
  - **tts-1-hd**：$30 / 百万字符
- **音色**：6 个（alloy / echo / fable / onyx / nova / shimmer）；均为英文；中文也可读但音色非中文优化。
- **流式**：✅ 流式响应（SSE）；默认 MP3，可选 OPUS/PCM/WAV。
- **音频格式**：MP3 / OPUS / PCM / WAV。
- **延迟**：200–500ms。
- **音质**：⭐⭐⭐⭐。
- **优点**：免配置；质量稳定；与 OpenAI LLM 同生态。
- **缺点**：中文音色少；境外合规/延迟问题。

---

## 4. 端侧 TTS 方案对比

### 4.1 总览矩阵

| 方案 | 支持硬件 | 模型大小 | RAM | 中文质量 | 流式 | 活跃度 |
|---|---|---|---|---|---|---|
| **ESP-SR TTS** | ESP32-S3/P4 | 2.2 MB | 20 KB | ⭐⭐⭐（机械感） | ❌ | ⭐⭐⭐⭐⭐ Espressif 官方 |
| **ESP-tts**（社区） | ESP32-S3 | 1–3 MB | 30 KB | ⭐⭐⭐ | ❌ | ⭐⭐⭐ |
| **Piper TTS** | Linux x86 / Raspberry Pi / Android | 中文 medium ≈ 63 MB；low ≈ 15 MB | 100–300 MB | ⭐⭐⭐⭐ | ✅ | ⭐⭐⭐⭐⭐ rhasspy/piper |
| **VITS / Bert-VITS2 / Matcha** | Linux x86 / Raspberry Pi 4/5 / Android | 100–500 MB | 200MB – 1GB | ⭐⭐⭐⭐⭐ | ✅ | ⭐⭐⭐⭐ |
| **sherpa-onnx TTS** | Linux/ARM/Android/iOS/HarmonyOS | VITS/Matcha/ZipVoice 中文 50–200 MB | 100–500 MB | ⭐⭐⭐⭐ | ✅ | ⭐⭐⭐⭐⭐ k2-fsa 官方 |

### 4.2 各方案详细说明

#### 1) ESP-SR TTS（乐鑫官方）

- **架构**：[github.com/espressif/esp-sr/esp-tts](https://github.com/espressif/esp-sr)。
- **资源**（[Benchmark](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/benchmark/README.html)）：Flash 2.2 MB，RAM 20 KB，合成速度 = 实时 × 4.5（240MHz）。
- **中文质量**：⭐⭐⭐，机械感明显，但语音清晰；适合播报。
- **支持硬件**：ESP32-S3 / ESP32-P4（v2.5 起支持）。
- **缺点**：音质较"机器人"，无法表达情感；只支持中文。

#### 2) ESP-tts（社区方案）

- **仓库**：[github.com/JunweiLiang/esp-tts](https://github.com/JunweiLiang/esp-tts) 等，基于 Tacotron2 / VITS 蒸馏。
- **资源**：模型 1–3 MB，RAM 30–50 KB。
- **中文质量**：⭐⭐⭐，比 ESP-SR 略好。
- **缺点**：社区维护，更新慢；音色较少。

#### 3) Piper TTS（rhasspy）

- **仓库**：[github.com/rhasspy/piper](https://github.com/rhasspy/piper)。
- **模型**：ONNX 格式；中文模型推荐 [piper-zh-cn-huayan-medium](https://huggingface.co/Trelis/piper-zh-cn-huayan-medium)（63 MB），也有 low (~15 MB)。
- **资源**：CPU 上 100–300 MB RAM；树莓派 4 上实时。
- **中文质量**：⭐⭐⭐⭐，自然度中等偏上。
- **流式**：✅（按 chunk 输出）。
- **优点**：开源；多语言（40+）；ONNX 跨平台。
- **缺点**：中文声音克隆质量一般；中等模型对树莓派 4 CPU 占用较高。

#### 4) VITS / Bert-VITS2 / Matcha（高保真端侧 TTS）

- **VITS**：[github.com/Voine/VITS-MNN](https://github.com/Voine/VITS-MNN)（移动端 MNN 框架）。
- **Bert-VITS2**：[github.com/Voine/Bert-VITS2-MNN](https://github.com/Voine/Bert-VITS2-MNN)。
- **资源**：模型 100–500 MB；RAM 200 MB–1 GB；适合树莓派 4/5、Mac、Jetson。
- **中文质量**：⭐⭐⭐⭐⭐，接近商用云端；Bert-VITS2 支持自定义音色。
- **流式**：✅。
- **缺点**：模型大；树莓派 4 上 small 模型实时，更大模型 0.5x 实时。

#### 5) sherpa-onnx TTS（推荐）

- **仓库**：[github.com/k2-fsa/sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)（TTS 部分 [k2-fsa TTS docs](https://k2-fsa.github.io/sherpa/onnx/tts/)）。
- **模型**：
  - **Matcha-zh**：中文，约 60–100 MB，自然度 ⭐⭐⭐⭐
  - **VITS-zh**：约 100 MB
  - **ZipVoice / Pocket TTS**：声音克隆
- **支持硬件**：Linux/ARM/Raspberry Pi/Android/iOS/HarmonyOS；WebAssembly。
- **优点**：与 sherpa-onnx ASR 同生态，**一套库搞定 ASR + TTS + VAD + KWS**；Web demo 多；活跃。
- **缺点**：中文 TTS 模型需自己挑选（社区贡献）。

---

## 5. ESP32 + Mac Adapter 混合架构选型建议

> **目标场景**：资源受限的 ESP32 嵌入式终端（P4/S3）作为前端采集 + 离线唤醒/命令词；Mac/PC Adapter 作为云端 AI 中转层，统一对接 ASR/TTS/LLM 服务。

### 5.1 总体架构图

```
┌─────────────────────┐   USB/Wi-Fi    ┌──────────────────────────┐   云端   ┌────────────┐
│  ESP32-P4 / S3 前端 │ ─────────────► │  Mac/PC Adapter 中转层  │ ────────► │ ASR / LLM  │
│  • WakeNet 唤醒     │                │  • 流式协议转换          │           │ TTS 云服务 │
│  • MultiNet 命令词  │                │  • 重试/缓冲            │ ◄───────  └────────────┘
│  • AFE 音频前端     │ ◄───────────── │  • 多服务路由           │
│  • ESP-TTS 备选播报 │   音频流       │  • 用量统计             │
└─────────────────────┘                └──────────────────────────┘
        端侧                              Mac 端
```

### 5.2 端侧（ESP32-P4/S3）— 强烈推荐

#### 唤醒词 + 命令词：ESP-SR v2.5

- **WakeNet9s**（无 PSRAM 也能跑）或 **WakeNet9**（性能更强）：中文唤醒 "你好小智" 等内置词
  - RAM 15–24 KB；模型 < 1 MB Flash
  - 准确率 98%（1m 静默）/ 96%（SNR=4dB 噪声）/ 误触发 < 1次/12h
- **MultiNet7_cn**：中文命令词识别（"打开空调"、"调高音量"等 ≤ 300 条命令）
  - RAM 22 KB（内 13.3KB + PSRAM 9KB）
- **AFE**：AEC + NS + VAD 一体
  - RAM 约 200 KB；CPU 占用 16%（双核）
- **资源总计**：Flash 5–8 MB（含模型）；RAM 250–300 KB；CPU 占用 < 30%

> ✅ **结论**：ESP-SR 是 ESP32 平台上**唯一一站式**成熟方案，强烈推荐。

#### 备选唤醒词：Picovoice Porcupine

- 仅当需要**英文唤醒**或**超低功耗**（< 30MB RAM）时考虑；中文需企业训练。

#### 端侧 TTS 备选：ESP-SR TTS

- 适合"语音播报固定短语"（如"好的，正在为您打开空调"），2.2 MB Flash、20 KB RAM。
- 音质较差，仅作 **failover**：当 Mac Adapter 不可达时本地应急。

### 5.3 Mac Adapter 端 — 推荐方案

#### ASR 推荐：讯飞 / 火山引擎 / 阿里云 FunASR 三选一

| 优先级 | 方案 | 理由 |
|---|---|---|
| ⭐⭐⭐⭐⭐ | **阿里云 Paraformer/Gummy（实时）** | 流式 WebSocket < 500ms；中文 WER ≈ 5%；价格 $1.40/h 适中；SDK 完善 |
| ⭐⭐⭐⭐⭐ | **讯飞 WebAPI 流式** | 中文识别行业最强；双向流式协议稳定；可复用同一 APPID 的 TTS 服务 |
| ⭐⭐⭐⭐ | **火山引擎豆包 ASR** | 与豆包 LLM 同生态，一条链路省运维 |

**避坑**：
- **不要用 OpenAI Whisper API** 作为主 ASR——不支持流式、中文弱、跨境合规问题；
- **不要用一句话识别 ISI** 作主路径——必须整段上传，不能流式返回中间结果，体感差；
- 若产品要进**海外市场**，Azure Speech 是唯一合规选择。

#### TTS 推荐：讯飞超拟人 / 火山豆包 / Edge TTS（备选）

| 优先级 | 方案 | 理由 |
|---|---|---|
| ⭐⭐⭐⭐⭐ | **讯飞超拟人语音合成** | 中文音色 80+；双向流式协议与 LLM 输出天然契合；首响 < 300ms |
| ⭐⭐⭐⭐⭐ | **火山引擎豆包 TTS** | 流式 < 300ms；情感控制强；声音克隆 |
| ⭐⭐⭐⭐ | **Edge TTS**（个人项目/开发测试） | 完全免费；300+ Azure 同源音色 |
| ⭐⭐⭐⭐ | **Azure Neural**（海外/企业） | 音质 ⭐⭐⭐⭐⭐；中文音色 50+ |
| 备选 | **ElevenLabs** | 仅当需要英文声音克隆或多语言混读时 |

**避坑**：
- **不要用阿里云 TTS 的"按次"计费** 处理长文本——超过 300 字符按 100 字符/次分段计费，成本高；
- **不要用 OpenAI TTS** 作主路径——6 个英文音色，中文无优化。

#### Mac Adapter 额外建议

1. **多服务路由**：讯飞（火山引擎）+ Edge（备用）+ Azure（海外）三方 fallback，确保单点故障不影响体验。
2. **流式转接**：用 [FunASR runtime](https://github.com/modelscope/FunASR) 或 [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) 作 Mac 端"本地热备"ASR，断网时仍可识别。
3. **本地 TTS 备选**：Mac 端可跑 **Piper** 或 **sherpa-onnx VITS/Matcha-zh**，断网时本地合成（音质尚可）。
4. **音频格式**：统一 OPUS 16kHz（流式友好、带宽省）；ESP32 → Mac 用 WebSocket / USB Audio。

### 5.4 资源评估

| 模块 | 端侧 (ESP32-P4) | Mac Adapter | 云端 |
|---|---|---|---|
| **Flash** | 8 MB（含 ESP-SR 模型） | 50 MB（含 sherpa-onnx 备用） | — |
| **RAM** | 300 KB（SRAM+PSRAM） | 500 MB–1 GB（ONNX Runtime） | — |
| **CPU 占用** | < 30%（双核） | < 5%（Intel/Apple Silicon） | — |
| **带宽（单路）** | 上行 32 kbps（OPUS 16k） | — | 32 kbps 上行 / 64 kbps 下行 |
| **功耗（待唤醒）** | ~80 mA | — | — |
| **算力要求** | ESP32-P4 400 MHz 双核 RISC-V + AI 扩展 | Mac M1/M2 或 Intel i5+ | — |

> ✅ **ESP32-P4**（400 MHz 双核 RISC-V + AI 扩展 + 768KB SRAM + PSRAM）**完全胜任** ESP-SR v2.5 全部功能。

### 5.5 成本估算（每月 1000 次对话）

**假设**：单次对话 = 用户说 15 秒 + AI 回复 30 秒语音；用户音色为中文普通话。

#### 推荐方案 A：讯飞 + Edge TTS（个人/小团队首选）

| 项目 | 计量 | 单价 | 月费用 |
|---|---|---|---|
| 讯飞 ASR 流式（WebAPI） | 15 s × 1000 = 4.2 h | ¥0.0001/s ≈ ¥0.6/分 ≈ ¥36/h | **¥150** |
| Edge TTS（免费） | 30 s × 1000 = 8.3 h ≈ 60 万字符 | ¥0 | **¥0** |
| **合计** | | | **≈ ¥150 / 月** |

#### 推荐方案 B：阿里云 + 火山豆包（企业首选）

| 项目 | 计量 | 单价 | 月费用 |
|---|---|---|---|
| 阿里云 Paraformer 实时 | 4.2 h | $1.40/h ≈ ¥10/h | **¥42** |
| 火山豆包 TTS（流式） | 60 万字符 | 字数包（10 万字 ≈ ¥6） | **¥36** |
| 火山音色包（1 个） | — | ¥150 / 音色 / 年 | **¥12.5 / 月** |
| **合计** | | | **≈ ¥90 / 月** |

#### 推荐方案 C：全 Edge（零成本开发原型）

| 项目 | 计量 | 单价 | 月费用 |
|---|---|---|---|
| Edge TTS（反向爬虫） | 60 万字符 | ¥0 | **¥0** |
| 本地 sherpa-onnx ASR（Paraformer-zh） | 4.2 h | ¥0（Mac 端运行） | **¥0** |
| **合计** | | | **¥0 / 月** |

> ⚠️ **注意**：Edge TTS **不可商用**（违反微软 ToS）；1000 次/月以下推荐 A/B 方案。

#### 高阶方案：Whisper API + OpenAI TTS（不推荐主用）

| 项目 | 计量 | 单价 | 月费用 |
|---|---|---|---|
| OpenAI Whisper | 4.2 h = 252 min | $0.006/min | **$1.51 ≈ ¥11** |
| OpenAI TTS-1 | 60 万字符 | $15 / 百万字符 | **$9 ≈ ¥65** |
| **合计** | | | **≈ ¥76 / 月** |

> 但因中文识别/合成均不如国产，仅适合海外或多语言混读场景。

---

## 6. 参考资料

### 云端 ASR

1. [阿里云智能语音交互 - 产品定价（2024-09-23）](https://www.alibabacloud.com/help/zh/isi/product-overview/pricing)
2. [Alibaba Cloud Model Studio Pricing（FunASR/Paraformer）](https://www.alibabacloud.com/help/en/model-studio/model-pricing)
3. [火山引擎 - 豆包语音 TTS 产品页](https://www.volcengine.com/product/tts)
4. [火山引擎 TTS vs 主流方案实测对比（2026 版）](https://developer.volcengine.com/articles/7628812815666511908)
5. [火山豆包大模型价格清单（ITHome 报道）](https://www.ithome.com/0/769/505.htm)
6. [讯飞开放平台 - 超拟人语音合成 API 文档](https://www.xfyun.cn/doc/spark/super%20smart-tts.html)
7. [讯飞开放平台首页](https://www.xfyun.cn/)
8. [百度智能云 - 语音技术文档](https://cloud.baidu.com/doc/SPEECH/s/qlcirqhz0)
9. [腾讯云 ASR 计费概述](https://cloud.tencent.cn/document/product/1093/35686)
10. [Tencent Cloud ASR Billing Overview（国际站）](https://intl.cloud.tencent.com/document/product/1118/43352)
11. [OpenAI Whisper API 模型文档](https://developers.openai.com/api/docs/models/whisper-1)
12. [OpenAI Whisper-1 API 第三方报价](https://openrouter.ai/openai/whisper-1)
13. [Azure 中国 AI Services 定价详情](https://www.azure.cn/en-us/pricing/details/cognitive-services/)
14. [Azure Speech 介绍（短语音识别价格综述）](https://aca.bce.baidu.com/doc/SPEECH/s/qlcirqhz0)

### 端侧 ASR

15. [ESP-SR GitHub（Espressif 官方）](https://github.com/espressif/esp-sr)
16. [ESP-SR v2.5.0 组件文档](https://components.espressif.com/components/espressif/esp-sr/versions/2.5.0/readme)
17. [ESP-SR WakeNet 文档](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/README.html)
18. [ESP-SR Benchmark（资源占用）](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/benchmark/README.html)
19. [ESP-SR TTS（esp-tts README）](https://raw.githubusercontent.com/espressif/esp-sr/master/esp-tts/README.md)
20. [Picovoice Porcupine 官方文档](https://picovoice.ai/docs/porcupine/)
21. [Porcupine Wake Word 介绍（Skill4Agent）](https://skill4agent.com/zh/skill/framersai-agentos-skills/porcupine)
22. [Vosk 开源 ASR 实践指南（百度智能云）](https://cloud.baidu.com/article/3956040)
23. [Whisper.cpp 树莓派部署教程（51K Star）](https://www.cnblogs.com/labhub/p/22669565)
24. [sherpa-onnx GitHub（k2-fsa 官方）](https://github.com/k2-fsa/sherpa-onnx)
25. [sherpa-onnx README（含流式/非流式模型列表）](https://raw.githubusercontent.com/csukuangfj/sherpa-onnx/refs/heads/master/README.md)
26. [ESP32-P4 高性能 SoC 介绍](https://www.espressif.com/en/products/socs/esp32-p4)

### 云端 TTS

27. [阿里云 TTS 定价（同 ASR 文档）](https://www.alibabacloud.com/help/zh/isi/product-overview/pricing)
28. [Azure TTS v2 入门实战（CSDN）](https://devpress.csdn.net/avi/69935b980a2f6a37c5921b50.html)
29. [OpenAI TTS-1 / TTS-1-HD 模型文档](https://developers.openai.com/api/docs/models/tts-1-hd)
30. [OpenAI Edge TTS 中文使用方案](https://cloud.baidu.com/article/3901084)
31. [edge-tts GitHub（XiangCoder 免费方案）](https://github.com/XiangCoder/edge-tts)
32. [ElevenLabs Multilingual V2 中文 WaveSpeedAI 介绍](https://wavespeed.ai/blog/zh-CN/posts/introducing-elevenlabs-multilingual-v2-on-wavespeedai/)
33. [ElevenLabs Pricing 2026](https://texttolab.com/blog/elevenlabs-pricing)
34. [AI 配音 API/SaaS 横评（ElevenLabs vs Azure vs 国内方案）](https://gitcode.csdn.net/6a26b08110ee7a33f2796471.html)
35. [OpenAI TTS-1 HD 模型基准与价格（OrcaRouter）](https://www.orcarouter.ai/zh-CN/models/openai/tts-1-hd)

### 端侧 TTS

36. [piper-tts GitHub（rhasspy）](https://github.com/cdwiegand/piper-tts)
37. [Piper TTS 中文 huayan-medium 模型](https://huggingface.co/Trelis/piper-zh-cn-huayan-medium)
38. [Bert-VITS2-MNN（移动端 MNN）](https://github.com/Voine/Bert-VITS2-MNN)
39. [VITS-MNN（VITS 移动端）](https://github.com/Voine/VITS-MNN)
40. [ESP-TTS 多语言实现（CSDN 实战）](https://wenku.csdn.net/column/7of5xxo1k8)
41. [sherpa-onnx TTS 文档](https://k2-fsa.github.io/sherpa/onnx/tts/)

### 综合对比与社区博客

42. [实时语音转写大模型 API 对比（CSDN）](https://agent.csdn.net/6a17c4e310ee7a33f275e3e2.html)
43. [讯飞与腾讯云 Android 实时语音识别对比（CSDN）](https://shuaici.blog.csdn.net/article/details/142970169)
44. [Edge TTS 作为主 TTS 提供方 GitHub Issue](https://github.com/Olbrasoft/VirtualAssistant/issues/280)
45. [Whisper 评测 2026（chdh.me）](https://chdh.me/tools/ai-tools/video/whisper/)

---

## 附录：决策树速查

```
┌─ 需要英文/多语言为主？
│   ├─ 是 → Azure Cognitive Services（ASR+TTS）
│   └─ 否（中文为主）↓
│
├─ 需要极致音质 + 情感 + 实时对话？
│   ├─ 是 → 讯飞超拟人（ASR+TTS 双向流式）
│   └─ 否↓
│
├─ 是否对接豆包 LLM？
│   ├─ 是 → 火山引擎（豆包 ASR + 豆包 TTS）
│   └─ 否 → 阿里云 Paraformer ASR + Edge TTS / 讯飞 TTS
│
└─ 是否个人开发/零成本？
    ├─ 是 → Edge TTS + sherpa-onnx ASR（本地）
    └─ 否 → 按上面推荐
```
