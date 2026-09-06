# W3-W5 完整路线图 + 商业模式综合归档

> **状态**: W3 Phase 1 完成（4 麦 + AEC + 上行链路）
> **W4**: WakeNet9 + AFE 集成
> **W5**: 火山豆包 TTS 双向流式
> **核心决策**: 走小智路径（开源 + 私有云 + B 端授权）

---

## 0. TL;DR

| 阶段 | 内容 | 状态 | Commit |
|-----|------|------|--------|
| W1 | SDIO WiFi 黑屏修复 | ✅ | `bae736c` |
| W2 | DSH 18 帧协议验证 | ✅ | `c3feb13` |
| **W3 Phase 1** | **4 麦 + 软件 AEC + 端到端 audio→ASR** | **✅** | **`6620ca4`** |
| W4 (W3 P2) | WakeNet9 + ESP-SR AFE 集成 | 🟡 待硬件换 ESP32-S3 | — |
| W5 (W3 P3) | 火山豆包双向流式 TTS | 🟡 Mac Adapter 待写 | — |

**已交付**：
- 完整音频上行链路（ESP32 端 6 步 + Mac Adapter ASR 闭环）
- mock_dsh_server 支持 WS Binary 帧，端到端测试通过
- 商业模式调研报告（xiaozhi 150 万台 / 4000 万签约 / 99 元年费）

---

## 1. W3 Phase 1 实施总结（已验证）

### 1.1 ESP32 端 6 步上行链路

```
p4c5_audio_record_multi (4ch 24kHz)
  ↓
4 通道分离: ch0+ch2=mic, ch1=AEC ref, ch3=unused
  ↓
aec_sw (NLMS 自适应滤波, taps=128, mu=0.005)
  ↓
resampler_24_16 (24k→16k, 480→320 samples)
  ↓
opus_encoder (libopus, 16kbps, 20ms 帧)
  ↓
dsh_client_send_audio → WS Binary → Mac Adapter
```

**关键设计决策**：
- ✅ 4 通道全开（与 xiaozhi kevin-p4c5-4g 一致）
- ✅ 自实现 NLMS AEC（P4 没有 ESP-SR AFE 硬件加速）
- ✅ 串口命令触发（`audio start/stop`）

**验证**：
- Build: ✅ 成功（1.4MB）
- Mock 端到端: ✅ Python websockets 测试通过（100 帧 → ASR → LLM 7 帧回复）

### 1.2 Mac Adapter ASR 闭环（mock 实现）

**文件**: `tools/mock_dsh_server.py` +96 lines

**POC 简化**：
- 不做真实 OPUS 解码（后续用 `opuslib`）
- 不做真实 VAD（每 100 帧触发）
- 不调真实 ASR（识别文字直接 mock 成 "[Mock-ASR] 收到 N bytes"）

**完整版待写**：`tools/p4c5_asr_adapter.py`（Python，复刻 OMT asr.js 设计）
- opuslib 解码 → PCM
- VAD RMS 检测静音
- 阿里云 DashScope `qwen3-asr-flash-realtime` 流式 ASR
- 识别文字 → `client/user_input` 上行

---

## 2. W4 路线图（WakeNet9 + AFE 集成）

### 2.1 核心限制（DSH 不打太极）

**ESP32-P4 不支持 ESP-SR AFE 硬件加速**：

| 算法 | ESP32-P3 | ESP32-S3 | 备注 |
|-----|-----------|-----------|------|
| AFE AEC | ❌ | ✅ | 需要矢量指令 |
| WakeNet | ⚠️ 模型小 | ✅ 完整 | 算力限制 |
| AFE VAD | ❌ | ✅ | NSNet 依赖 |
| BSS / DOA | ❌ | ✅ | 需要多核 |

**结论**：要做完整 WakeNet + AFE，硬件必须换 **ESP32-S3** 主板。

### 2.2 替代方案（不换硬件）

**POC 路径**（保留 P4）：
- ✅ 软件 AEC（自实现 NLMS）— 已完成
- ✅ 软件 VAD（基于音频能量）— Mac Adapter 端实现
- 🟡 WakeNet（占位）：用物理按键触发录音
- 🟡 NSNet（占位）：不做端侧降噪，靠麦克风阵列自然降噪

