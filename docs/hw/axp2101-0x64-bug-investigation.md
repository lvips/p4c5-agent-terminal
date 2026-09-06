# AXP2101 寄存器 0x64 (CHG_VOLTAGE_SETTING) 深度调查报告

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）— T11 深度调查
> **触发事件**：M10 实测发现写入 0x2B 读回 0x03，误判为"充电电压 bug"
> **结论**：❌ **不是 bug，是规格书注释错误导致的误判**。xiaozhi 原代码 0x03 = 4.2V 完全正确。

---

## 1. 问题起源

### 1.1 M10 实测现象

```
I (1793) p4c5_pmic:   [0x64] CHG_VOLTAGE_SETTING: = 0x2B (充电截止电压=4.192V)
W (1802) p4c5_pmic:   ⚠️ R2.2: 🔴 xiaozhi原值0x03(3.55V)已修正为0x2B(4.2V)
W (1908) p4c5_pmic:   ⚠️ [0x64] CHG_VOLTAGE_SETTING: expected 0x2B, got 0x03  ← 验证失败
```

写入 0x2B（二进制 `00101011`）后，I2C 读回始终为 0x03（二进制 `00000011`）。

### 1.2 初始假设链（错误）

基于 `axp2101_registers.h` 的注释（已被 T11 修正）：

| 来源 | 注释内容 | 推论 |
|---|---|---|
| `axp2101_registers.h` L330-333 | `[5:0] 目标电压, 步进 16mV/bit, 3.504V~4.608V` | 0x03 = 3.504+3×16mV = **3.552V** |
| `pmic-registers-audit.md` §3.11 | "xiaozhi 写 0x03 → 3.552V，电池充不满" | 🔴 高风险 |
| `p4c5_pmic.cc` (M10 版本) | 直接写入 0x2B = 3.504+43×16mV = **4.192V** | 修正方案 |

---

## 2. 根因分析

### 2.1 AXP2101 规格书真相（来源：PDF p.48, §6.13.2.62）

**REG 64: CV charger voltage setting**

| Bit | Description | R/W | Reset | Default |
|---|---|---|---|---|
| 7:3 | — | **RO** | / | 0 |
| 2:0 | Charge voltage limit: **000=5.0V, 001=4.0V, 010=4.1V, 011=4.2V, 100=4.35V, 101=4.4V, 11X=reserved** | RW | POR | **011b** |

**关键事实**：
- **只有 3 位 [2:0] 可写**，不是 6 位 [5:0]
- bits [7:3] 是**只读位（RO）**，硬件始终返回 0
- **默认值 = 011b = 4.2V**
- 编码方式为查表（3-bit 枚举），不是线性步进

### 2.2 规格书佐证（来源：PDF p.23, §6.7.1 Charger Characteristics）

> "Target charge voltage settable (VREG, reg64H[2:0]), **default: 4.2V, range: 4.0V/4.1V/4.2V/4.35V/4.4V**"

与 §6.13.2.62 完全一致：3-bit 枚举，默认 4.2V。

### 2.3 误判的根因

**`axp2101_registers.h` 的注释是错的**（已在 T11 由 CCA 修正）：

```c
// ❌ 错误注释（原始版本）：
// [5:0] Target voltage value
//   Step: 16 mV/bit
//   Range: 3.504 V (0x00) to 4.608 V (0x3F)
// [7:6] Reserved

// ✅ 正确注释（T11 修正后）：
// [7:3] Read-only (always 0)
// [2:0] Charge voltage limit:
//   000=5.0V  001=4.0V  010=4.1V
//   011=4.2V  100=4.35V 101=4.4V  11X=reserved
// Default: 011b (4.2V)
```

**错误链**：寄存器头文件注释错误 → audit 报告基于错误注释 → CCA 将 0x03 误判为 3.55V → 修正为 0x2B → 硬件屏蔽高位 → 读回 0x03 → 验证失败。

---

## 3. 五个假设的验证

### 假设 1：硬件屏蔽了写入值的高位 ✅ **确认**

**证据**：写入 0x2B (`00101011`) → 读回 0x03 (`00000011`)

```
0x2B = 0010 1011
        ^^^^ ^^^
        [7:3] [2:0]
硬件屏蔽 [7:3] → 只保留 [2:0] = 011 = 0x03
```

**结论**：AXP2101 的 reg 0x64 [7:3] 是硬件只读位，写入时被忽略，始终返回 0。写入 0x2B 等价于写入 0x03。

### 假设 2：xiaozhi 的 set_charge_voltage() 有 bug ❌ **否定**

**xiaozhi 代码**（`axp2101.c` L604-613）：

