# W5 真物理 E2E 首次完全跑通 🏆

**日期**: 2026-09-07
**状态**: ✅ 完整真物理链路验证通过
**Commits**:
- `f5d3f40` fix(asr): POP 签名 unpack bug + on_text callback 参数错误
- `4d4b159` feat(asr): 真阿里云 NLS ISI 凭证支持 + 真 OPUS 解码集成
- `f1e7895` feat(ui): 触摸 Hold to Talk 真物理反馈 + mic 启动

---

## 一、完整链路 (从用户说话到屏幕显示)

```
用户按下 Hold to Talk 按钮 (252, 555)
    ↓
ESP32 btn_talk_event_cb LV_EVENT_PRESSED
    ↓
ESP32 s_audio_recording = true
    ↓
ESP32 audio_uplink_task 启动
    ↓
ESP32 ES7210 ADC 4ch @ 24kHz (ch0+ch2=mic, ch1=AEC_ref)
    ↓
ESP32 mixer + NLMS AEC + 24→16k 重采样
    ↓
ESP32 libopus 16kbps 20ms 帧编码 → WS Binary
    ↓
mock_dsh :8765 接收 OPUS 帧
    ↓
mock_dsh OpusDecoder (libopus via opuslib + Homebrew)
    ↓
mock_dsh VADSession: RMS 80/50 阈值 + 900ms 静音检测
    ↓
真 VAD: speech start (RMS=336 / 133 / 98) + speech end (silence 905ms)
    ↓
mock_dsh NLSASREngine.recognize(pcm_int16)
    ↓
阿里云 NLS GetToken (POP API v1, HMAC-SHA1)
    ↓
阿里云 NLS ISI (REST POST) → 真识别文字
    ↓
mock_dsh on_text callback (lambda fix: text, meta)
    ↓
mock_dsh _handle_user_input → mock LLM
    ↓
mock_dsh 18 帧下行 (thinking → assistant_text → ... → assistant_done)
    ↓
ESP32 dsh_client WS 接收 → p4c5_ui_set_message()
    ↓
ESP32 屏幕显示 mock LLM 回复
```

## 二、真物理验证证据 (来自真实运行日志)

### 2.1 ESP32 触摸 (真物理)

```
I (3212411) p4c5_ui: 👆 [touch] PRESSED at (252, 555)
I (3212412) p4c5_ui: 👆 [TALK] PRESSED at (252, 555)
I (3212412) app_main: 🎙 [UI] Hold to Talk PRESSED → audio recording ON
I (3212419) audio_uplink: 🎙️ 启动上行 (ADC 4ch enabled)
I (3213580) audio_uplink: 已发送 500 帧 (opus 29 bytes/帧, AEC=ON, wake=MAN)
I (3214711) p4c5_ui: 👆 [touch] RELEASED
I (3214712) p4c5_ui: 👆 [TALK] RELEASED at (252, 555)
I (3214712) app_main: 🎙 [UI] Hold to Talk RELEASED → audio recording OFF
```

### 2.2 mock_dsh 真 OPUS 解码 + VAD (真物理)

```
15:50:03 [INFO] 🎙  speech start (RMS=336, frame=0)      ← 您说话
15:50:04 [INFO] 🤫 speech end (silence 905ms >= 900ms)    ← VAD 检测静音结束
15:50:07 [INFO] 🎙  speech start (RMS=133, frame=0)      ← 您又说话
15:51:34 [INFO] 🤫 speech end (silence 84413ms >= 900ms)  ← 长静音结束
15:51:36 [INFO] 🎙  speech start (RMS=98, frame=0)       ← 您再说话
```

### 2.3 阿里云 NLS ISI 真识别 (真物理 HTTP REST)

```
15:50:04 [INFO] 🔄 ASR 识别中: 17920 samples (1120ms)...
15:50:04 [INFO] 🔑 刷新阿里云 NLS token...
15:50:05 [INFO] ✅ token 有效期到 2026-09-09 03:50:05      ← 真 token
15:50:05 [INFO] 📝 ASR 识别: "不。" (1120ms, 56 frames)     ← 您说 "不"
15:51:34 [INFO] 🔄 ASR 识别中: 46080 samples (2880ms)...
15:51:34 [INFO] 📝 ASR 识别: "你好啊你好啊。" (2880ms, 144 frames)  ← 您说 "你好啊你好啊"
```

### 2.4 mock LLM 完整 18 帧对话流 (真物理)

```
15:50:06 [INFO]   💬 User input: 不。                        ← on_text fix 生效
15:51:35 [INFO]   💬 User input: 你好啊你好啊。              ← 真触发 LLM
[mock_dsh 发送: thinking → assistant_text × N → assistant_done]
```

### 2.5 ESP32 屏幕显示 (真物理)

```
收到您的消息: "不。"。这是 Mock DSH 的自动回复。
收到您的消息: "你好啊你好啊。"。这是 Mock DSH 的自动回复。
```

## 三、本次踩的 3 个坑 (诚实地记录)

### 3.1 bug 1: `MockOpusDecoder(SAMPLE_RATE, CHANNELS)` 传错参数

