# p4c5_audio 组件测试规范

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA（真机验证）+ M5 阶段长稳测试
> **配套文档**：`M4-development-guide.md` §3.4 + `audio-codecs.md`

---

## 1. 测试目标

1. **完整音频链路验证**：ES8311（DAC）→ NS4150B（PA）→ 扬声器 + ES7210（ADC）← 4 麦 → I2S
2. **4 麦录音质量**：4 通道独立录音 + 波束成形底噪评估
3. **PA 输出功率**：NS4150B 在不同音量下的 THD+N 和最大输出
4. **AEC 回声消除**：扬声器播放时录音，验证回声抑制效果

---

## 2. 测试环境

### 2.1 硬件

| 项 | 规格 | 备注 |
|---|---|---|
| 开发板 | ESP32-P4C5 酷世 DIY | 含 4 麦 + 扬声器 + NS4150B |
| 供电 | USB-C 5V/2A | 必须 2A（PA + 4G 峰值） |
| 扬声器 | 4Ω / 8Ω 1W 以上 | 板载或外接 |
| 麦克风 | MSM381A3729H9CP ×4 | 板载 MEMS 差分 |
| 示波器 | ≥20MHz 带宽 | 测 I2S 时序（MCLK/BCLK/WS/DOUT） |
| 万用表 | 直流电压档 | 测 PA 供电（5V）+ ALDO3（3.3V）|
| 声级计 | 可选 | 测 SPL（扬声器输出声压级）|

### 2.2 软件

| 项 | 版本 | 说明 |
|---|---|---|
| 固件 | p4c5-agent-terminal v0.1.0 | CCA T2 产出 |
| IDF | ≥ 5.5.5 | 必须 |
| 串口终端 | `idf.py monitor` | 115200bps |
| PC 录音工具 | xiaozhi `scripts/acoustic_check/graphic.py` | UDP 接收 PCM 做时域/频域分析 |
| 音频分析 | Python + numpy/scipy | 离线 THD+N / FFT |

### 2.3 关键参数（来自 xiaozhi config）

| 参数 | 值 | 来源 |
|---|---|---|
| 输入采样率 | 24000 Hz | `config.h` `AUDIO_INPUT_SAMPLE_RATE` |
| 输出采样率 | 24000 Hz | `config.h` `AUDIO_OUTPUT_SAMPLE_RATE` |
| 位深 | 16 bit | `box_audio_codec.cc` |
| I2S TX 模式 | STD（单声道 16bit）| `CreateDuplexChannels` |
| I2S RX 模式 | TDM（4 通道 16bit）| `CreateDuplexChannels` |
| ES8311 I2C | 0x18 | `config.h` |
| ES7210 I2C | 0x40 | `config.h` |
| PA_EN | GPIO3 | `config.h` `AUDIO_CODEC_PA_PIN` |
| MCLK/BCLK/WS/DOUT/DIN | 13/12/10/9/11 | `config.h` |
| AEC 参考 | `input_reference = true` | `config.h` `AUDIO_INPUT_REFERENCE` |
| 输入增益 | 30 dB | `box_audio_codec.cc` `input_gain_` |
| PA 电压 | 5.0V | `es8311_cfg.hw_gain.pa_voltage` |

---

## 3. 测试用例

### TC-01: ES8311 设备识别

- **前置条件**：AXP2101 ALDO3 = 3.3V 已输出，I2C 总线初始化完成
- **测试步骤**：调用 `p4c5_audio_init()` 后观察串口日志
- **预期结果**：日志输出 `I ... BoxAudioCodec: BoxAudioDevice initialized`
- **通过标准**：日志包含该字符串且无 `E` 级别 I2C 错误

### TC-02: ES7210 设备识别

- **前置条件**：TC-01 通过
- **测试步骤**：I2C 扫描 + 读 ES7210 寄存器 0x00
- **预期结果**：I2C 扫描到地址 0x40；ES7210 响应正常
- **通过标准**：日志无 `assert` 失败，`es7210_codec_new()` 返回非 NULL

### TC-03: I2S 时钟波形验证

- **前置条件**：TC-01 通过
- **测试步骤**：示波器探头接 GPIO13 (MCLK)，触发测频率
- **预期结果**：MCLK = 24000 × 256 = **6.144 MHz**（±1%）
- **通过标准**：实测频率在 6.08 - 6.21 MHz 范围内
- **测量点**：MCLK=GPIO13, BCLK=GPIO12, WS=GPIO10

