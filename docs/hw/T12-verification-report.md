# T12 验证报告：R4 + R10 + R11 修复

> **版本**：v1.0（2026-09-06）
> **作者**：CCA（科研主管）
> **测试日期**：2026-09-06
> **固件 binary**：`p4c5_agent_terminal.bin` (1,061,424 bytes / 0x105130)
> **芯片**：ESP32-P4 v1.3, ECO2
> **IDF**：ESP-IDF v5.5.5

---

## 1. 修复内容

### R4：ALDO4 电压提升（2.9V → 3.4V）

| 项 | 修改前 | 修改后 |
|---|---|---|
| `p4c5_pmic.cc` | `axp2101_set_ldo_voltage(ALDO4, 2.9f)` | `axp2101_set_ldo_voltage(ALDO4, 3.4f)` |
| `config.h` | `P4C5_PMIC_ALDO4_MV 2900` | `P4C5_PMIC_ALDO4_MV 3400` |
| AXP2101 REG95 | N=24, reg=0x18 | N=29, reg=0x1D |

**原因**：ML307C 最低工作电压 3.4V (VBAT range: 3.4-4.4V typ)，2.9V 导致波特率检测失败。

### R10：DSH URL menuconfig

新增 Kconfig 选项（`main/Kconfig.projbuild`）：
- `P4C5_DSH_WEBSOCKET_URL` — WebSocket URL（默认 `ws://dsh.example.com/ws`）
- `P4C5_DSH_DEVICE_ID` — 设备 ID（默认 `p4c5-001`）
- `P4C5_DSH_AUTH_TOKEN` — 认证 token（默认空）

`app_main.c` 改用 `CONFIG_*` 宏，不再硬编码。

### R11：ST7123 触摸集成

| 项 | 修改前 | 修改后 |
|---|---|---|
| Touch init | TODO 桩代码 | 完整初始化 |
| I2C bus | 未接入 | `p4c5_board_get_i2c_bus()` |
| I2C 地址 | 0x5A（错误） | **0x55**（实测正确） |

**关键发现**：config.h 中 `P4C5_TP_I2C_ADDR` 原定义为 0x5A，但实测 ST7123 响应在 0x55（Espressif 组件默认地址）。酷世原理图注释有误。

---

## 2. 硬件验证结果

### 串口日志关键行

```
I (1725) p4c5_pmic:   ALDO4 = 3.4V ✅ (R4 fixed: was 2.9V, ML307C needs ≥3.4V)
I (1979) p4c5_pmic: All 13 registers verified ✅

I (2865) ST7123: Firmware version: 1(1.71.1.5), Max.X: 480, Max.Y: 800, Max.Touchs: 5
I (2865) ST7123: Touch panel create success, version: 1.0.2
I (2867) p4c5_display: ST7123 touch initialized ✅ (addr=0x55)
I (2878) p4c5_display: Display initialized: 480x800 RGB565, BL=255

I (2950) p4c5_audio: BoxAudioDevice initialized
```

### 6 步初始化检查

| 步骤 | 模块 | 状态 | 详情 |
|---|---|---|---|
| [0/5] | Board (I2C) | ✅ | SDA=7, SCL=8, 400kHz |
| [1/5] | PMIC (AXP2101) | ✅ | ID=0x4A, ALDO4=3.4V, 13/13 regs OK |
| [2/5] | Display (ST7102) | ✅ | LCD init OK, **ST7123 touch ✅** |
| [3/5] | Audio (ES8311+ES7210) | ✅ | DAC + ADC + 4mic TDM |
| [4/5] | 4G (ML307C) | ❌ | 波特率检测循环（ALDO4 提升后仍未解决） |
| [5/5] | dsh_client | ✅ | init OK, CONFIG_* URL 生效 |

---

## 3. 未解决问题

### 3.1 ML307C 波特率检测失败（仍阻塞）

**现象**：ALDO4 已从 2.9V 提升到 3.4V，但 `AtUart: Detecting baud rate...` 仍每 1.17s 循环。

**排查**：
1. ALDO4 电压不够？— 尝试提升到 3.5V（最大值）
2. ALDO4 电流不足？— 3.4V 空载 OK，但 ML307C 发射时可能电压跌落
3. ML307C 需要不同上电时序？— PWR 引脚脉冲宽度可能不对
4. UART 默认波特率不在检测范围？— 可能需要先以 115200 通信

**结论**：R4 修复了 ALDO4 电压问题，但 ML307C 不响应可能涉及多个因素，需要进一步硬件调试。

---

## 4. Commit 记录

| Commit | 内容 |
|---|---|
| `2b7cedf` | fix(T12-R4): ALDO4 from 2.9V to 3.4V for ML307C |
| `ecacc7e` | feat(T12-R10): DSH client config via menuconfig |
| `eec7341` | feat(T12-R11): Integrate ST7123 touch with shared I2C bus |
| `1875373` | fix(T12-R11): ST7123 touch address 0x55 (not 0x5A) |

---

## 5. 风险状态更新

| 风险 | M10 状态 | T12 更新 |
|---|---|---|
| **R4** | ❌ ALDO4=2.9V, ML307C 不响应 | ✅ **ALDO4=3.4V 已修复**（ML307C 仍需进一步调试） |
| **R10** | 🟡 DSH URL 硬编码 | ✅ **menuconfig 可配置** |
| **R11** | 🟡 Touch 未集成 | ✅ **ST7123 触摸初始化成功**（0x55, 5-point, 480×800） |

---

**报告版本**：v1.0（2026-09-06, CCA T12）
