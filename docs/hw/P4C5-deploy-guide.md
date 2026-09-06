# P4C5 完整语音功能部署指南 (W1-W4 汇总)

> **适用版本**: W1 + W2 + W3 + W4 (commit `3d7c6d3`)
> **硬件**: ESP32-P4C5 + ESP32-C5 SDIO WiFi 子板
> **日期**: 2026

---

## 0. TL;DR

5 分钟快速启动：

```bash
# 1. 烧录 ESP32 固件
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5/hardware/p4c5-agent-terminal
. /Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5/export.sh
idf.py flash monitor

# 2. 启动 Mac 工具链 (3 个进程)
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5
python3 tools/mock_dsh_server.py --port 8765 --broadcast-text &
python3 tools/p4c5_asr_adapter.py --mock --port 8766 --upstream ws://127.0.0.1:8765/ws &
python3 tools/p4c5_tts_adapter.py --mock --port 8767 &

# 3. 串口验证
python3 tools/esp32_test.py --port /dev/cu.usbserial-1410
```

---

## 1. 硬件连接

### 1.1 必需硬件

| 组件 | 型号 | 备注 |
|------|------|------|
| 主控 | ESP32-P4 | RISCV, 360MHz, 512KB SRAM |
| WiFi | ESP32-C5 子板 | SDIO 接口, W1 已修黑屏 |
| ADC | ES7210 | 4 通道 TDM |
| DAC | ES8311 | 1 通道 mono 播放 |
| 麦克风 | 4x MEMS | 间距 5cm, 全向 |
| 扬声器 | 4Ω 3W | ES8311 直接驱动 |

### 1.2 AEC 参考信号

**关键**: 软件 AEC 需要 ch1 (MIC2) 接到扬声器回采。

```
ES8311 OUT ──(耦合)── ES7210 MIC2 (ch1)
```

如果不接，AEC 仍然跑但效果差（aec ≈ mic）。

---

## 2. 软件架构总览

### 2.1 ESP32 端组件树

```
app_main.cpp
├── dsh_client              (W2: 18 帧 DSH 协议, WS Binary 支持)
├── audio_uplink_task       (W3: 6 步上行 pipeline)
│   ├── p4c5_audio          (4ch@24kHz 录音)
│   ├── audio_mixer         (4ch→1ch 混音)
│   ├── aec_sw              (NLMS 软件 AEC, 128 taps)
│   ├── resampler_24_16     (24k→16k 重采样)
│   ├── opus_encoder        (libopus 16kbps/20ms)
│   └── wake_word_detector  (W4: RMS-based 唤醒)
├── tts_player              (W3: WS Binary PCM → ES8311 DAC)
└── 其他 UI/系统组件
```

### 2.2 Mac 端工具链

```
mock_dsh_server (8765)
  ├─ 18 帧 DSH 协议
  ├─ mock LLM (自动回复)
  └─ --broadcast-text (供 TTS Adapter 订阅)

p4c5_asr_adapter (8766)
  ├─ opuslib OPUS 解码
  ├─ VAD (RMS 能量 + 静音超时)
  ├─ Mock ASR / DashScope qwen3-asr-flash-realtime
  └─ 上行 user_input

p4c5_tts_adapter (8767)
  ├─ Mock TTS / 火山豆包双向流式 TTS
  ├─ 监听 DSH 下行 assistant_text
  └─ WS Binary PCM 24kHz 下行
```

---

## 3. 串口命令参考

| 命令 | 功能 | 示例 |
|------|------|------|
| `audio start` | 启动录音 + 上行 | `audio start` |
| `audio stop` | 停止录音 | `audio stop` |
| `audio status` | 显示 audio + wake 状态 | `audio status` |
| `wake enable` | 启动 RMS 唤醒检测 | `wake enable` |
| `wake disable` | 关闭唤醒检测 | `wake disable` |
| `wake status` | 显示 wake 状态 + 统计 | `wake status` |
| `tts stats` | 显示 TTS player 统计 | `tts stats` |

### 3.1 关键日志

#### `audio_diag` (W3: AEC 实时诊断)

