# AXP2101 PMIC 调研报告

> **版本**：v2.0（2026-09-06，CCB T11/T12/T13 更新）
> **资料来源**：AXP2101 规格书 Rev1.4 `docs/hw/datasheets/C3036461_*.PDF`
> **验证状态**：T11 硬件实测通过（commit f449dbd）

---

## 1. 核心规格

| 项 | 规格 | 来源 |
|---|---|---|
| **I2C 地址** | **0x34** | 规格书 p.1 |
| **CHIP_ID** | **0x4A**（reg 0x03）| 规格书 p.30, §6.13.2.1 |
| **输入电压** | 2.6V–5.5V | 规格书 p.15 §6.3 |
| **封装** | 5×5 mm QFN-40 | 规格书 p.3 |
| **充电** | 内置 CC/CV，截止电压可配 4.0/4.1/4.2/4.35/4.4V | 规格书 p.23 §6.7.1 |
| **库仑计** | 精确电量计量（±3%）| 规格书 p.21 §6.6 |

### 输出通道（本项目使用）

| 通道 | 类型 | 范围 | **本项目配置** | 用途 | 来源 |
|---|---|---|---|---|---|
| DCDC1 | DC-DC | 0.5–3.4V | **3.3V** | ESP32-P4/C5/外设主电源 | p4c5_pmic.cc L275 |
| ALDO1 | LDO | 0.5–3.5V | **1.8V** | 辅助 | p4c5_pmic.cc L286 |
| ALDO3 | LDO | 0.5–3.5V | **3.3V** | 音频 codec (ES8311+ES7210) | p4c5_pmic.cc L296 |
| **ALDO4** | LDO | 0.5–3.5V | **3.4V** ✅ | 4G 模组 VBAT | p4c5_pmic.cc L308 (T12 修复) |
| ALDO2 | LDO | 0.5–3.5V | 3.3V (兼容) | 未用，xiaozhi 兼容 ~1mA | p4c5_pmic.cc L343 |

---

## 2. 13 个特殊寄存器（init table，已核对）

> 来源：`p4c5_pmic.cc` `s_init_seq[]` (L75–106)。
> 规格书章节：§6.13.2.x，页码均为 Rev1.4 PDF 页码。

| # | Reg | Val | RMW? | 功能 | 规格书章节 | 页码 | 位定义 | 验证 |
|---|---|---|---|---|---|---|---|---|
| 1 | 0x22 | 0x06 | N | PWROFF_EN | §6.13.2.19 | p.36 | [2]=1 DIE OT, [1:0]=EFUSE | ✅ 通过 |
| 2 | 0x27 | 0x10 | N | IRQ/OFF/ON level | §6.13.2.24 | p.38 | [5:4]=01 IRQ=1.5s, [3:2]=00 OFF=4s, [1:0]=EFUSE | ✅ 通过 |
| 3 | 0x93 | 0x1C | N | ALDO2 voltage | §6.13.2.78 | p.54 | [4:0]=0x1C=28 → 3.3V | ✅ 通过 |
| 4 | 0x90 | 0x02 | **Y** | LDO ON/OFF ctrl 0 | §6.13.2.75 | p.53 | [3]=ALDO4 en, [2]=ALDO3 en, [1]=ALDO2 en(OR) | ✅ RMW |
| 5 | **0x64** | 0x03 | **Y** | Charge voltage limit | §6.13.2.62 | p.48 | **[2:0]=011=4.2V**, [7:3]=RO | ✅ T11 RMW |
| 6 | 0x61 | 0x05 | N | Precharge current | §6.13.2.59 | p.47 | [3:0]=0101=100mA (25×N) | ✅ 通过 |
| 7 | 0x62 | 0x0A | N | Constant charge current | §6.13.2.60 | p.48 | [4:0]=01010=10 → 200+100×(10-8)=400mA | ✅ 通过 |
| 8 | 0x63 | 0x15 | N | Termination current | §6.13.2.61 | p.48 | [4]=1 enable, [3:0]=0101=125mA | ✅ 通过 |
| 9 | 0x14 | 0x00 | N | Min system Vsys DPM | §6.13.2.10 | p.33 | [6:4]=000=4.1V | ⚠️ 注释有误 |
| 10 | 0x15 | 0x00 | N | Input voltage limit | §6.13.2.11 | p.33 | [3:0]=0000=3.88V | ✅ 通过 |
| 11 | 0x16 | 0x05 | N | Input current limit | §6.13.2.12 | p.34 | [2:0]=101=2000mA | ⚠️ 注释有误 |
| 12 | 0x24 | 0x01 | N | PWROFF VSYS threshold | §6.13.2.21 | p.37 | [2:0]=001=2.7V | ✅ 通过 |
| 13 | 0x50 | 0x14 | N | TS pin ctrl | §6.13.2.46 | p.45 | [4]=1 TS固定输入, [3:2]=01, [1:0]=00=20uA | ✅ 通过 |

