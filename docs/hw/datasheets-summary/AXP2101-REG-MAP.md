# AXP2101 寄存器完整速查表

> **版本**：v1.0（2026-09-06，CCB T13）
> **来源**：AXP2101 规格书 Rev1.4 `C3036461_*.PDF`
> **用途**：快速查找寄存器地址、位定义、规格书页码

---

## 使用说明

- **Reg**：寄存器地址（十六进制）
- **Section**：规格书章节编号
- **Page**：PDF 页码
- **Bits**：可写位定义
- **Default**：POR（上电复位）默认值
- **Init**：本项目 init table 写入值（— = 未配置）

---

## 寄存器表（0x00–0x6F）

| Reg | Section | Page | 名称 | Bits (R/W) | Default | Init | 说明 |
|---|---|---|---|---|---|---|---|
| 0x00 | §6.13.2.0 | p.30 | CHIP_ID | [7:0] RO | 0x4A | — | 芯片 ID，应读回 0x4A |
| 0x01 | §6.13.2.1 | p.30 | PMU_STATUS | [7:0] RO | — | — | 充电状态、电源状态 |
| 0x03 | §6.13.2.1 | p.30 | CHIP_ID | [7:0] RO | 0x4A | — | 同上 |
| 0x04–0x07 | §6.13.2.3-6 | p.32 | DATA_BUFFER 0-3 | [7:0] RW | 0x00 | — | 通用数据缓冲 |
| 0x10 | §6.13.2.7 | p.32 | PMU common config | [7:0] mixed | — | — | PMU 通用配置 |
| 0x12 | §6.13.2.8 | p.33 | BATFET control | [3] RW, rest RO | EFUSE | — | BATFET 使能 |
| 0x13 | §6.13.2.9 | p.33 | Die temp control | [2:1] RW, [0] RW | 01b, 1b | — | 过温保护 |
| **0x14** | §6.13.2.10 | p.33 | **Min system Vsys DPM** | [6:4] RW, rest RO | **110b** | **0x00** | 4.1+N×0.1V；**0x00=4.1V** |
| **0x15** | §6.13.2.11 | p.33 | **Input voltage limit** | [3:0] RW, [7:4] RO | **0110b** | **0x00** | 3.88+N×0.08V；**0x00=3.88V** |
| **0x16** | §6.13.2.12 | p.34 | **Input current limit** | [2:0] RW, [7:3] RO | **100b** | **0x05** | 查表；**0x05=2000mA** |
| 0x17 | §6.13.2.13 | p.34 | Reset fuel gauge | [3] RWAC, [2] RW | 0b | — | 库仑计复位 |
| 0x18 | §6.13.2.14 | p.34 | Charger/FG/WD on/off | [3:0] mixed | 1b,0b,1b,0b | — | 充电/库仑计/看门狗使能 |
| 0x19 | §6.13.2.15 | p.34 | Watchdog control | [5:4] RW, [2:0] RW | 0b, 110b | — | 看门狗超时配置 |
| 0x1A | §6.13.2.16 | p.35 | Low battery warning | [7:4] RW, [3:0] RW | 1010b, 0001b | — | 低电量警告阈值 |
| 0x20 | §6.13.2.17 | p.35 | PWRON status | [7:0] RO | — | — | 开机状态 |
| 0x21 | §6.13.2.18 | p.35-36 | PWROFF status | [7:0] RO | — | — | 关机状态 |
| **0x22** | §6.13.2.19 | p.36 | **PWROFF_EN** | [2] RW, [1:0] RW | 1b, EFUSE, EFUSE | **0x06** | 关机使能控制 |
| 0x23 | §6.13.2.20 | p.36 | DCDC OVP/UVP | [5:0] RW | 1b | — | DCDC 过压/欠压关机 |
| **0x24** | §6.13.2.21 | p.37 | **PWROFF VSYS threshold** | [2:0] RW, [7:3] RO | EFUSE | **0x01** | 2.6+N×0.1V；**0x01=2.7V** |
| 0x25 | §6.13.2.22 | p.37 | PWROK/PWROFF sequence | [4:0] mixed | 1b,1b,0b,EFUSE | — | 上下电时序 |
| 0x26 | §6.13.2.23 | p.37 | Sleep/wakeup | [4:0] mixed | — | — | 休眠/唤醒 |
| **0x27** | §6.13.2.24 | p.38 | **IRQ/OFF/ON level** | [5:4] RW, [3:2] RW, [1:0] RW | 01b, 01b, EFUSE | **0x10** | IRQ=1.5s, OFF=4s, ON=EFUSE |
| 0x28–0x2A | §6.13.2.25-27 | p.38-39 | Fast pwron 0-2 | [7:0] RW | 0b | — | 快速上电时序 |
| 0x2B | §6.13.2.28 | p.39 | Fast pwron 3 | [7:0] RW | 0b | — | 快速上电时序 |
| 0x30 | §6.13.2.29 | p.39 | ADC channel enable 0 | [7:0] mixed | — | — | ADC 通道使能 |
| 0x34–0x3D | §6.13.2.30-39 | p.40-41 | ADC 数据（VBAT/VSYS/TDIE/VBUS）| RO | 0b | — | 14-bit ADC 读数 |
| 0x40–0x4C | §6.13.2.40-42 | p.41-42 | IRQ Enable 0-2 | [7:0] RW | System Reset | — | 中断使能 |
| 0x49–0x4A | §6.13.2.44-45 | p.44 | IRQ Status 1-2 | [7:0] RW1C | 0b | — | 中断状态（写 1 清除）|
| **0x50** | §6.13.2.46 | p.45 | **TS pin CTRL** | [4] RW, [3:2] RW, [1:0] RW | EFUSE, EFUSE, 10b | **0x14** | TS 引脚功能配置 |
| 0x52–0x53 | §6.13.2.47-48 | p.45 | TS_HYSL2H / TS_HYSH2L | [7:0] RW | 2h, 1h | — | TS 迟滞 |
| 0x54–0x5B | §6.13.2.49-56 | p.45-47 | JEITA 温度配置 | [7:0] RW | various | — | JEITA 标准温度阈值 |
| **0x61** | §6.13.2.59 | p.47 | **Iprechg charger** | [3:0] RW, [7:4] RO | **0101b** | **0x05** | 预充电流 25×N mA；**0x05=125mA** |
| **0x62** | §6.13.2.60 | p.48 | **ICC charger** | [4:0] RW, [7:5] RO | {EFUSE,0b,EFUSE} | **0x0A** | 恒流充电电流；**0x0A=400mA** |
| **0x63** | §6.13.2.61 | p.48 | **Iterm charger** | [4] RW, [3:0] RW | 1b, 0101b | **0x15** | 终止电流使能+值；**[3:0]=0101=125mA** |
| **0x64** | §6.13.2.62 | p.48 | **CV charger voltage** | [2:0] RW, [7:3] RO | **011b** | **0x03** | 充电截止电压查表；**011=4.2V** |
| 0x65 | §6.13.2.63 | p.48 | Thermal regulation | [1:0] RW, [7:2] RO | — | — | 温度调节阈值 |
| 0x67 | §6.13.2.64 | p.49 | CHG timeout | [7:0] RW | — | — | 充电超时 |
| 0x68 | §6.13.2.65 | p.49 | BATT detect ctrl | [7:0] RW | — | — | 电池检测 |

