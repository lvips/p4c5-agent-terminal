# xiaozhi vs p4c5 电源配置对比

> **版本**：v1.0（2026-09-06，CCB T14）
> **xiaozhi 参考**：`/tmp/p4c5_xiaozhi/main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc`（310 行）
> **p4c5 实现**：`components/p4c5_pmic/p4c5_pmic.cc` + `components/p4c5_4g/p4c5_4g.cc`
> **结论**：13 寄存器完全一致，差异在 ALDO4 电压 + DTR + 架构

---

## 1. 调研背景

DSH 排查 ML307C 拨号失败时，对比 xiaozhi 官方参考实现与 p4c5 当前实现。
xiaozhi 是 Kevin 维护的开源参考板级代码（已验证），p4c5 是本项目移植实现。

---

## 2. PMIC 寄存器配置对比

### 2.1 完整对比表

> 规格书来源：AXP2101 Rev1.4 `C3036461_*.PDF`，章节 §6.13.2.x

| # | Reg | 章节 | 页码 | 功能 | xiaozhi | p4c5 | 差异 |
|---|---|---|---|---|---|---|---|
| 1 | 0x22 | §6.13.2.19 | p.36 | PWROFF_EN | 0x06 | 0x06 | ✅ 一致 |
| 2 | 0x27 | §6.13.2.24 | p.38 | IRQ/OFF/ON level | 0x10 | 0x10 | ✅ 一致 |
| 3 | 0x93 | §6.13.2.78 | p.54 | ALDO2 voltage (3.3V) | 0x1C | 0x1C | ✅ 一致 |
| 4 | 0x90 | §6.13.2.75 | p.53 | LDO enable (ALDO2 OR) | RMW 0x02 | RMW 0x02 | ✅ 一致 |
| 5 | 0x64 | §6.13.2.62 | p.48 | Charge voltage (4.2V) | 0x03 | 0x03 | ✅ 一致 |
| 6 | 0x61 | §6.13.2.59 | p.47 | Precharge current (125mA) | 0x05 | 0x05 | ✅ 一致 |
| 7 | 0x62 | §6.13.2.60 | p.48 | Constant charge (400mA) | 0x0A | 0x0A | ✅ 一致 |
| 8 | 0x63 | §6.13.2.61 | p.48 | Termination current | 0x15 | 0x15 | ✅ 一致 |
| 9 | 0x14 | §6.13.2.10 | p.33 | Min Vsys DPM (4.1V) | 0x00 | 0x00 | ✅ 一致 |
| 10 | 0x15 | §6.13.2.11 | p.33 | Input voltage limit (3.88V) | 0x00 | 0x00 | ✅ 一致 |
| 11 | 0x16 | §6.13.2.12 | p.34 | Input current limit (2000mA) | 0x05 | 0x05 | ✅ 一致 |
| 12 | 0x24 | §6.13.2.21 | p.37 | PWROFF VSYS threshold (2.7V) | 0x01 | 0x01 | ✅ 一致 |
| 13 | 0x50 | §6.13.2.46 | p.45 | TS pin ctrl | 0x14 | 0x14 | ✅ 一致 |

**结论**：13 个寄存器**完全一致**，p4c5 忠实移植了 xiaozhi 的寄存器配置。

### 2.2 电压轨配置对比

| 电压轨 | xiaozhi | p4c5 | 差异 |
|---|---|---|---|
| DCDC1 | 3.3V | 3.3V | ✅ 一致 |
| ALDO1 | 1.8V | 1.8V | ✅ 一致 |
| ALDO3 | 3.3V | 3.3V | ✅ 一致 |
| **ALDO4** | **2.9V** | **3.4V** | ❌ **不同**（p4c5 T12 修复）|
| ALDO2 | 3.3V（兼容）| 3.3V（兼容）| ✅ 一致 |

### 2.3 ALDO4 差异详解

| 项 | xiaozhi | p4c5 |
|---|---|---|
| 电压值 | 2.9V (reg 0x95 = 0x18) | 3.4V (reg 0x95 = 0x1D) |
| 计算 | N = (2.9-0.5)/0.1 = 24 | N = (3.4-0.5)/0.1 = 29 |
| 依据 | xiaozhi 原始值 | ML307C 规格书 p.11: VBAT ≥ 3.4V |
| 验证 | ✅ xiaozhi 实测可用 | 🟡 M12 待实测 |
| 来源 | `kevin_p4c5_4g_board.cc` L115 | `p4c5_pmic.cc` L308 |

**风险**：xiaozhi 使用 2.9V 且被验证可用，但 ML307C 规格书明确要求 ≥ 3.4V。
**可能解释**：
1. ML307C 内部有 DC-DC 升压，2.9V 输入仍能工作
2. 2.9V 是待机电压，发射时 PA 有升压电路
3. Kevin 的 xiaozhi 版本可能使用不同批次 ML307C（容忍更低电压）

**建议**：M12 实机对比测试 2.9V vs 3.4V 拨号成功率，以实测数据为准。

---

## 3. 4G 启动模式对比

### 3.1 POWER GPIO

| 项 | xiaozhi | p4c5 | 差异 |
|---|---|---|---|
| 引脚 | GPIO4 | GPIO4 | ✅ 一致 |
| 模式 | `gpio_set_level(GPIO4, 1)` | `gpio_set_level(GPIO4, 1)` | ✅ 一致 |
| 时序 | 直接拉高 | 直接拉高 | ✅ 一致 |
| 宏定义 | `ML307_POWER_OUTPUT_INVERT = false` | 硬编码 1 | 🟡 风格差异 |