### ⚠️ 注释差异标注

| Reg | 代码注释 | 规格书实际 | 影响 |
|---|---|---|---|
| 0x14 | "最低系统电压=3.0V" | [6:4]=000 → **4.1V** (Linear Charger Vsys DPM) | 功能正常，注释需修正 |
| 0x16 | "输入限流=350mA" | [2:0]=101 → **2000mA** (查表) | 功能正常，注释需修正 |
| 0x14 (L96) | "VSYS关机阈值=3.1V" | 0x24 reg 才是关机阈值；0x14 是充电 DPM | 寄存器编号对应正确 |

---

## 3. 关键寄存器详解

### 3.1 Reg 0x64: CV Charger Voltage Setting

**来源**：规格书 p.48, §6.13.2.62

| Bit | 描述 | R/W | Default |
|---|---|---|---|
| [7:3] | 只读，硬件屏蔽，始终返回 0 | RO | 0 |
| [2:0] | 充电截止电压（3-bit 查表）| RW | 011b |

**查表**：

| [2:0] | 电压 | 十六进制值 |
|---|---|---|
| 000 | 5.0V | 0x00 |
| 001 | 4.0V | 0x01 |
| 010 | 4.1V | 0x02 |
| **011** | **4.2V** ← 默认 | **0x03** |
| 100 | 4.35V | 0x04 |
| 101 | 4.4V | 0x05 |
| 11X | reserved | — |

**T11 结论**：写入 0x2B → 硬件屏蔽 [7:3] → 实际存储 0x03 = 4.2V ✅。xiaozhi 原值 0x03 一直是正确的，不是 bug。

### 3.2 Reg 0x95: ALDO4 Voltage Setting

**来源**：规格书 p.54, §6.13.2.80

| 项 | 值 |
|---|---|
| 位定义 | [4:0] RW, [7:5] RO |
| 电压公式 | V = 0.5 + N × 0.1V |
| 范围 | 0.5V ~ 3.5V (31 steps) |
| 当前值 | **3.4V → N=29 → reg=0x1D** (T12 修复) |
| 关联 | Reg 0x90 [bit 3] = ALDO4 enable |

### 3.3 Reg 0x90: LDOS ON/OFF Control 0

**来源**：规格书 p.53, §6.13.2.75

| Bit | LDO | 0=disable | 1=enable |
|---|---|---|---|
| [0] | DCDC5 | — | — |
| [1] | ALDO2 | — | 本项目 OR 写入 |
| [2] | ALDO3 | — | 3.3V (音频) |
| [3] | ALDO4 | — | 3.4V (4G) |
| [4] | BLDO1 | — | 未用 |
| [5] | BLDO2 | — | 未用 |
| [6] | CPUSLDO | — | 未用 |
| [7] | DLDO1 | — | 未用 |

---

## 4. 风险标注

| ID | 风险 | 状态 | 来源 |
|---|---|---|---|
| R2.2 | 0x64 充电电压 | ✅ T11 已修复：0x03=4.2V 正确 | 规格书 p.48 |
| R4 | ALDO4 电压不足 | ✅ T12 已修复：3.4V ≥ ML307C 最低要求 | 规格书 p.54 |
| R2.3 | 0x16 输入限流注释有误 | 🟡 功能正常，注释待修正 | 规格书 p.34 |
| R2.4 | ALDO2 多余使能 (~1mA) | 🟡 低影响，保持兼容 | 规格书 p.53 |
| R12 | ksdiy_lvgl_port.c 重复写 ALDO4 | 🟡 两处均已 3.4f | p4c5_pmic.cc L308 + ksdiy_lvgl_port.c L181 |

---

## 5. 与 OMT/Tab5 差异

| 维度 | OMT Tab5 | P4C5 |
|---|---|---|
| PMIC | AXP2101 仅做电源轨 | AXP2101 电源轨 + 充电管理 |
| 充电控制 | PI4IOE5V6416 IO 扩展器 (I2C 0x44) | AXP2101 内置 CC/CV |
| 电量检测 | ADC 电压法 (±15%) | 库仑计 (±3%) |
| 4G VBAT 控制 | 始终上电 | ALDO4 可独立断电 |

---

## 6. xiaozhi 参考配置（DSH 调研 2026-09-06）

xiaozhi `kevin-p4c5-4g` 板子使用 AXP2101 的完整寄存器配置。
来源：`/tmp/p4c5_xiaozhi/main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc` L41-119。

### 6.1 Pmic 构造函数（13 寄存器）

