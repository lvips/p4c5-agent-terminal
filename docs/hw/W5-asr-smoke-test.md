# W5: ASR 真机测试指南 (麦克风 → 阿里云 NLS → ESP32 串口)

> **目标**: 验证"麦克风是否好用"的端到端冒烟测试
> **状态**: ✅ 完整链路已就绪
> **.严格复刻 OMT**: 用阿里云 NLS 一句话识别 (ISI)

---

## 🎯 测试目的

**在加 TTS 之前先验证上行链路完整通畅**:

```
ESP32 麦克风 → AEC → Opus 编码 → WS Binary 上行 → ASR Adapter
                                                            ↓
                                       阿里云 NLS 一句话识别 (REST)
                                                            ↓
ESP32 串口显示 ← echo assistant_text 帧 ← ASR Adapter 回传识别文字
```

**通过标准**:
- ✅ 麦克风真的拾取声音 (audio_diag mic RMS > 1000 说话时)
- ✅ AEC 工作 (ref/aec 比例合理)
- ✅ Opus 编码成功 (ASR Adapter 收到 binary msg)
- ✅ VAD 检测正常 (speech start → 静音 900ms → speech end)
- ✅ 阿里云 NLS 真的返回文字
- ✅ ESP32 串口能看到识别结果

---

## 📋 准备工作

### 1. 阿里云凭证 (用户已有)

```bash
export ALIYUN_ACCESS_KEY_ID=xxx
export ALIYUN_ACCESS_KEY_SECRET=xxx
export ALIYUN_NLS_APPKEY=xxx
```

**AppKey 获取**:
- 登录 https://nls-portal.console.aliyun.com/
- 创建项目 → 获取 AppKey (形如 `xxxxxxxxxxxx`)
- 开通"一句话识别"服务

### 2. 安装依赖

```bash
pip3 install websockets numpy opuslib requests
```

### 3. 启动 ASR Adapter (echo 冒烟模式)

```bash
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5

# 真实 ASR 模式 (使用你的阿里云凭证)
python3 tools/p4c5_asr_adapter.py \
    --no-upstream \
    --echo \
    --port 8766
```

**输出**:
```
🚀 P4C5 ASR Adapter 启动 (OMT 同等配置)
   监听: ws://0.0.0.0:8766
   ASR 引擎: NLSASREngine
   OPUS 解码: opuslib
   ⏭  upstream: 已禁用 (冒烟测试模式)
   📤 echo 模式: ON (识别文字会回 ESP32 串口)
```

**参数说明**:
- `--no-upstream`: 跳过 DSH 上行 (专注 ASR 验证, 不需要 mock_dsh)
- `--echo`: 识别后把文字作为 assistant_text 帧回 ESP32

### 4. 烧录 + 启动 ESP32 真机

```bash
cd hardware/p4c5-agent-terminal
. /Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5/export.sh
idf.py flash monitor
```

**串口命令**: 启动录音
```
audio start
```

---

## 🧪 测试流程

### 步骤 1: 验证麦克风

**串口观察 audio_diag 日志**:
```
I (12345) audio_diag: frame=50 mic=800 ref=0 aec=600 aec/mic=75%
I (12465) audio_diag: frame=100 mic=900 ref=0 aec=700 aec/mic=77%
```

**不说话时**:
- mic RMS < 200 (背景噪声)
- ref RMS = 0 (扬声器没播放)

**说话时**:
- mic RMS > 1500 (正常对话)
- 持续说话 mic RMS 应稳定在 1500-5000

✅ 通过标准: 说话时 mic RMS > 1500

---

### 步骤 2: 验证 AEC (如果有扬声器)

**播放 1kHz 测试音 (可选)**:

```bash
# 用 Mac 播放测试音, ESP32 通过 ref 通道回采
say "test"  # 或用任意音频
```

**期望日志**:
```
I audio_diag: frame=150 mic=2000 ref=4000 aec=500 aec/mic=25%
```

✅ 通过标准: ref >> mic (回采明显), aec << mic (回声被消除)
✅ aec/mic < 30% 表示 AEC 收敛

---

### 步骤 3: 验证 ASR (核心测试)

**对着麦克风说话**:
- 任意中文, 例如"今天天气怎么样"
- 持续说 1-2 秒
- 然后保持安静

**期望日志 (ASR Adapter 端)**:
```
INFO:p4c5-asr:✅ 新连接: ('192.168.1.248', 12345)
INFO:p4c5-asr:👋 hello: p4c5-001
INFO:p4c5-asr:📦 binary msg #50 len=45
INFO:p4c5-asr:🎙 speech start (RMS=3500, frame=0)
INFO:p4c5-asr:📦 binary msg #100 len=42
INFO:p4c5-asr:🤫 静音开始 (frame=85, RMS=200)
INFO:p4c5-asr:🤫 speech end (silence 920ms >= 900ms)
INFO:p4c5-asr:🔄 ASR 识别中: 48000 samples (3000ms)...
INFO:p4c5-asr:📝 识别结果: "今天天气怎么样" (3000ms)
INFO:p4c5-asr:📤 echo 文字给 ESP32 (clients=1)
```

