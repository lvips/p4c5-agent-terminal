# W4 ASR Backend 设计

**日期**: 2026-09-07
**状态**: ✅ 后端架构完成 + E2E 链路打通
**PR**: `e918fdd` feat(tools): W4 ASR backend 集成 (Mock + 阿里云 NLS ISI)

---

## 一、目标

W3 验证 mic → mock_dsh → mock_asr (内嵌) → mock LLM 完整链路。
W4 把**内嵌 mock_asr 抽成可插拔的 backend**，让真实阿里云 NLS ISI (OMT 同款) 替换 mock 无需改代码。

## 二、架构

```
ESP32-P4 mic (4ch@24k)
  ↓ ADC + mixer + NLMS AEC + 24→16k resampler + libopus
  ↓ WS Binary (opus 16kbps, 20ms frames)
mock_dsh_server.py :8765
  ├─ OpusDecoder (libopus/opuslib or MockOpusDecoder)
  ├─ VADSession (RMS 80/50, 900ms silence, 20s hard cut)
  └─ ASREngine (环境变量驱动 backend)
        ├─ MockASREngine   (无 key fallback)
        └─ NLSASREngine    (阿里云 NLS ISI REST)
              ├─ TokenCache (HMAC-SHA1 POP v1, 5min 提前刷新)
              └─ call_isi_one_shot (PCM Int16 → POST)
  ↓ recognized_text
mock_dsh LLM (mock-claude, 18 帧对话流)
  ↓ WS JSON frames
ESP32 显示
```

## 三、Backend 选择

**完全由环境变量驱动**, 无需改代码:

```bash
# 真实阿里云 NLS ISI
export ALIYUN_ACCESS_KEY_ID=xxx
export ALIYUN_ACCESS_KEY_SECRET=xxx
export ALIYUN_NLS_APPKEY=xxx
python3 tools/mock_dsh_server.py --port 8765 --broadcast-text

# Mock (无 key)
python3 tools/mock_dsh_server.py --port 8765 --broadcast-text
```

启动日志会显示选了哪个:
```
12:18:38 [INFO] 🔑 检测到 ALIYUN_NLS 凭证 → 用 NLSASREngine (REST, appkey=xxx...)
12:18:38 [INFO] 🎭 无 ALIYUN_NLS 凭证 → 用 MockASREngine
```

## 四、阿里云 NLS ISI (1:1 OMT port)

**协议**: REST POST + POP API v1 HMAC-SHA1 签名
**Endpoint**: `https://nls-gateway.cn-shanghai.aliyuncs.com/stream/v1/asr`
**Token**:  `https://nls-meta.cn-shanghai.aliyuncs.com/`

完整 1:1 复刻 OMT tab5-adapter/asr.js:
- `special_url_encode()`: 阿里云 POP 签名特殊 URL 编码
- `compute_pop_signature()`: HMAC-SHA1, key=`{secret}&`, string_to_sign=`POST&/&{canonical}`
- `get_nls_token()`: POST 调 GetToken, 返回 `{id, expire_at}`
- `TokenCache`: 提前 5 分钟刷新 (避免临近过期失败)
- `call_isi_one_shot()`: POST 二进制 PCM (Int16 LE, 16kHz, mono) + `X-NLS-Token` header
- `VADSession`: rmsOfInt16 + RMS 80/50 + 900ms 静音 + 20s 硬截止 + silence_started_at 时间戳

**为什么 1:1 复刻 OMT 而不是用 dashscope？**
- 用户原话: "原来要求你先完成跟OMT一样的配置。因为OMT的配置中语音识别的部分, 阿里百炼的智能语音识别模型我有API"
- OMT 已经在生产环境跑通 NLS ISI (token + REST 流程验证过)
- 改 dashscope 是新协议, 风险大, 不在用户范围

## 五、VAD Session

完全复刻 OMT asr.js VAD 状态机:

```python
class VADSession:
    VAD_RMS_START = 80          # ~ -57 dBFS on 16-bit
    VAD_RMS_END = 50
    VAD_END_HOLD_MS = 900       # 900ms 静音后判定说话结束
    VAD_MAX_DURATION_MS = 20000 # 20s 硬截止
```

**3 状态**:
1. **idle**: 等待 speech start (RMS > 80)
2. **speaking**: 累积 PCM, 检测静音开始
3. **silence_hold**: 静音持续 900ms → finalize 调 ASR