```
I (12345) audio_diag: frame=50 mic=1500 ref=8000 aec=400 aec/mic=26%
```

| 字段 | 含义 | 期望 |
|------|------|------|
| mic | 4 麦混音 RMS | 说话时 > 1500 |
| ref | AEC 参考 (扬声器回采) RMS | 播放时 > 1500 |
| aec | AEC 处理后 RMS | 远小于 mic (回声被消除) |
| aec/mic | 残余回声比例 | < 30% 良好, > 70% 需调参 |

#### `wake_det` (W4: 唤醒检测)

```
I (12345) wake_det: 🌟 WAKE detected! (RMS=5000, events=1)
I (13000) wake_det: 💤 SLEEP detected (RMS=0, events=1)
```

---

## 4. 验证清单

### 4.1 第一次启动

- [ ] ESP32 烧录成功 (`Project build complete`)
- [ ] 串口显示 `All subsystems initialized`
- [ ] WiFi 连接成功 (SDIO 子板)
- [ ] DSH 连接成功 (`dsh_client_connected`)
- [ ] ASR Adapter / TTS Adapter / mock_dsh 三个进程启动

### 4.2 录音功能 (W3)

```bash
# 1. 串口命令
audio start

# 2. 期望日志
I audio_uplink: 已发送 250 帧 (opus 30 bytes/帧, AEC=ON, wake=MAN)
I audio_diag: frame=50 mic=800 ref=0 aec=600
I audio_diag: frame=100 mic=900 ref=0 aec=700
...
```

- [ ] `audio_diag` 日志每 1 秒出现
- [ ] mic RMS 在说话时 > 1500
- [ ] ref RMS 在播放扬声器时 > 1500 (需硬件连接)
- [ ] aec RMS < mic × 30% (AEC 收敛)

### 4.3 唤醒检测 (W4)

```bash
# 1. 启动唤醒
wake enable

# 2. 验证静默无误唤醒 (等 5 秒应无 wake event)
I wake_det: ⏳ 等待语音...
I wake_det: ⏳ 等待语音...

# 3. 大声说话 → 应触发 WAKE
I wake_det: 🌟 WAKE detected! (RMS=5000, events=1)

# 4. 静音 2 秒 → 应触发 SLEEP
I wake_det: 💤 SLEEP detected (RMS=0, events=1)

# 5. 查看统计
wake status
I wake_det: 📊 [stats] frames=300 wake=1 sleep=1 active=0

# 6. 关闭
wake disable
```

### 4.4 TTS 下行播放 (W3)

```bash
# 1. TTS Adapter 已启动 (8767 listening)

# 2. 触发 user_input (通过 ASR Adapter 或 mock_dsh)
# 期望看到 tts_player_feed_pcm 被调用

# 3. 查看统计
tts stats
I tts_player: 📊 [stats] received=51 played=51 overflow=0 queue=0/8
```

### 4.5 端到端语音对话 (完整链路)

```bash
# 1. 确保所有进程在运行
pgrep -f mock_dsh_server.py      # mock_dsh
pgrep -f p4c5_asr_adapter.py      # asr
pgrep -f p4c5_tts_adapter.py      # tts

# 2. ESP32 录音模式
audio start

# 3. 说话 (对着麦克风) → 释放 → 等待

# 4. 期望完整链路:
#    ESP32 OPUS 上行 → ASR Adapter VAD finalize
#    → mock_dsh user_input → mock LLM
#    → assistant_text (broadcast) → TTS Adapter
#    → 合成 PCM → WS Binary 下行 → tts_player
#    → ES8311 DAC → 扬声器播放

# 5. 验证 (串口日志)
I audio_uplink: 已发送 250 帧 ...
I tts_player: 已播放 100 帧 (累计)
```

---

## 5. 端到端测试脚本

### 5.1 Mac 端集成测试 (无需 ESP32)

```bash
# 单连接测试 (验证 mock_dsh + LLM)
python3 tools/integration_test.py --mode single

# 双连接测试 (验证 ASR Adapter)
python3 tools/integration_test.py --mode dual

# 4 组件测试 (完整链路)
python3 tools/integration_test_v2.py
```