### TC-04: 1kHz 测试音播放

- **前置条件**：TC-01 + TC-03 通过
- **测试步骤**：调用 `p4c5_audio_play_tone(1000, 5000)` 生成 1kHz 正弦波播放 5 秒，音量 70%
- **预期结果**：扬声器发出清晰 1kHz 纯音，无杂音/破音
- **通过标准**：① 耳朵可听纯音 ② 示波器测 I2S DOUT (GPIO9) 有 1kHz 正弦 PCM 波形 ③ 日志无 `esp_codec_dev_write` 错误

### TC-05: PA 使能控制

- **前置条件**：TC-04 通过
- **测试步骤**：① 设 `PA_EN = GPIO3 = LOW`，播放测试音 ② 设 `PA_EN = HIGH`，再播放
- **预期结果**：① GPIO3=LOW 时扬声器无声（NS4150B 进入关断 <1µA）② GPIO3=HIGH 时扬声器有声
- **通过标准**：万用表测 GPIO3 电平切换正确；声级计测无声时 SPL < 30dB

### TC-06: 音量调节范围

- **前置条件**：TC-04 通过
- **测试步骤**：从 volume=0 到 volume=100，每 10 步播放 1s 测试音，记录 SPL
- **预期结果**：SPL 随音量递增，volume=0 近似静音，volume=100 无破音
- **通过标准**：至少 8 个档位 SPL 递增；100% 无削波（THD < 10%）

### TC-07: 4 麦独立录音

- **前置条件**：TC-02 通过
- **测试步骤**：`p4c5_audio_record_start()` 录音 3 秒，保存 4 通道 WAV（PC 端 `graphic.py` 接收）
- **预期结果**：4 个 WAV 文件各有独立音频数据；对 MIC1 说话时仅 MIC1 通道有信号
- **通过标准**：4 通道均有非零数据；目标通道 RMS > 1000，非目标通道 RMS < 200

### TC-08: 4 麦录音回放

- **前置条件**：TC-07 通过
- **测试步骤**：录音 5 秒后通过扬声器回放（单声道混合）
- **预期结果**：扬声器可听到清晰录音内容
- **通过标准**：录音可辨识语音内容（播一段话回放能听清）

### TC-09: AEC 回声消除

- **前置条件**：TC-04 + TC-07 通过
- **测试步骤**：① 扬声器播放 1kHz 测试音（volume=50%）② 同时启动录音 5 秒 ③ 分析录音 FFT
- **预期结果**：录音中 1kHz 分量被大幅抑制（相比无 AEC 时）
- **通过标准**：AEC 开启后 1kHz 分量幅度比关闭时降低 ≥ 20dB
- **关键配置**：`input_reference = true`（`config.h`），AEC mode = `AFE_MODE_HIGH_PERF`

### TC-10: 采样率切换

- **前置条件**：TC-04 通过
- **测试步骤**：分别设置采样率为 8000/16000/24000/48000 Hz，播放测试音
- **预期结果**：各采样率下播放正常，音调正确（不加速/减速）
- **通过标准**：24000 Hz 为默认，必须通过；其他采样率可作参考
- **注意**：ESP-IDF `esp_audio_codec` 可能只支持部分采样率

### TC-11: 音频编解码器资源释放

- **前置条件**：TC-04 + TC-07 通过
- **测试步骤**：反复调用 `init()` → `play()` → `record()` → `deinit()` 10 次
- **预期结果**：无内存泄漏，每次 `deinit()` 后 I2S/I2C 资源释放
- **通过标准**：10 次循环后 `heap_caps_get_free_size()` 波动 < 1%

### TC-12: 双工模式（同时录放）

- **前置条件**：TC-04 + TC-08 通过
- **测试步骤**：同时播放 1kHz 测试音 + 录音 5 秒
- **预期结果**：播放和录音同时进行，不卡顿不死锁
- **通过标准**：录音数据无断续（DMA underrun 计数 = 0）；播放无爆音

---

## 4. 性能测试

### 4.1 THD+N 测量（PA 输出）

| 条件 | 目标 |
|---|---|
| 测试信号 | 1kHz 正弦波，volume=50% |
| 负载 | 4Ω 电阻代替扬声器 |
| 测量 | 示波器采集 PA 输出 → FFT |
| 通过标准 | THD+N < 1% @ 1W（NS4150B 规格书典型值 0.1% @ 1W/8Ω/1kHz）|
| 失败排查 | 若 THD > 5%：检查 PA 供电 5V 是否稳定；检查 I2S 数据位宽是否 16bit |