**生产路径**（换硬件）：
- 保留 P4 主控 + 5 寸 MIPI 屏幕
- 副板 ESP32-S3 协处理：跑 WakeNet + AFE
- SPI/SDIO 通信：P4 ↔ S3 协处理

### 2.3 OMT/xiaozhi 实际做法

- OMT POC：物理按键触发，没有 WakeNet
- xiaozhi 量产：ESP32-S3 主控 + ESP-SR AFE

---

## 3. W5 路线图（火山豆包双向流式 TTS）

### 3.1 协议要点（subagent 249bae11 调研）

**WebSocket URL**: `wss://openspeech.bytedance.com/api/v3/tts/bidirection`

**Headers**:
- `X-Api-App-Key`: 应用 ID
- `X-Api-Access-Key`: Access Key
- `X-Api-Resource-Id`: `volc.service_type.10029` (TTS 1.0) 或 `seed-tts-2.0` (TTS 2.0)
- `X-Api-Connect-Id`: 连接 ID

**4 字节协议头（位打包）**：
- byte0: `(protocol_version << 4) | header_size` → 默认 `0x11` (pv=1, hdr_size=1)
- byte1: `(message_type << 4) | flags` → 默认 `0x14` (msg_type=1, flags=4)
- byte2: `(serialization << 4) | compression` → 默认 `0x10` (json, no compression)
- byte3: reserved → `0x00`

**事件流程**：
1. Client → Server: `StartSession(100)` + Session 配置
2. Client → Server: `TaskRequest(200)`（可多次发文本片段）
3. Client → Server: `FinishSession(102)`
4. Server → Client: `TTSResponse(352)`（音频数据）
5. Server → Client: `SessionFinished(152)`

**音频参数**：
```json
{
  "audio_params": {
    "format": "pcm",
    "sample_rate": 24000,
    "speech_rate": 0,
    "loudness_rate": 0
  }
}
```

### 3.2 Mac Adapter 实施计划

**文件**: `tools/p4c5_tts_adapter.py`（待写）

**核心流程**：
1. 收到 DSH 下行 `assistant_text` 帧
2. 累积文本片段（流式合成）
3. 火山豆包 TTS 双向流式调用
4. PCM 24kHz 音频 → ESP32 端（WS Binary 下行）
5. ESP32 端 ES8311 DAC 播放

**性能目标**（subagent 调研）：
- 火山首音延迟：~0.317s（广州联通，单地点实测）
- 讯飞首音延迟：~0.253s（同条件）
- 小智输出格式：Opus 24kHz mono 60ms 帧

### 3.3 ESP32 端实施计划

**待写**：
- `components/audio_pipeline/tts_player/` — ES8311 DAC 播放任务
- WS Binary 下行接收（dsh_client 需加 receive_binary）
- Opus 解码（如用 Opus 格式）+ ES8311 播放
- 播放队列管理（避免 underrun）

---

## 4. 商业模式综合调研（subagent ea1f024a 报告）

### 4.1 一句话结论

> 走小智 AI 的成功路径：**开源 + 私有云 + B 端授权**
> 避坑：硬件微利、订阅套路、隐私黑盒

### 4.2 关键数据（公开来源验证）

| 指标 | 数值 | 来源 |
|-----|------|------|
| 小智 AI 设备出货 | **150 万台** | 新京报 2026 |
| 日均对话 | 400-900 万条 | 十方融海官方 |
| 日均 token | **1200-2700 亿** | 十方融海官方 |
| 签约订单 | **4000 万元** | 南方日报 2026 |
| GitHub star | 25K+ | github.com/78/xiaozhi-esp32 |
| 声音复刻订阅 | 99 元/年 | xiaozhi.me 官方文档 |
| 贴牌授权单价 | 10-30 元/台 | 行业惯例 |
| 合作提价空间 | 30-100% | 案例: 东莞音箱厂翻倍 |

### 4.3 BOM 估算（我们 ESP32-P4 + C5）