### 5.2 Wake Detector 单元测试

```bash
python3 tools/wake_detector_test.py
# 期望: 5/5 通过
```

### 5.3 ESP32 真机测试

```bash
# 安装依赖
pip3 install pyserial

# 连接 ESP32 (Mac: /dev/cu.usbserial-*, Linux: /dev/ttyUSB*)
python3 tools/esp32_test.py --port /dev/cu.usbserial-1410
```

---

## 6. 故障排查

### 6.1 ESP32 启动问题

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| 黑屏不启动 | 电源问题 | 检查 USB-C 供电 (>5V/2A) |
| 反复重启 | 看门狗 / OOM | 减小 `audio_uplink_task` 栈 (16KB → 8KB) |
| WiFi 连接失败 | C5 子板没插好 | 检查 SDIO 接口, 重插拔 |

### 6.2 录音问题

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| mic RMS 一直 = 0 | 麦克风没接 / 通道错 | 检查 ES7210 I2S 数据, 用示波器 |
| mic RMS 一直 = 32768 | 削波 / 增益过大 | 调小 `p4c5_audio` 增益 |
| ref RMS = 0 | AEC 参考没接 | 检查 ES7210 MIC2 与 ES8311 OUT 耦合 |
| aec ≈ mic | AEC 不收敛 | 调大 `aec_sw` 的 filter_taps (128 → 256) |

### 6.3 上行问题

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| OPUS 帧无上行 | DSH 未连接 | 检查 `dsh_client_connected` 日志 |
| dsh_client_send_audio 返回 ESP_ERR_INVALID_STATE | DSH 断连 | 重启 `dsh_client_connect()` |
| ASR Adapter 收不到 binary | 端口错 / firewall | `lsof -i :8766` 检查 |

### 6.4 唤醒问题

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| 完全不唤醒 | mic 没声音 | 检查 mic RMS (audio_diag) |
| 误唤醒严重 | 阈值过低 / 环境噪声 | 调高 `wake_rms_threshold` (1500 → 3000) |
| 唤醒但不上行 | wake_events 触发了但 ASR 不通 | 看 `is_uploading` 状态 |
| 唤醒后不睡眠 | 阈值过低 / sleep_hold 太短 | 调高 `sleep_rms_threshold` 或增大 `sleep_hold_frames` |

### 6.5 TTS 下行问题

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| TTS Adapter 不收 PCM | ESP32 没连 8767 | `lsof -i :8767` 检查 |
| 收到但无声 | ES8311 DAC 没初始化 | 检查 `p4c5_audio_enable_output(true)` |
| 帧长 2880 不匹配 | 火山 TTS 帧大小不同 | TTS Adapter 加缓冲对齐 |

---

## 7. 性能调优

### 7.1 AEC 参数

`main/app_main.cpp` 中 `aec_cfg`：

```cpp
aec_cfg.filter_taps = 128;      // 5.3ms @ 24kHz, 可调 64-256
aec_cfg.step_size   = 0.005f;   // 学习率, 0.001-0.01
aec_cfg.leakage     = 0.999f;   // 泄漏因子, 0.99-1.0
```

**调优建议**:
- 收敛慢 → 增大 `step_size` (0.005 → 0.01)
- 不稳定 → 减小 `step_size` (0.005 → 0.001)
- 长回声路径 → 增大 `filter_taps` (128 → 256)

### 7.2 Opus 编码

`audio_pipeline/opus_encoder/opus_encoder.cpp`：

```cpp
opus_cfg.bitrate = 16000;       // 16kbps, 可调 8000-32000
opus_cfg.complexity = 5;        // 0-10, 越高压缩越好 CPU 越大
opus_cfg.use_vbr = true;        // VBR 比 CBR 节省 20% 带宽
```

**调优建议**:
- 带宽紧张 → 8kbps (但 ASR 识别率下降)
- 高质量优先 → 32kbps
- CPU 紧张 → complexity = 3

### 7.3 Wake 阈值