### 4.2 频率响应

| 条件 | 目标 |
|---|---|
| 扫频范围 | 100Hz - 10kHz（24kHz 采样率 Nyquist = 12kHz）|
| 方法 | 播放扫频信号 + PC 端录回 → 计算各频点幅度 |
| 通过标准 | 200Hz - 8kHz 范围内波动 ≤ ±3dB |
| 注意 | 扬声器本身频响是瓶颈，此测主要验证 ES8311 + I2S 链路平坦度 |

### 4.3 信噪比（4 麦）

| 条件 | 目标 |
|---|---|
| 环境 | 安静房间（背景噪声 < 30dBA）|
| 方法 | 录音 10 秒（无人说话），计算各通道 RMS |
| 通过标准 | 各通道底噪 RMS < 200（16bit PCM，满幅 32768）|
| 参考 | ES7210 规格书 SNR ≥ 104dB（A-weighted）|
| 失败排查 | 若底噪高：检查 ALDO3 纹波；检查 MEMS 麦焊接；检查 PCB 地平面 |

### 4.4 AEC 收敛时间

| 条件 | 目标 |
|---|---|
| 方法 | 突然开始播放 1kHz 信号，计时到录音中该信号被抑制 > 20dB |
| 通过标准 | 收敛时间 < 500ms |
| 配置 | `afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF` |
| 注意 | xiaozhi acoustic_check 记录显示 ES7210 需关闭 `INPUT_REFERENCE` 才能正常做声波解码——AEC 参考通道可能干扰原始信号 |

---

## 5. 长稳测试（24h）

### 5.1 测试计划

| 阶段 | 时长 | 操作 | 监控项 |
|---|---|---|---|
| Phase 1 | 0-6h | 持续播放 1kHz 测试音（volume=50%）| PA 温度（每 30min 手触检查）|
| Phase 2 | 6-12h | 持续录音（4 麦全开）| 内存使用量（每 1h 记录）|
| Phase 3 | 12-18h | 交替录放（各 30s 循环）| DMA underrun 计数 |
| Phase 4 | 18-24h | 双工模式（同时录放）| I2S 错误计数 + 系统日志 |

### 5.2 通过标准

- **0 次** panic / Guru Meditation Error / assert 失败
- **内存泄漏** < 5%（24h 前后 `heap_caps_get_free_size()` 对比）
- **PA 温度** < 70°C（手触烫但可坚持 3 秒）
- **I2S 错误** = 0（DMA underrun/overflow 计数）
- **音频质量** 无退化（24h 后重测 TC-04，THD+N 变化 < 1dB）

### 5.3 自动化监控脚本

```python
# 串口采集关键指标（每 60s 打印一次）
# 通过 idf.py monitor 输出解析：
# - free_heap: xxx bytes
# - i2s_err_count: xxx
# - pmic_temp: xx.x C
# - pa_temp: xx.x C (需外部温度传感器)
```

---

## 6. 失败排查

### 6.1 ES8311 无响应（I2C 超时）

| 检查项 | 方法 | 预期 |
|---|---|---|
| I2C 总线是否初始化 | 日志查 `i2c_new_master_bus` | 无 error |
| SDA/SCL 电平 | 万用表测 GPIO7/GPIO8 静态电平 | 高电平（内部上拉）|
| I2C 地址 | `i2c_master_probe(bus, 0x18)` | 返回 ACK |
| ALDO3 电压 | 万用表测 AXP2101 ALDO3 输出 | 3.3V ±5% |
| 复位时序 | 示波器测 ES8311 RESET 引脚 | 上电后 > 10ms 释放 |

### 6.2 扬声器无声

| 检查项 | 方法 | 预期 |
|---|---|---|
| PA_EN (GPIO3) | 万用表 / LED 测试 | 播放时为 HIGH（3.3V）|
| PA 供电 | 万用表测 NS4150B VDD | 5.0V ±10% |
| 音量寄存器 | `esp_codec_dev_set_out_vol(dev, 70)` | 返回 ESP_OK |
| MUTE 状态 | 检查 ES8311 寄存器 0x32 | 非静音位 |
| I2S DOUT 波形 | 示波器测 GPIO9 | 有 PCM 数据跳变 |
| 扬声器连接 | 万用表测扬声器阻抗 | 4Ω 或 8Ω（非开路）|