`OpusDecoder` 是无参构造, 内部已固定 SAMPLE_RATE/CHANNELS。
**错误调用** → `OpusDecoder(SAMPLE_RATE, CHANNELS)` → TypeError → catch → fallback MockOpusDecoder
**修复**: `OpusDecoder()`
**影响**: 之前所有 "OPUS 解码器: Mock" 的 log, 实际是 fallback, 不是真 libopus
**commit**: `4d4b159`

### 3.2 bug 2: `for k, v in keys` (POP 签名 unpack)

`build_canonicalized_query`:
```python
keys = sorted(params.keys())  # keys 是 strings 列表
pairs = [... for k, v in keys]  # 错! keys 是 strings 不是 tuples
```
**错误**: `ValueError: too many values to unpack (expected 2)`
**修复**: 
```python
return "&".join(
    f"{special_url_encode(k)}={special_url_encode(str(params[k]))}"
    for k in sorted(params.keys())
)
```
**影响**: 阿里云 GetToken POP 签名失败, NLSASREngine.recognize() 在 NLS 调用时崩
**commit**: `f5d3f40`

### 3.3 bug 3: `on_text` lambda 只接 1 个参数

VADSession 调 `self.on_text(text, meta)` 传 2 个参数, 但是 lambda 只接 1 个:
```python
on_text=lambda text: self._on_asr_text(ws, session, text)  # 只接 text
```
**错误**: `lambda() takes 1 positional argument but 2 were given`
**修复**:
```python
on_text=lambda text, meta: self._on_asr_text(ws, session, text)  # 接 2 个
```
**影响**: on_text callback 失败, user_text 没传到 mock LLM, 不触发完整 18 帧
**commit**: `f5d3f40`

## 四、关键工程经验 (教训)

1. **"测试通过"必须真物理验证**: 之前所有 "通过" 都是逻辑链路通过, 物理只是 ESP32 真发数据 + mock_dsh 真收数据. 真物理 = mic 录音内容 + OPUS 解码结果 + VAD 检测特征 + ASR 识别文字.
2. **诚实记录 bug**: 即使自己代码里写了 `# TODO`, 也要承认, 不能模糊.
3. **阿里云 POP 签名严格按 RFC**: `keys` 是 list 不是 tuple 列表, 必须用 `params.items()` 或 `sorted(params.keys())`.
4. **回调签名要严格匹配**: `self.on_text(text, meta)` 必须有接收 2 参数的 callback.
5. **monkey-patch 必须在 import 之前**: libopus 在 `/opt/homebrew/lib`, ctypes.util.find_library 找不到, 必须 monkey-patch.

## 五、文件清单

| 文件 | 改动 |
|------|------|
| `tools/mock_dsh_server.py` | +libopus monkey-patch + ISI_* 凭证兼容 + OpusDecoder() 无参 + on_text 2 参数 |
| `tools/p4c5_asr_adapter.py` | +get_aliyun_credentials() 兼容 ALIYUN_*/ISI_* + build_canonicalized_query 修复 |
| `hardware/p4c5-agent-terminal/components/p4c5_ui/p4c5_ui.c` | btn_talk_event_cb 真物理反馈 + status 颜色 |
| `hardware/p4c5-agent-terminal/main/app_main.cpp` | extern "C" p4c5_ui_btn_talk_press/release |

## 六、对比 xiaozhi 架构

| 特性 | xiaozhi kevin-p4c5-4g | p4c5 (我们) |
|---|---|---|
| ASR engine | ESP-SR AFE (客户端) + server side | mock_dsh 真阿里云 NLS ISI (server) |
| Wake word | WakeNet9 (客户端) | 自实现 RMS wake (客户端) |
| AEC | ESP-SR AFE | 自实现 NLMS POC |
| VAD | ESP-SR AFE | OMT 复刻 RMS 80/50 |
| OPUS 解码 | 服务器端 (libopus) | 服务器端 (libopus + opuslib) |
| 触摸 UI | LVGL 9 | LVGL 9 (同款) |

## 七、后续 (Phase 1-4)

- **Phase 1**: ESP-SR AFE 替换自实现 AEC + VAD + Wake (xiaozhi kevin-p4c5-4g 已验证在 P4 上跑通)
- **Phase 2**: WakeNet9 集成 (xiaozhi 同款)
- **Phase 3**: 阿里百炼 CosyVoice TTS 下行 (您有 API)
- **Phase 4**: 应用状态机 + 多模态

## 八、Git

```
f5d3f40 fix(asr): POP 签名 unpack bug + on_text callback 参数错误
4d4b159 feat(asr): 真阿里云 NLS ISI 凭证支持 + 真 OPUS 解码集成
f1e7895 feat(ui): 触摸 Hold to Talk 真物理反馈 + mic 启动
73368f8 docs(hw): W4 ASR backend 设计文档 (Mock + 阿里云 NLS ISI)
e918fdd feat(tools): W4 ASR backend 集成 (Mock + 阿里云 NLS ISI)
```

Pushed to `https://github.com/lvips/p4c5-agent-terminal.git` main branch.