```c
esp_err_t axp2101_set_charge_voltage(axp2101_charge_voltage_t voltage) {
    if (voltage < 0x01 || voltage > 0x05) return ESP_ERR_INVALID_ARG;
    uint8_t reg = 0;
    axp2101_get_chg_voltage_setting(&reg);
    reg = (reg & 0xF8) | (uint8_t)voltage;  // 保留 [7:3]，写入 [2:0]
    return axp2101_set_chg_voltage_setting(reg);
}
```

**分析**：
- `(reg & 0xF8)` = 保留 bits [7:3]（只读位，值为 0）
- `| voltage` = 写入 bits [2:0]
- 枚举值 0x01-0x05 对应 [2:0] = 001-101

**结论**：xiaozhi 的位操作**完全正确**，恰好匹配规格书的 3-bit [2:0] 字段。`0xF8` 掩码 = `11111000` 保留了 [7:3]，写入 [2:0]。

### 假设 3：充电状态锁定了寄存器  **否定**

**证据**：
- PMU_STATUS_2 (reg 0x01) [2:0] 充电阶段在 M10 测试期间为 `000`（未充电，无电池）
- 即使有电池充电，规格书未说明充电中锁定 reg 0x64
- 直接 I2C 写入（绕过充电逻辑）仍被屏蔽 → 与充电状态无关

### 假设 4：芯片版本差异导致寄存器定义不同 ❌ **否定**

**证据**：
- AXP2101 规格书 Rev 1.4（2022-10）§6.13.2.62 明确定义 [2:0] 3-bit 字段
- 规格书 Rev 1.4 的修订历史注明 "Update reg 64 in Chapter6.13"
- `axp2101_registers.h` 的 `[5:0]` 注释**不是芯片差异，是文档错误**
- xiaozhi 的驱动代码（`axp2101.c`）使用的是正确的 3-bit 操作

### 假设 5：P4C5 的写路径有 bug ✅ **部分确认**

**M10 版本 p4c5_pmic.cc 的问题**：

```c
// ❌ M10 版本：直接写入 0x2B（is_read_modify_write = false）
{ 0x64, 0x2B, false, "CHG_VOLTAGE_SETTING", ... }

// 验证逻辑（直接写入模式）：
if (actual != expected)  // actual=0x03, expected=0x2B → FAIL
    ESP_LOGW(TAG, "⚠️ expected 0x2B, got 0x03");
```

**问题**：直接写入 0x2B 后，硬件屏蔽高位存储 0x03，但验证逻辑期望读回 0x2B → 误报失败。

**T11 修正**（CCA 已提交）：

```c
// ✅ T11 版本：RMW 模式，写入 0x03
{ 0x64, 0x03, true, "CHG_VOLTAGE_SETTING",
  "充电截止电压=4.2V (bits[2:0]=011b; bits[7:3]硬件只读)",
  "R2.2: ✅ 0x03=4.2V 已是正确值（之前误以为需要 0x2B）" },

// 验证逻辑（RMW 模式）：
if ((actual & 0x03) == 0x03)  // (0x03 & 0x03) == 0x03 → PASS ✅
```

---

## 4. xiaozhi 调用链完整追踪

### 4.1 代码路径

```
kevin_p4c5_4g_board.cc (Pmic 构造函数)
  └─ WriteReg(AXP2101_REG_CHG_VOLTAGE_SETTING, 0x03)  ← 直接 I2C 写入
      └─ i2c_master_transmit(dev, {0x64, 0x03}, 2)
          └─ AXP2101 硬件接收: reg[2:0] = 011 = 4.2V ✅
```

### 4.2 为何 xiaozhi 用 0x03 而非 set_charge_voltage()

xiaozhi 的 `Pmic::Pmic()` 构造函数使用**直接 I2C 写入**（`WriteReg`），而非经过 `axp2101_set_charge_voltage()` 封装层。原因可能是：
1. 构造函数需要批量写入 14 个寄存器，直接 I2C 更高效
2. 避免封装层的 RMW 开销
3. Kevin（xiaozhi 作者）清楚 reg 0x64 的实际位定义

### 4.3 枚举值对照表

| 枚举 | 值 | [2:0] | 电压 | xiaozhi 使用 |
|---|---|---|---|---|
| `AXP2101_CHG_VOLT_4V00` | 0x01 | 001 | 4.0V | — |
| `AXP2101_CHG_VOLT_4V10` | 0x02 | 010 | 4.1V | — |
| `AXP2101_CHG_VOLT_4V20` | **0x03** | **011** | **4.2V** | ✅ **直接写入** |
| `AXP2101_CHG_VOLT_4V35` | 0x04 | 100 | 4.35V | — |
| `AXP2101_CHG_VOLT_4V40` | 0x05 | 101 | 4.4V | — |

---

## 5. 影响评估

### 5.1 对 P4C5 固件的影响