```cpp
wake_cfg.wake_rms_threshold  = 1500;  // 调高 = 减少误唤醒
wake_cfg.sleep_rms_threshold = 500;   // 调低 = 更快进入睡眠
wake_cfg.wake_hold_frames    = 5;     // 调大 = 需要更长持续时间
wake_cfg.sleep_hold_frames   = 100;   // 调大 = 等待更久才停止录音
```

---

## 8. 升级路线

### 8.1 W5: 火山 TTS 真实接入

```bash
# 1. 申请火山豆包 App Key
#    https://www.volcengine.com/product/voice-tech

# 2. 设置环境变量
export VOLC_APP_KEY=xxx
export VOLC_ACCESS_KEY=xxx
export VOLC_RESOURCE_ID=volc.service_type.10029

# 3. 启动 (非 mock)
python3 tools/p4c5_tts_adapter.py --port 8767
# 自动使用 VolcTTSEngine 而非 Mock
```

### 8.2 真实 ASR (DashScope)

```bash
# 1. 申请阿里云百炼 API key
#    https://bailian.console.aliyun.com/

# 2. 设置环境变量
export DASHSCOPE_API_KEY=sk-xxx

# 3. 启动 (非 mock)
python3 tools/p4c5_asr_adapter.py --port 8766 --upstream ws://127.0.0.1:8765/ws
# 自动使用 DashScopeASREngine
```

### 8.3 W4 真实 WakeNet (需 ESP32-S3 协处理)

1. 购买 ESP32-S3 协处理板 (SDIO/UART 接口)
2. 移植 ESP-SR WakeNet9 (~300KB 模型)
3. P4 通过串口发送 raw PCM → S3 推理 → 返回 wake event
4. 用 speexdsp AEC 替代 NLMS

---

## 9. 文件索引

### 9.1 源码

```
hardware/p4c5-agent-terminal/
├── main/
│   ├── app_main.cpp                   (音频任务 + DSH 集成)
│   ├── CMakeLists.txt
│   └── idf_component.yml
└── components/
    ├── audio_pipeline/
    │   ├── audio_mixer/               (W3: 4ch→1ch)
    │   ├── aec_sw/                    (W3: NLMS 软件 AEC)
    │   ├── resampler_24_16/           (W3: 24k→16k)
    │   ├── opus_encoder/              (W3: libopus)
    │   ├── tts_player/                (W3: WS Binary → DAC)
    │   └── wake_word_detector/        (W4: RMS 唤醒)
    ├── dsh_client/                    (W2: 18 帧 + WS Binary)
    └── p4c5_audio/                    (硬件驱动)
```

### 9.2 Mac 工具

```
tools/
├── mock_dsh_server.py                 (DSH mock + broadcast)
├── p4c5_asr_adapter.py                (VAD + ASR)
├── p4c5_tts_adapter.py                (TTS + PCM 下行)
├── integration_test.py                (单/双连接测试)
├── integration_test_v2.py             (4 组件测试)
├── wake_detector_test.py              (Wake 单元测试)
├── esp32_test.py                      (ESP32 真机测试)
└── requirements-asr.txt
```

### 9.3 文档

```
docs/hw/
├── W1-completion-summary.md           (SDIO WiFi 修复)
├── W2-completion-summary.md           (DSH 协议验证)
├── W3-aec-implementation.md           (W3: 4 麦 AEC)
├── W3-asr-plan.md
├── W3-implementation-plan.md
├── W3-competitive-analysis.md
├── W3-complete-roadmap.md             (W3-W5 路线图)
├── W3-completion-summary.md           (W3 总结)
├── P4C5-deploy-guide.md               (本文档)
└── ...
```

---

## 10. 联系与支持

- 项目仓库: https://github.com/lvips/p4c5-agent-terminal
- 提交 issue: GitHub Issues
- 完整文档: `docs/hw/` 目录
- 调研报告: `docs/research/` + 根目录 .md 文件

---

**当前最新 commit**: `3d7c6d3`
**W1-W4 全部状态**: ✅ 完成
**Build size**: 0x1970c0 / 0x3f0000 (60% free)