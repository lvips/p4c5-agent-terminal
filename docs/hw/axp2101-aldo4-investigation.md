# AXP2101 ALDO4 电压寄存器调研报告

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）— T12 调研
> **目标**：定位 ALDO4 寄存器，计算 3.4V 写入值，给 CCA 一行改代码指南
> **前置**：M10 确认 ALDO4=2.9V 导致 ML307C 拨号失败（R4 🔴 阻塞）

---

## 1. 概述

M10 实测发现 P4C5 的 AXP2101 ALDO4 输出 2.9V，而 ML307C 4G 模组规格书要求 VBAT ≥ 3.4V。模组上电后 UART 波特率检测循环失败（10s 超时），根因是供电不足。本报告定位 ALDO4 电压寄存器，计算 3.4V 对应写入值，列出所有代码修改点和副作用。

---

## 2. AXP2101 ALDO4 寄存器规格

### 2.1 寄存器定义

**来源**：AXP2101 规格书 Rev1.4, p.54, §6.13.2.80

| 项 | 值 |
|---|---|
| **寄存器地址** | **0x95** |
| **寄存器名称** | ALDO4 voltage setting |
| **Access** | R/W |
| **Reset value** | EFUSE（芯片熔丝默认值） |
| **位定义** | **[4:0]** — aldo4 output voltage configuration |
| | [7:5] — RO, 始终读回 0 |
| **电压范围** | 0.5V ~ 3.5V |
| **步进** | **100mV/bit**（线性编码） |
| **步数** | 31 steps |
| **编码公式** | `V = 0.5 + value × 0.1`（V 单位：V）|
| **反向公式** | `value = (V - 0.5) / 0.1` |

### 2.2 编码表（部分）

| [4:0] 二进制 | 十进制 | 电压 | 用途 |
|---|---|---|---|
| 00000 | 0 | 0.5V | 最小 |
| 01001 | 9 | 1.4V | — |
| 10001 | 17 | 2.2V | — |
| **11000** | **24** | **2.9V** | **当前值** |
| **11101** | **29** | **3.4V** | **目标值** ✅ |
| 11110 | 30 | 3.5V | ALDO4 最大 |
| 11111 | 31 | reserved | — |

### 2.3 关联寄存器

| 地址 | 名称 | 功能 | 与 ALDO4 关系 |
|---|---|---|---|
| **0x95** | ALDO4 voltage | 电压设定 | **直接控制** |
| 0x90 [bit 3] | LDOS ON/OFF control 0 | ALDO4 使能 | 必须先使能再设电压 |
| 0x34/0x35 | VBAT ADC | 电池电压测量 | 无直接影响 |

**来源**：规格书 p.53, §6.13.2.75 REG 90:
```
bit[3]: aldo4 enable
  0: disable
  1: enable
  Reset: EFUSE
```

---

## 3. 3.4V 对应值计算

### 3.1 公式推导

```
V = 0.5 + value × 0.1
3.4 = 0.5 + value × 0.1
value = (3.4 - 0.5) / 0.1 = 2.9 / 0.1 = 29
```

### 3.2 结果

| 表示法 | 值 |
|---|---|
| 十进制 | **29** |
| 十六进制 | **0x1D** |
| 二进制 | **11101** |
| float 参数 | **3.4f** |

### 3.3 验证

```
0.5 + 29 × 0.1 = 0.5 + 2.9 = 3.4V ✅
0x1D = 0b11101 → [4:0] = 11101 = 29 ✅
```

---

## 4. 修改方案

### 4.1 需要修改的代码位置（共 3 处 + 1 常量）

| # | 文件 | 行号 | 当前值 | 目标值 | 说明 |
|---|---|---|---|---|---|
| 1 | `p4c5_pmic.cc` | L307 | `2.9f` | **`3.4f`** | PMIC 初始化 ALDO4 电压 |
| 2 | `p4c5_pmic.cc` | L488 | `2.9f` | **`3.4f`** | 4G 电源开启时重设电压 |
| 3 | `p4c5_pmic.cc` | L493 | `"ALDO4=2.9V"` | **`"ALDO4=3.4V"`** | 日志文案 |
| 4 | `p4c5_pmic.cc` | L311 | `"2.9V"` | **`"3.4V"`** | 初始化日志文案 |
| 5 | `ksdiy_lvgl_port.c` | L181 | `2.9f` | **`3.4f`** | Display 组件也写了 ALDO4 |
| 6 | `config.h` | L63 | `2900` | **`3400`** | 常量定义（当前未被引用） |
| 7 | `p4c5_board.h` | L12 | 注释 | 更新 | 文档注释 |
| 8 | `p4c5_4g.h` | L9 | 注释 | 更新 | 风险标注注释 |
| 9 | `p4c5_pmic.h` | L12-15, L117-118 | 注释 | 更新 | 头文件文档 |