**期望日志 (ESP32 串口)**:
```
I (13000) on_dsh_frame: assistant_text: "你说的是: 今天天气怎么样" (echo=true)
```

✅ 通过标准:
- ASR Adapter 日志显示"识别结果"包含你说的内容
- ESP32 串口显示"你说的是: [识别文字]"

---

### 步骤 4: 故障排查

#### ❌ ASR Adapter 没收到 binary msg

**原因**:
- ESP32 DSH client 未连接
- WS Binary 上行失败

**排查**:
1. ESP32 串口: 检查 `dsh_client_connected` 日志
2. Mac 端: `lsof -i :8766` 确认端口在听
3. 网络: ESP32 和 Mac 在同一 WiFi

#### ❌ audio_diag 显示 mic RMS = 0

**原因**:
- 麦克风没接 / 通道错
- ES7210 没初始化

**排查**:
1. 检查硬件连接
2. 烧录时确认 ES7210 驱动加载
3. 用示波器看 I2S 数据线

#### ❌ VAD 没触发 speech start

**原因**:
- 麦克风音量太小 (RMS < 80)
- 或者 mic 录到了静音

**排查**:
1. 看 audio_diag mic RMS 是否 > 1000
2. 把 VAD_RMS_START 调低 (80 → 50)
3. 大声说话或靠近麦克风

#### ❌ 阿里云 NLS 返回错误

**原因**:
- AppKey 错误
- accessKey 无效
- 没开通"一句话识别"服务

**排查**:
1. ASR Adapter 日志看 `status` 字段
2. 登录 https://nls-portal.console.aliyun.com/ 检查项目状态
3. 测试 AppKey 是否正确

#### ❌ ESP32 串口没显示 echo 文字

**原因**:
- echo 模式未开启
- ESP32 dsh_client 不处理 echo

**排查**:
1. ASR Adapter 启动参数包含 `--echo`
2. ESP32 端 `on_dsh_frame` 已处理 `assistant_text` (W2 已实现)
3. 串口日志级别 INFO (VLOG_D 不显示)

---

## 🧪 自动化测试 (无需 ESP32)

如果你还没拿到 ESP32, 可以跑自动化测试:

```bash
# 1. 启动 ASR Adapter (mock 模式)
python3 tools/p4c5_asr_adapter.py --mock --no-upstream --echo --port 8766

# 2. 跑冒烟测试
python3 tools/asr_smoke_test.py
```

**期望输出**:
```
✅ Echo 收到的文本: ['你说的是: [Mock-ASR #1] 识别到语音 (1480ms, RMS=5402)']
🎉 冒烟测试通过! 收到 1 条 echo
```

这是**逻辑链路验证**, 不验证麦克风硬件。真机硬件验证必须用 ESP32。

---

## 📝 验证清单

完成下列所有项表示 ASR 链路完整可用:

- [ ] Mac 端启动 ASR Adapter (echo 模式)
- [ ] ESP32 烧录 + 启动 audio start
- [ ] audio_diag: 说话时 mic RMS > 1500
- [ ] audio_diag: ref RMS 在扬声器播放时 > 1000 (如有)
- [ ] audio_diag: aec/mic < 30% (AEC 收敛)
- [ ] ASR Adapter 日志: 收到 binary msg
- [ ] ASR Adapter 日志: speech start/end 都触发
- [ ] ASR Adapter 日志: 阿里云 NLS 返回真实文字
- [ ] ESP32 串口: 显示 "你说的是: [识别文字]"
- [ ] 关闭 audio stop, ESP32 不再上行

---

## 🚀 下一步

ASR 链路验证通过后:
1. **去掉 `--no-upstream`** 启动 ASR Adapter, 同时启动 `mock_dsh_server.py`, 验证完整链路 (ASR → mock_dsh → mock LLM → ...)
2. **加 TTS Adapter**: 启动 `p4c5_tts_adapter.py`, 验证完整双向对话
3. **替换 Mock**: 用真实阿里云 API key 替代 mock

---

## 📂 相关文件

- `tools/p4c5_asr_adapter.py` - ASR Adapter (OMT 同等配置)
- `tools/asr_smoke_test.py` - 自动化冒烟测试
- `tools/requirements-asr.txt` - 依赖
- `tools/mock_dsh_server.py` - Mock DSH 服务端 (完整链路验证时用)
- `硬件串口命令`: `audio start` / `audio stop` / `audio status` / `tts stats`