---

## 寄存器表（0x80–0x9A，电源输出）

| Reg | Section | Page | 名称 | Bits (R/W) | Default | Init | 说明 |
|---|---|---|---|---|---|---|---|
| 0x80 | §6.13.2.68 | p.50 | DCDC EN ctrl | [7:0] RW | — | — | DCDC 使能控制 |
| 0x82 | §6.13.2.70 | p.50 | DCDC1 voltage | [6:0] RW | — | — | 0.5+N×0.05V (≤1.2V) or 1.22+N×0.02V |
| 0x90 | §6.13.2.75 | p.53 | **LDOS ON/OFF ctrl 0** | [7:0] RW | — | **0x02** (RMW) | LDO 使能；[1]=ALDO2, [2]=ALDO3, [3]=ALDO4 |
| 0x91 | §6.13.2.76 | p.53 | LDOS ON/OFF ctrl 1 | [7:0] RW | — | — | BLDO/CPUSLDO/DLDO 使能 |
| 0x92 | §6.13.2.77 | p.54 | ALDO1 voltage | [4:0] RW | EFUSE | — | 0.5+N×0.1V, 0.5-3.5V |
| **0x93** | §6.13.2.78 | p.54 | **ALDO2 voltage** | [4:0] RW | EFUSE | **0x1C** | 0.5+N×0.1V；**0x1C=28→3.3V** |
| — | §6.13.2.79 | p.54 | ALDO3 voltage | [4:0] RW | EFUSE | — | 0.5+N×0.1V → 3.3V (通过 API) |
| **0x95** | §6.13.2.80 | p.54 | **ALDO4 voltage** | [4:0] RW, [7:5] RO | EFUSE | **0x1D** (API) | 0.5+N×0.1V；**0x1D=29→3.4V** |
| 0x96–0x9A | §6.13.2.81-85 | p.54-55 | BLDO1/2, CPUSLDO, DLDO1/2 | [4:0] RW | EFUSE | — | 其他 LDO 电压 |