### 4.2 最小修改（仅 p4c5_pmic.cc）

```c
// p4c5_pmic.cc — 改动 1 行（L307）：
// 修改前：
ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f),
                    TAG, "ALDO4 voltage failed");

// 修改后：
ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 3.4f),
                    TAG, "ALDO4 voltage failed");
```

### 4.3 推荐修改（全部位置）

```c
// p4c5_pmic.cc — L307 和 L488：2.9f → 3.4f
// ksdiy_lvgl_port.c — L181：2.9f → 3.4f
// config.h — L63：2900 → 3400
// 所有日志/注释：2.9V → 3.4V
```

### 4.4 底层寄存器操作（CCA 如需直接 I2C 写入）

```c
// 直接 I2C 写入 reg 0x95 = 0x1D (3.4V)
uint8_t buf[2] = { 0x95, 0x1D };
i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(100));

// 或使用封装 API：
axp2101_set_aldo4_voltage(0x1D);  // 0x1D = 29 = 3.4V

// 或使用 float API：
axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 3.4f);
// 内部计算: reg_val = (3.4 - 0.5) / 0.1 = 29 = 0x1D
// 然后调用 axp2101_set_aldo4_voltage(0x1D)
```

---

## 5. 副作用清单

| # | 副作用 | 概率 | 影响 | 缓解措施 |
|---|---|---|---|---|
| 1 | ALDO4 升压影响其他电压轨 | 🟢 极低 | 各 LDO 独立调节，互不影响 | 验证其他电压读数 |
| 2 | 修改时序问题 | 🟢 低 | ALDO4 已在 enable 之前设电压 | 保持现有顺序不变 |
| 3 | ML307C 上电冲击电流增大 | 🟡 中 | 3.4V vs 2.9V 压差更大，PA 发射时瞬态电流可能增大 | 观察 ALDO4 电压跌落 |
| 4 | ALDO4 接近上限（3.5V max） | 🟡 中 | 3.4V 距上限仅 100mV，余量较小 | 确认 ML307C 不需 > 3.5V |
| 5 | `ksdiy_lvgl_port.c` 重复写 ALDO4 | 🟡 中 | Display 组件和 PMIC 组件都设 ALDO4，可能竞态 | 两处都改为 3.4f |
| 6 | 电池充电时 ALDO4 波动 | 🟢 低 | 充电不影响 ALDO4 输出 | 充电时实测 ALDO4 电压 |

### 5.1 ML307C 电压范围 vs ALDO4 上限

| 参数 | ML307C | AXP2101 ALDO4 | 余量 |
|---|---|---|---|
| 最小工作电压 | 3.4V | 0.5V | — |
| 典型工作电压 | 3.8V | — | ⚠️ ALDO4 达不到 3.8V |
| 最大工作电压 | 4.4V | 3.5V | — |
| **PA 发射峰值** | 可能 > 4V | **3.5V (max)** | **⚠️ 注意** |

**风险**：ALDO4 最大只能输出 3.5V，而 ML307C 标称工作电压到 4.4V。3.4V 满足**最低要求**，但 PA 发射瞬间可能需要更高电压。ML307C 内部可能有 DC-DC 升压电路，需实测确认 3.4V 下能否正常发射。

### 5.2 ksdiy_lvgl_port.c 重复写入问题

`ksdiy_lvgl_port.c` L175-183 也配置了 AXP2101 电源轨（DCDC1, ALDO1, ALDO3, ALDO4），这与 `p4c5_pmic.cc` 的初始化**功能重叠**。两处都写 ALDO4 为 2.9f：

```
初始化顺序（app_main.c）：
  [0/5] Board init → I2C 总线
  [1/5] PMIC init (p4c5_pmic.cc) → ALDO4 = 2.9f
  [2/5] Display init (ksdiy_lvgl_port.c) → ALDO4 = 2.9f (重复写入)
  ...
```

Display init 在 PMIC init 之后执行，如果 Display 组件独立配置电源轨，它会**覆盖** PMIC 组件的设定。两处都必须改为 3.4f，否则 Display init 会把 ALDO4 改回 2.9V。

---

## 6. 验收标准

