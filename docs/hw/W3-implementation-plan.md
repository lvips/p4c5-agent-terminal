# W3 Phase 1 实施计划 — 语音上行链路 (OMT 等同功能)

> **目标**: 把 ESP32 终端的音频采集 → OPUS 编码 → WS Binary 上行 → Mac Adapter ASR 链路打通
> **对齐目标**: 与 OMT (Tab5) 项目同等功能与形式
> **状态**: 🚧 实施中

---

## 🎯 验收标准

| 项 | 验收 |
|---|---|
| 4 麦录音 | p4c5_audio ES7210 4 通道录音 ✅ |
| 4ch→1ch 混音 | audio_mixer ✅ |
| 24kHz→16kHz 重采样 | resampler (24→16) ✅ |
| VAD 门控 | 能量阈值 VAD ✅ |
| Opus 编码 (16kbps, 60ms 帧) | libopus ✅ |
| WS Binary 帧上行 | dsh_client_send_audio ✅ |
| Mac Adapter ASR | 阿里云 DashScope 流式 ASR ✅ |
| 文字 → DSH 协议层 user_input 上行 | 已有 ✅ |
| Mock server 收到文字 | 验证 ✅ |

---

## 📋 实施步骤

### Step 1: 添加 OMT 4 个组件到我们项目

**复制组件**:
- `audio_input` (ESP32 端 4 麦录音 + VAD)
- `audio_mixer` (4ch → 1ch 混音)
- `resampler` (24k → 16k, **从 48→16 改为 24→16**)
- `opus_encoder` (libopus 封装)

**修改项**:
- 引脚配置 (Tab5 → P4C5)
- 采样率 (48kHz → 24kHz, 适配 p4c5_audio)
- BSP 抽象 → p4c5_audio API

### Step 2: 加 libopus 依赖

```yaml
# main/idf_component.yml
dependencies:
  78/esp-opus: "^1.0.5"
```

### Step 3: 扩展 dsh_client 加 send_audio (WS Binary)

```c
// dsh_client_transport_ws.c
int dsh_client_send_audio(const uint8_t* data, size_t len);
```

### Step 4: 写 audio_uplink_task (main/app_main.c)

```c
static void audio_uplink_task(void *arg) {
    // 20ms 帧循环
    for (;;) {
        // 1. p4c5_audio_record_multi (4ch 24kHz)
        // 2. audio_mixer (4→1)
        // 3. resampler (24k→16k, 480 → 320 samples)
        // 4. VAD 检查
        // 5. opus_encoder_encode
        // 6. dsh_client_send_audio (WS Binary)
    }
}
```

### Step 5: 加录音控制 (`s_app_recording`)

**MVP**: 触发方式
- 选项 1: UI 按钮 (LVGL touch)
- 选项 2: 串口命令 (`voice_on` / `voice_off`)
- 选项 3: 暂时手动 GPIO 触发

**当前选择**: 串口命令 (`audio start` / `audio stop`)

### Step 6: Mac Adapter / Mock Server 加 ASR Pipeline

**实现位置**: `tools/p4c5_asr_adapter.py` (新建)

**功能**:
- 接收 WS Binary (Opus 帧)
- 用 opuslib 解码 → PCM
- VAD 检测 (能量阈值)
- 静默 900ms 后调阿里云 DashScope 流式 ASR
- 文字 → 通过 W2 协议层 `client/user_input` 上行到 mock server

**或者**: 扩展 mock_dsh_server.py 加 ASR pipeline

### Step 7: 端到端测试

**测试流程**:
1. 启动 mock_dsh_server (8765)
2. 启动 p4c5_asr_adapter (8766, 监听 ESP32 WS)
3. ESP32 连接 p4c5_asr_adapter
4. 串口命令 `audio start`
5. 用户说话 (对着 4 麦)
6. 看 mock server 是否收到 `client/user_input` 含识别文字

---

## ⚠️ 风险

| 风险 | 缓解 |
|---|---|
| OMT 组件用 BSP 抽象 | 移植需重写 init 部分 |
| 24kHz 重采样未实现 | OMT resampler 写的是 48→16, 需改写或新写 24→16 |
| p4c5_audio 是同步 API, OMT audio_input 是异步任务 | 需在 p4c5_audio 基础上加 task 包装 |
| libopus 在 ESP32-P4 上 CPU 占用 | 压测, 可能需用 16kbps 低复杂度 |
| 阿里云 API key 申请 | 用公开测试 key 或 mock |

---

## 📊 资源评估

| 项 | 大小 |
|---|---|
| libopus (~200KB) | flash +200KB |
| audio_input + mixer + resampler | flash +50KB |
| audio_uplink_task (24KB 栈) | RAM +24KB |
| Opus 缓冲 (1 帧 1276B) | RAM +2KB |
| 4ch PCM 缓冲 | RAM +10KB |

---

## 📅 时间线

| Day | 任务 |
|---|---|
| Day 1 | 复制 OMT 组件 + 改引脚 + 加 libopus 依赖 + 编译 |
| Day 2 | 改 resampler 为 24k→16k + 写 audio_uplink_task |
| Day 3 | dsh_client 加 send_audio + 串口命令触发 |
| Day 4 | 写 Mac adapter ASR pipeline |
| Day 5 | 端到端测试 |

---

## 📚 参考

- [OMT audio_uplink_task](../../../../../../项目开发/OMT/hardware/tab5-agent-terminal/main/app_main.cpp)
- [OMT asr.js](../../../../../../项目开发/OMT/hardware/tab5-adapter/asr.js)
- [W3-competitive-analysis.md](W3-competitive-analysis.md)