```c
// kevin_p4c5_4g_board.cc L41-61
WriteReg(0x22, 0b110);    // PWROFF_EN
WriteReg(0x27, 0x10);     // IRQ/OFF/ON level
WriteReg(0x93, 0x1C);     // ALDO2 = 3.3V
value = ReadReg(0x90) | 0x02; WriteReg(0x90, value);  // ALDO2 enable (RMW)
WriteReg(0x64, 0x03);     // Charge voltage = 4.2V
WriteReg(0x61, 0x05);     // Precharge = 125mA
WriteReg(0x62, 0x0A);     // Constant charge = 400mA
WriteReg(0x63, 0x15);     // Termination current
WriteReg(0x14, 0x00);     // Min Vsys DPM = 4.1V
WriteReg(0x15, 0x00);     // Input voltage limit = 3.88V
WriteReg(0x16, 0x05);     // Input current limit = 2000mA
WriteReg(0x24, 0x01);     // PWROFF VSYS threshold = 2.7V
WriteReg(0x50, 0x14);     // TS pin ctrl
```

### 6.2 电压轨配置

```c
// kevin_p4c5_4g_board.cc L99-119
axp2101_set_dcdc_voltage(KSDIY_PMIC_DCDC1, 3.3f);   // 主电源
axp2101_set_ldo_voltage(KSDIY_PMIC_LDO_ALDO1, 1.8f); // 辅助
axp2101_set_ldo_voltage(KSDIY_PMIC_LDO_ALDO3, 3.3f); // 音频
axp2101_set_ldo_voltage(KSDIY_PMIC_LDO_ALDO4, 2.9f); // 4G VBAT ← 关键差异
vTaskDelay(pdMS_TO_TICKS(50));
```

### 6.3 寄存器逐项对照

| 地址 | 名称 | xiaozhi 值 | p4c5 值 | 章节 | 页码 | 状态 |
|---|---|---|---|---|---|---|
| 0x22 | PWROFF_EN | 0x06 | 0x06 | §6.13.2.19 | p.36 | ✅ 一致 |
| 0x27 | IRQ/OFF/ON | 0x10 | 0x10 | §6.13.2.24 | p.38 | ✅ 一致 |
| 0x93 | ALDO2 voltage | 0x1C (3.3V) | 0x1C | §6.13.2.78 | p.54 | ✅ 一致 |
| 0x90 | LDO enable | RMW 0x02 | RMW 0x02 | §6.13.2.75 | p.53 | ✅ 一致 |
| 0x64 | Charge voltage | 0x03 (4.2V) | 0x03 | §6.13.2.62 | p.48 | ✅ 一致 |
| 0x61 | Precharge | 0x05 (125mA) | 0x05 | §6.13.2.59 | p.47 | ✅ 一致 |
| 0x62 | Constant charge | 0x0A (400mA) | 0x0A | §6.13.2.60 | p.48 | ✅ 一致 |
| 0x63 | Termination | 0x15 | 0x15 | §6.13.2.61 | p.48 | ✅ 一致 |
| 0x14 | Min Vsys DPM | 0x00 (4.1V) | 0x00 | §6.13.2.10 | p.33 | ✅ 一致 |
| 0x15 | Input V limit | 0x00 (3.88V) | 0x00 | §6.13.2.11 | p.33 | ✅ 一致 |
| 0x16 | Input I limit | 0x05 (2000mA) | 0x05 | §6.13.2.12 | p.34 | ✅ 一致 |
| 0x24 | PWROFF VSYS | 0x01 (2.7V) | 0x01 | §6.13.2.21 | p.37 | ✅ 一致 |
| 0x50 | TS pin ctrl | 0x14 | 0x14 | §6.13.2.46 | p.45 | ✅ 一致 |

### 6.4 关键差异

| 差异 | xiaozhi | p4c5 | 说明 |
|---|---|---|---|
| **ALDO4 电压** | **2.9V** (0x18) | **3.4V** (0x1D) |  p4c5 T12 基于 ML307C 规格书修复 |
| 寄存器验证 | 无 | 读回验证 | p4c5 增强 |
| 运行时电源控制 | 无 | `p4c5_pmic_set_4g_power()` | p4c5 增强 |
| DTR 引脚 | 无 | GPIO51 | p4c5 增强（低功耗）|

### 6.5 风险标注

| 风险 | 说明 | 优先级 |
|---|---|---|
| ALDO4 2.9V vs 3.4V | xiaozhi 验证 2.9V 可用，ML307C 规格书要求 ≥3.4V | **P1** — M12 实测 |
| Display 重复写 ALDO4 | ksdiy_lvgl_port.c 也写 ALDO4，需两处同步 | P1 |
| 0x14/0x16 注释 | 代码注释与规格书不一致，功能正常 | P3 |

---

**下次更新**：0x14/0x16 注释修正 + M12 ALDO4 实测后