| # | 验收项 | 方法 | 期望 |
|---|---|---|---|
| 1 | ALDO4 实测电压 | 万用表测 ALDO4 引脚 | ≥ 3.4V（允许 ±5% = 3.23-3.57V） |
| 2 | ML307C AT 响应 | `AT\r\n` 串口命令 | `OK` 响应 |
| 3 | 波特率检测 | 固件日志 | `Detecting baud rate...` 后成功识别 |
| 4 | 13 个其他寄存器 | `p4c5_pmic_dump_all_regs()` | 值不受影响 |
| 5 | DCDC1/ALDO1/ALDO3 | 万用表 + 日志 | 3.3V/1.8V/3.3V 不变 |
| 6 | ML307C 网络注册 | `AT+CREG?` | `+CREG: 0,1`（已注册） |
| 7 | DSH WebSocket 连接 | 固件日志 | `🟢 DSH connected` |
| 8 | PA 发射时 ALDO4 跌落 | 示波器 | 跌落 < 0.3V（不低于 3.1V） |

---

## 7. R4 风险状态更新

| 阶段 | 状态 | 描述 |
|---|---|---|
| M5 | ⚠️ 待实测 | ALDO4=2.9V 标注为 R2.1 风险 |
| M10 | 🔴 确认阻塞 | ML307C 波特率检测 10s 超时，根因 ALDO4 不足 |
| **T12** | 🎯 **方案就绪** | **ALDO4 → 3.4V（reg 0x95 = 0x1D），3 处代码修改** |
| M12 | ⏳ 待验证 | 升压后重测 ML307C + DSH 连接 |

### R4 → R12 追踪

| 风险项 | M10 | T12 | M12（预期） |
|---|---|---|---|
| R4: ALDO4 电压不足 | 🔴 阻塞 |  方案已定 | ✅ 已修复 |
| 关联: ML307C 拨号失败 | 🔴 阻塞 | — | ✅ 预期通过 |
| 关联: DSH 连接 | 🔴 阻塞（依赖 4G） | — | ✅ 预期通过 |

---

## 8. 完整电压对照表（ALDO 系列）

所有 ALDO/BLDO 寄存器使用相同编码（0.5V-3.5V, 100mV/step），仅地址不同：

| LDO | 寄存器 | 当前用途 | 当前电压 | 当前寄存器值 |
|---|---|---|---|---|
| ALDO1 | 0x92 | 辅助 | 1.8V | 0x0D (13) |
| ALDO2 | 0x93 | xiaozhi 兼容（~1mA） | 3.3V | 0x1C (28) |
| ALDO3 | 0x94 | 音频 codec | 3.3V | 0x1C (28) |
| **ALDO4** | **0x95** | **4G 模组 VBAT** | **2.9V → 3.4V** | **0x18 → 0x1D** |
| BLDO1 | 0x96 | 未用 | — | EFUSE |
| BLDO2 | 0x97 | 未用 | — | EFUSE |
| CPUSLDO | 0x98 | CPU 内核 | — | EFUSE |
| DLDO1 | 0x99 | 未用 | — | EFUSE |
| DLDO2 | 0x9A | 未用 | — | EFUSE |

**注**：CPUSLDO (0x98) 编码不同 — 0.5V-1.4V, 50mV/step, 20 steps（规格书 p.55, §6.13.2.83）。

---

## 9. 给 CCA 的一行修改指南

**最快修改**（仅改 p4c5_pmic.cc 两处）：

```diff
--- hardware/p4c5-agent-terminal/components/p4c5_pmic/p4c5_pmic.cc
+++ hardware/p4c5-agent-terminal/components/p4c5_pmic/p4c5_pmic.cc
@@ -304,7 +304,7 @@
-    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f),
+    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 3.4f),
@@ -485,7 +485,7 @@
-        err = axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f);
+        err = axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 3.4f);
```

**同时修改 ksdiy_lvgl_port.c**：

```diff
--- hardware/p4c5-agent-terminal/components/lvgl_st7102_display/ksdiy_lvgl_port.c
+++ hardware/p4c5-agent-terminal/components/lvgl_st7102_display/ksdiy_lvgl_port.c
@@ -178,7 +178,7 @@
-    ESP_ERROR_CHECK(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f));
+    ESP_ERROR_CHECK(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 3.4f));
```

---

**版本**：v1.0（2026-09-06, CCB T12）
**数据来源**：
- AXP2101 规格书 Rev1.4: `docs/hw/datasheets/C3036461_*.PDF` p.53-54 (§6.13.2.75-80)
- 代码: `pmic_axp2101/axp2101_registers.h` L108-116, L2034-2053
- 代码: `p4c5_pmic/p4c5_pmic.cc` L302-311, L478-498
- 代码: `lvgl_st7102_display/ksdiy_lvgl_port.c` L170-185
- 代码: `main/config.h` L63
- M10 日志: `/tmp/m10_log.txt`