**代码对照**：

```c
// xiaozhi: kevin_p4c5_4g_board.cc L71-81
void Enable4GModule() {
    gpio_config_t cfg = { .pin_bit_mask = (1ULL << ML307_POWER_PIN), ... };
    gpio_config(&cfg);
    gpio_set_level(ML307_POWER_PIN, ML307_POWER_OUTPUT_INVERT ? 0 : 1);
    // ML307_POWER_PIN = GPIO4, ML307_POWER_OUTPUT_INVERT = false → 拉高
}

// p4c5: p4c5_4g.cc L59-75
static esp_err_t power_on(void) {
    gpio_config_t cfg = { .pin_bit_mask = (1ULL << P4C5_4G_PWR_GPIO), ... };
    gpio_config(&cfg);
    gpio_set_level(P4C5_4G_PWR_GPIO, 1);  // 直接拉高
}
```

**结论**：两者都是**直接拉高 GPIO4**，无 PWRKEY 时序差异。功能完全一致。

### 3.2 DTR Pin

| 项 | xiaozhi | p4c5 | 差异 |
|---|---|---|---|
| DTR GPIO | 无（-1）| GPIO51 | ❌ **p4c5 新增** |
| 用途 | — | 低功耗睡眠控制 | p4c5 增强 |
| API | — | `p4c5_4g_sleep()` / `p4c5_4g_wake()` | p4c5 增强 |

**影响**：DTR 是 p4c5 的增强功能，不影响基本拨号。xiaozhi 没有 DTR 也能正常拨号。

### 3.3 运行时 4G 电源控制

| 项 | xiaozhi | p4c5 |
|---|---|---|
| ALDO4 运行时控制 | 无（启动时固定 2.9V）| `p4c5_pmic_set_4g_power(bool)` |
| 可断电 |  | ✅ ALDO4 可独立关闭（~15mA 节省）|

**结论**：p4c5 增强了电源管理，支持运行时 ALDO4 开关。这是 p4c5 的**优势**而非偏差。

---

## 4. 启动顺序对比

### 4.1 xiaozhi（单构造函数，集中式）

```
KevinP4c54gBoard()
  1. InitializeI2c()            → I2C 总线
  2. InitializePmicRails()      → DCDC1/ALDO1/ALDO3/ALDO4 + 50ms
  3. new Pmic(i2c_bus, 0x34)    → 13 寄存器写入
  4. InitializeSt7102Display()  → LCD + Touch (含 PMIC 重复配置)
  5. InitializeCamera()         → 摄像头（如启用）
  6. Enable4GModule()           → GPIO4 拉高
  7. InitializeButtons()        → 按键
  8. GetBacklight()->Restore()  → 背光恢复
```

### 4.2 p4c5（多组件，模块化）

```
app_main()
  [0/5] p4c5_board_init()      → I2C 总线
  [1/5] p4c5_pmic_init()        → DCDC1/ALDO1/ALDO3/ALDO4(3.4V) + 50ms
                                  → 13 特殊寄存器 + 验证
  [2/5] p4c5_display_init()     → LCD + Touch（含 PMIC 重复配置）
  [3/5] p4c5_audio_init()       → ES8311 + ES7210
  [4/5] p4c5_4g_init()          → GPIO4 拉高 + DTR + Detect
  [5/5] DSH client init         → WebSocket 连接
```

### 4.3 差异分析

| 维度 | xiaozhi | p4c5 | 影响 |
|---|---|---|---|
| 架构 | 单构造函数 | 模块化组件 | p4c5 更清晰 |
| PMIC 13 寄存器 | Pmic 构造函数 | p4c5_pmic.cc init table | ✅ 值一致 |
| ALDO4 电压 | 2.9V | 3.4V | ❌ **关键差异** |
| 寄存器验证 | 无 | 读回验证 | p4c5 增强 |
| Display 重复配 PMIC | ✅（ksdiy_lvgl_port.c）| ✅（同文件）| ⚠️ 两者都有 |
| 4G 启动时机 | 构造函数第 6 步 | 组件 [4/5] | 🟡 时序略不同 |

---

## 5. 风险标注与修复优先级

| # | 差异 | 风险 | 建议 | 优先级 |
|---|---|---|---|---|
| 1 | ALDO4 2.9V vs 3.4V | 🟡 中 | M12 实测两种电压拨号成功率 | **P1** |
| 2 | Display 重复写 ALDO4 | 🟡 中 | 两处都改同值，避免竞态 | **P1** |
| 3 | p4c5_board.cc ST7123 地址注释 0x5A |  低 | 修正为 0x55 | P2 |
| 4 | 0x14/0x16 代码注释与规格书不一致 | 🟢 低 | 功能正常，注释待修正 | P3 |
| 5 | DTR GPIO51（xiaozhi 无）|  低 | 增强功能，不影响拨号 | 保留 |

---

## 6. 结论

**13 个 PMIC 寄存器配置完全一致**，p4c5 忠实移植了 xiaozhi 的参考实现。
**唯一实质性差异**是 ALDO4 电压（2.9V → 3.4V），这是基于 ML307C 规格书的修复。
**架构差异**（单构造函数 vs 模块化组件）不影响功能，p4c5 反而增加了寄存器验证和运行时电源管理。

---

**下次更新**：M12 实测 ALDO4 2.9V vs 3.4V 拨号对比后