### 6.3 4 麦只录到 1-2 个通道

| 检查项 | 方法 | 预期 |
|---|---|---|
| `mic_selected` 寄存器 | 检查 `es7210_cfg.mic_selected` | `ES7210_SEL_MIC1\|MIC2\|MIC3\|MIC4` |
| ES7210 MICBIAS | 万用表测 MICBIAS 输出 | ~2.5V（供 MEMS 麦偏置）|
| MEMS 麦焊接 | 放大镜/万用表检查 | 4 颗麦全部焊好 |
| TDM slot 配置 | 检查 `i2s_tdm_slot_mask_t` | `SLOT0\|SLOT1\|SLOT2\|SLOT3` |
| 通道映射 | 分析 WAV 各通道数据 | 每麦对应独立通道 |

### 6.4 AEC 回声消除不工作

| 检查项 | 方法 | 预期 |
|---|---|---|
| `input_reference` | 检查 config.h | `AUDIO_INPUT_REFERENCE = true` |
| 参考通道 | 检查 `fs.channel_mask` | 包含 `CHANNEL_MASK(1)`（参考通道）|
| AEC 模型 | 日志查 `afe_config->aec_mode` | `AFE_MODE_VOIP_HIGH_PERF` |
| ES7210 INPUT_REFERENCE | xiaozhi 记录显示 ES7210 做声波解码需关闭此选项 | AEC 与原始信号存在冲突，需实测 |
| 延迟匹配 | 检查 AEC 参考信号与录音的对齐 | 延迟 < 10ms |

### 6.5 播放有爆音/杂音

| 检查项 | 方法 | 预期 |
|---|---|---|
| DMA underrun | 日志查 `i2s_err_count` | = 0 |
| I2S 时钟抖动 | 示波器测 MCLK 抖动 | < 1ns |
| PA 供电纹波 | 示波器 AC 耦合测 VDD | < 50mV p-p |
| 地线回路 | 检查 USB 地与板地 | 单点接地 |

### 6.6 内存泄漏

| 检查项 | 方法 | 预期 |
|---|---|---|
| `data_if_mutex_` 死锁 | 检查 Read/Write 并发 | mutex 正确加锁 |
| DMA 描述符泄漏 | 反复 `EnableInput/EnableOutput` | `heap` 不减少 |
| I2C 事务队列 | 检查 `trans_queue_depth` 配置 | 无堆积 |

---

## 7. 风险标注

引用可信度评估报告 R3.x 系列：

| 风险 | 描述 | 测试覆盖 | 缓解 |
|---|---|---|---|
| ~~R3.1~~ | ~~ES7210 规格书缺失~~ | — | ✅ **已解决**：用 `esp_audio_codec` 封装 |
| R3.2 | MEMS 麦克风（敏芯微 MSM381A3729H9CP）社区资料少 | TC-07/08 | 走 xiaozhi 封装，无需裸驱动 |
| R3.3 | PA 静态电流未实测 | TC-05 + 长稳 Phase 1 | 长稳阶段测 PA 温度推算功耗 |
| R3.4 | 4 麦间距（用于波束成形）未定义 | TC-07 | 测各麦独立信噪比，评估波束成形可行性 |
| R3.5 | 音频 PCB 走线（差分对/地层）需实机 | TC-03/04 + 性能测试 | 示波器测 I2S 信号完整性 |
| R3.6 | AEC 回声消除效果需实测 | TC-09 + 性能 4.4 | xiaozhi ES7210 需关 `INPUT_REFERENCE` 做声波解码——AEC 参考通道可能有冲突，需实机确认 |

**新增风险**（测试过程中发现）：

| 风险 | 描述 | 缓解 |
|---|---|---|
| R-AUDIO-1 | I2S TDM 模式（RX）与 STD 模式（TX）共用引脚（MCLK/BCLK/WS），时序冲突 | `CreateDuplexChannels` 中 TX 用 STD、RX 用 TDM，两者共享时钟线——示波器验证时序无冲突 |
| R-AUDIO-2 | 24kHz 采样率的 AEC 模型可能不默认支持（esp-sr 默认 16kHz）| 检查 `afe_config_init` 是否支持 24kHz，否则降采样到 16kHz 做 AEC |

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：M4 真机验证后，由 CCA 补充实测数据