**关键修复 (W3 bug)**: 原版用 `silence_started_at_frame` (frame count), 改用 `silence_started_at` (时间戳) — 与 OMT 严格 1:1。

## 六、E2E 实测 (USB-Serial-JTAG + MockASREngine)

```
12:37:16 [INFO] ✅ 新连接: ('192.168.1.248', 61726)
12:37:16 [INFO] 📋 Hello from p4c5-001 v0.1.0
12:37:23 [INFO] 🎙 speech start (RMS=8485, frame=0)
12:37:23 [INFO] 🎤 [vad] frames=1 samples=320 speech_started=True
12:37:28 [INFO] 🎤 [vad] frames=251 samples=80320 elapsed=4.9s
12:37:38 [INFO] 📊 接收帧数: 300 (9187 bytes)
12:37:39 [INFO] 📥 client/user_input "W2 测试: 验证 mock server 完整对话流"
12:37:39 [INFO] 📤 thinking → assistant_text → ... → assistant_done
```

✅ ESP32 mic 上行 → mock_dsh WS → MockOpusDecoder 解码 → VAD 检测 → MockASREngine → text → mock LLM → 完整 18 帧对话流。

## 七、已知限制 & 后续工作

### 7.1 audio_stop 不触发 VAD finalize

**现象**: ESP32 audio_stop 后, mock_dsh VAD 不会自动 finalize (因为 VAD.feed() 只在有新帧时调)。

**生产方案** (W5 待做):
- ESP32 在 audio_stop 时发 `audio/control` WS Binary 帧 `{action: "end"}`
- mock_dsh 收到后调 `vad_session._finalize()`
- 或者 ESP32 检测 silence 900ms 自动 end (需要 ESP32 端 VAD, 即 ESP-SR AFE)

### 7.2 MockOpusDecoder 不是真 mic 数据

**现象**: E2E 测试用 MockOpusDecoder (生成 1kHz sine wave, RMS=8485), 不是真 mic。

**修复**: 装 `opuslib` (`pip3 install opuslib`), 自动切到真 libopus 解码。或者用 ESP32 直接发 PCM (去掉 OPUS 编码), 更简单。

### 7.3 NLSASREngine 没真测

**原因**: 无阿里云 NLS AK/SK。

**用户操作**:
```bash
export ALIYUN_ACCESS_KEY_ID=...
export ALIYUN_ACCESS_KEY_SECRET=...
export ALIYUN_NLS_APPKEY=...
python3 tools/mock_dsh_server.py --port 8765 --broadcast-text
# 启动日志确认 NLSASREngine 选中
```

## 八、文件清单

| 文件 | 改动 |
|------|------|
| `tools/mock_dsh_server.py` | +149/-53: backend 集成, VAD 路径, env 驱动 |
| `tools/p4c5_asr_adapter.py` | 不变 (已实现 Mock/NLS/Opus/VAD/Token) |
| `hardware/p4c5-agent-terminal/main/app_main.cpp` | v10-v12 (REPL + audio_uplink enable_input + 32KB stack) |

## 九、对比 xiaozhi 架构

xiaozhi kevin-p4c5-4g 用 ESP-SR AFE 做 wake + AEC + VAD **全部在 ESP32 上**, 然后才把 cleaned PCM 走 WS Binary 上行 server。我们 W4 是把 VAD + ASR 放在 server 端。

**取舍**:
- xiaozhi 方案: ESP32 端压力大 (AFE ~50KB RAM), 但 server 简单, 延迟低
- 我们方案: ESP32 端轻 (只上行 OPUS), server 压力大 (要做 VAD + ASR + LLM), 延迟高

**未来**: 集成 ESP-SR AFE (Phase 2), VAD 移到 ESP32, 减少 server 负担 + 降低延迟。

## 十、Git

```
e918fdd feat(tools): W4 ASR backend 集成 (Mock + 阿里云 NLS ISI)
03520e1 fix(p4c5-agent-terminal): audio_uplink 启动前 enable ADC input + 32KB task stack
fcf299b feat(p4c5-agent-terminal): REPL 通过 USB-Serial-JTAG 工作
```

Pushed to `https://github.com/lvips/p4c5-agent-terminal.git` main branch.