| 项目 | M10 状态 | T11 修正后 |
|---|---|---|
| reg 0x64 实际值 | 0x03 (4.2V) | 0x03 (4.2V) — **无变化** |
| 充电截止电压 | 4.2V ✅ | 4.2V ✅ |
| 验证日志 | ⚠️ expected 0x2B, got 0x03 | ✅ bits OK |
| 电池容量影响 | **无** — 一直是 4.2V | **无** |

**结论**：M10 测试期间，电池充电截止电压**一直是正确的 4.2V**。"读回 0x03"不是 bug，是硬件的正常行为。

### 5.2 对文档的影响

| 文档 | 问题 | 修正 |
|---|---|---|
| `pmic-registers-audit.md` §3.11 | 基于错误注释，判定 0x03=3.55V | 需更新：0x03=4.2V ✅ |
| `M5-verification-report.md` | "0x64=0x2B 生效" | 需标注：硬件屏蔽后=0x03 |
| `M10-e2e-verification-report.md` §6.2 | "回读 0x03 是非阻塞异常" | 需改为：0x03 是正确值 |
| `axp2101_registers.h` | 注释 [5:0] 错误 | ✅ CCA 已修正为 [2:0] |
| `p4c5_pmic.cc` | 直接写 0x2B 验证失败 | ✅ CCA 已修正为 RMW 0x03 |

### 5.3 对其他项目的启示

**OMT/Tab5 项目**（xiaozhi agent 调查）：
- Tab5 **不使用 AXP2101 做充电控制**
- 充电通过 PI4IOE5V6416 IO 扩展器（I2C 0x44）的 P7 引脚控制
- 电池电压通过 INA226（I2C 0x41）监控
- AXP2101 在 Tab5 上仅做电源轨管理，不涉及充电电压设定

---

## 6. 修正后的寄存器 0x64 完整规格

```
AXP2101 REG 0x64: CV Charger Voltage Setting
├─ I2C 地址: 0x34, 寄存器地址: 0x64
├─ 位定义:
│   ├─ [7:3] — RO, 硬件屏蔽, 始终读回 0
│   └─ [2:0] — RW, 充电截止电压（查表编码）
│       ├─ 000 = 5.0V
│       ├─ 001 = 4.0V
│       ├─ 010 = 4.1V
│       ├─ 011 = 4.2V ← 默认值，xiaozhi 使用
│       ├─ 100 = 4.35V
│       ├─ 101 = 4.4V
│       └─ 11X = reserved
─ 默认值: 011b (POR = 4.2V)
├─ 充电规格:
│   ├─ 输入电压: 3.9V~5.5V, PWM charger
│   ├─ 目标电压精度: ±0.5% (@25°C, 4.2V)
│   └─ 充电阶段: 预充 → 恒流 → 恒压 → 完成
└─ 来源: AXP2101 规格书 Rev1.4, p.48 §6.13.2.62 + p.23 §6.7.1
```

---

## 7. 风险状态更新

| 风险 | M10 状态 | T11 更新 |
|---|---|---|
| R2 (AXP2101 寄存器配置) | ⚠️ 0x64 回读异常 | ✅ **已解决**：0x03=4.2V 是正确值 |
| R2.2 (0x64 充电电压) | 🔴 高（以为只充 3.55V） | ✅ **消除**：一直是 4.2V |

---

## 8. 教训与建议

### 8.1 教训

1. **永远以规格书 register description table 为准**，不要依赖 header 注释
2. **写-读不一致不一定是 bug**：可能是硬件屏蔽（RO bits），需要用 RMW 验证
3. **3-bit 枚举 ≠ 6-bit 线性**：reg 0x64 只有 6 个有效值，不是 64 个

### 8.2 给 CCA 的建议

1. ✅ `p4c5_pmic.cc` 的 RMW 修正已正确应用
2. ✅ `axp2101_registers.h` 注释已修正
3. 🟡 建议更新 `pmic-registers-audit.md` §3.11 标注正确解读
4. 🟡 建议更新 `M10-e2e-verification-report.md` §6.2 消除误报

---

**版本**：v1.0（2026-09-06, CCB T11）
**数据来源**：
- AXP2101 规格书 Rev1.4: `docs/hw/datasheets/C3036461_*.PDF` p.23 §6.7.1 + p.48 §6.13.2.62
- xiaozhi 驱动: `pmic_axp2101/axp2101.c` L604-613
- P4C5 驱动: `p4c5_pmic/p4c5_pmic.cc` (T11 修正版)
- M10 实测日志: `/tmp/m10_log.txt`
- Audit 报告: `docs/hw/pmic-registers-audit.md`
- OMT/Tab5 调查: agent "Find xiaozhi axp2101 code"