---

## 寄存器表（0xA0+，库仑计）

| Reg | Section | Page | 名称 | Bits | Default | 说明 |
|---|---|---|---|---|---|---|
| 0xA1 | §6.13.2.86 | p.55 | Battery param | mixed | — | 电池参数配置 |
| 0xA2 | §6.13.2.87 | p.55 | Fuel gauge ctrl | mixed | — | 库仑计控制 |
| 0xA4 | §6.13.2.88 | p.55 | Battery percentage | RO | — | 电量百分比 |

---

## 关键常量速查

| 常量 | 值 | 含义 |
|---|---|---|
| CHIP_ID | 0x4A | AXP2101 设备 ID |
| I2C 地址 | 0x34 | AXP2101 I2C 7-bit 地址 |
| ALDO 电压公式 | V = 0.5 + N×0.1 | 0.5V–3.5V |
| DCDC1 电压公式 | V = 0.5 + N×0.05 (N≤14) 或 1.22 + N×0.02 | 0.5V–3.4V |
| 充电电压 LUT | 000=5.0V, 001=4.0V, 010=4.1V, **011=4.2V**, 100=4.35V, 101=4.4V | Reg 0x64 [2:0] |
| 预充电流公式 | I = 25×N mA (N=0-8) | Reg 0x61 [3:0] |
| 恒流充电公式 | I = 25×N (N≤8) 或 200+100×(N-8) (N>8) | Reg 0x62 [4:0] |

---

## 本项目 init table 值汇总

| Reg | Val | 功能 | 解码 |
|---|---|---|---|
| 0x22 | 0x06 | PWROFF_EN | DIE OT + EFUSE |
| 0x27 | 0x10 | IRQ/OFF/ON | IRQ=1.5s, OFF=4s, ON=EFUSE |
| 0x93 | 0x1C | ALDO2 voltage | 3.3V |
| 0x90 | 0x02 (RMW) | LDO enable | ALDO2 OR enable |
| 0x64 | 0x03 (RMW) | Charge voltage | 4.2V |
| 0x61 | 0x05 | Precharge current | 125mA |
| 0x62 | 0x0A | Constant charge current | 400mA |
| 0x63 | 0x15 | Termination current | enable + 125mA |
| 0x14 | 0x00 | Min system Vsys DPM | 4.1V |
| 0x15 | 0x00 | Input voltage limit | 3.88V |
| 0x16 | 0x05 | Input current limit | 2000mA |
| 0x24 | 0x01 | PWROFF VSYS threshold | 2.7V |
| 0x50 | 0x14 | TS pin ctrl | TS 固定输入, 20uA |

---

**下次更新**：代码注释修正（0x14/0x16）后同步更新