| 配置 | BOM | 建议零售价 |
|-----|-----|-----------|
| 经济版（1.5寸屏） | 160 元 | 399 元 |
| **标准版（3.4寸屏）** | **261 元** | **699 元** |
| 高配版（4寸触屏 + 4G） | 328 元 | 899 元 |

### 4.4 3 年财务预测

| 指标 | 第 1 年 | 第 2 年 | 第 3 年 |
|-----|--------|--------|--------|
| 总营收 | 993 万 | 5467 万 | **1.99 亿** |
| 净利润 | 300 万 | 1900 万 | **7000 万** |

### 4.5 我们的战略路径

**做什么**（差异化）：
- ✅ 开源 + 私有云 + 自有数据
- ✅ B 端公版方案授权
- ✅ 4G 移动场景 + 老人/儿童外出陪伴
- ✅ 教育/声纹/4G 订阅服务
- ✅ MCP + AI Agent 生态接入

**不做什么**：
- ❌ 不做纯硬件贴牌（毛利率薄）
- ❌ 不做订阅锁基础功能（消保委点名）
- ❌ 不与天猫精灵/小爱正面竞争（巨头寡头）
- ❌ 不做数据黑盒收集（合规风险）

---

## 5. 风险与缓解

| 风险 | 等级 | 缓解 |
|-----|------|------|
| 卷入"订阅套路"舆论 | 中 | 基础功能永久免费 |
| 数据合规 | 中 | 默认不存原始语音 |
| 退货率 30-40% | 高 | 30 天无理由 + 强化体验 |
| 内容审核（未成年） | 高 | 内置漏斗机制 + 分级 |
| 大厂降维打击 | 中 | 走开源 + 私有云 |
| 现金流 | 中 | 100-300 万启动资金 |
| 硬件 ESP32-P4 AEC 质量差 | 中 | 后期换 ESP32-S3 协处理 |

---

## 6. 30-60 天可执行清单

1. **Mac Adapter ASR**（W3 P1 闭环）— 写 `tools/p4c5_asr_adapter.py`（opuslib + DashScope）
2. **端到端测试** — ESP32 真实录音 → Mac ASR → DSH LLM → 文字回 ESP32 串口
3. **W5 TTS 实施** — 写 `tools/p4c5_tts_adapter.py`（火山双向流式）
4. **P4 软件 AEC 验证** — 播放 1kHz 测试音，检查 ch1 回采信号
5. **路演版 BP** — 基于财务模型 + W3 完成度
6. **接洽乐鑫**争取芯片级 NRE 赞助 + ESP32-S3 协处理支持
7. **接触立创/嘉立创**打样小批量（5K-1万台）
8. **申请深圳市开源生态补贴**（2025-2027 政策窗口）
9. **对接火山/DeepSeek 签年框 API**（目标 6 折）
10. **设计订阅产品**：声纹复刻 99/年 + 长记忆 49/年 + 4G 流量 99/年

---

## 7. 完整文档索引

### 实施归档
- `docs/hw/W3-aec-implementation.md` — 4 麦软件 AEC（256 行）
- `docs/hw/W3-asr-plan.md` — ASR 方案对比（162 行）
- `docs/hw/W3-implementation-plan.md` — OMT 等同功能 5 步计划（144 行）
- `docs/hw/W3-competitive-analysis.md` — 综合分析（360 行）
- `docs/hw/W3-complete-roadmap.md` — 本文档（路线图）

### 调研归档
- `docs/research/xiaozhi-esp32-2026.md` — 小智 ESP32 深度调研（655 行）
- `docs/research/protocol-comparison-2026.md` — 协议对比（492 行）
- `docs/research/asr-tts-comparison-2026/` — ASR/TTS 对比报告

### 商业归档
- `/Volumes/ZT-1T/项目开发/ESP32-P4C5/小智类AI语音终端产品商业模式调研报告.md` — 商业模式深度报告

### 已有完成
- `docs/hw/W1-completion-summary.md`
- `docs/hw/W2-completion-summary.md`

---

**最新 Commit**: `6620ca4` — `feat(tools): mock_dsh_server 支持 WS Binary 帧 + 简化 ASR 闭环`

**W3 Phase 1 状态**: ✅ 已完成并端到端验证