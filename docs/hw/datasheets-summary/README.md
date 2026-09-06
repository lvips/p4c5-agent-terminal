# Datasheets-Summary 操作手册

> **版本**：v1.0（2026-09-06，CCB T13 建立）
> **用途**：规范 datasheets-summary 文档的撰写、更新和交叉引用流程

---

## 1. 目录结构

```
docs/hw/datasheets-summary/
├── README.md              ← 本文件（操作手册 + 索引）
├── AXP2101-REG-MAP.md     ← AXP2101 寄存器完整速查表
├── pmic-axp2101.md        ← AXP2101 PMIC 调研报告
├── ml307c/                ← ML307C AT 命令手册（14 份 PDF）
├── ml307c.md              ← ML307C 4G 模组调研报告
├── peripherals.md         ← 外设汇总（LSM6DS3, WS2812, ST7123, RS485, MCP4725, CH343P, SD, 摄像头）
├── audio-codecs.md        ← 音频 codec（ES8311 + ES7210 + NS4150B）
├── p4c5-board.md          ← P4C5 开发板本体
├── esp32-p4.md            ← ESP32-P4 主控（待创建）
├── esp32-c5.md            ← ESP32-C5 RISC-V 协处理器（待创建）
└── ...（其他模块）
```

---

## 2. 撰写规范

### 2.1 必须包含的要素

每份调研报告必须包含：

1. **规格书来源**：文件名 + 页码 + 章节编号（如 `C3036461_*.PDF p.48 §6.13.2.62`）
2. **验证状态**：✅ 已验证 / 🟡 待验证 / ❌ 未核对
3. **代码引用**：具体文件名 + 行号（如 `p4c5_pmic.cc L308`）
4. **风险标注**：每个风险有 ID、状态、来源
5. **≤ 500 字**（信息密度 ≥ 80%）

### 2.2 禁止的行为（反模式）

| 反模式 | 说明 | 正确做法 |
|---|---|---|
| 盲抄 xiaozhi 值 | M12 教训：直接复制 ALDO4=2.9V 未查 ML307C 规格书 | **双源核对**：代码值 + 下游器件规格书 |
| 无页码引用 | "来自规格书"但不标页码 | 必须标注 `p.XX §X.X.X` |
| 注释当真理 | 把代码注释当作寄存器功能描述 | 以规格书 register description 为准 |
| 未标注验证状态 | 读者无法判断数据可靠性 | 每个寄存器标注 ✅/🟡/❌ |

---

## 3. 更新流程

### 3.1 触发条件

- 新模块引入（如新增 ST7123 → 更新 peripherals.md）
- 寄存器核对完成（如 T11 0x64 核对 → 更新 pmic-axp2101.md）
- 风险修复（如 T12 ALDO4 → 更新 pmic-axp2101.md + ml307c.md）
- 注释/代码修正（如 ST7123 地址勘误 → 更新 peripherals.md）

### 3.2 步骤

```
1. 读规格书 PDF → 定位页码和章节
2. 读代码 → 确认当前值和行号
3. 写/更新调研报告 → 包含页码引用
4. 更新交叉引用 → 关联文档互相引用
5. 更新本 README.md 索引（如有新文件）
6. Commit（只动 docs/hw/datasheets-summary/）
```

---

## 4. Commit 模板

```
docs: update datasheets-summary with page refs (T13)

- pmic-axp2101.md: add register page/section refs, update ALDO4 to 3.4V
- ml307c.md: fix VBAT range 3.4-4.2V → 3.4-4.5V per datasheet p.11
- peripherals.md: add ST7123 section (I2C 0x55, not 0x5A)
- README.md: new operation manual
- AXP2101-REG-MAP.md: new full register map with page refs

Sources:
- AXP2101 datasheet Rev1.4: C3036461_*.PDF p.30-56
- ML307C datasheet: C48982539_*.PDF p.11
- Code: p4c5_pmic.cc, ksdiy_lvgl_port.c, p4c5_display.cc
```

---

## 5. 文件索引

| 文件 | 模块 | 关键数据 | 最后更新 |
|---|---|---|---|
| pmic-axp2101.md | AXP2101 PMIC | 13 寄存器 + xiaozhi 对照 | T14 (v2.1) |
| ml307c.md | ML307C 4G | VBAT 3.4-4.5V, ALDO4 适配 | T13 (v2.0) |
| peripherals.md | 外设汇总 | ST7123(0x55), LSM6DS3(0x6A), MCP4725(0x60) | T13 (v2.0) |
| AXP2101-REG-MAP.md | AXP2101 寄存器速查 | 全 0x00-0xA4 寄存器 + 页码 | T13 (v1.0) |
| audio-codecs.md | 音频 | ES8311(0x18) + ES7210(0x40) + NS4150B | v1.0 |
| p4c5-board.md | 开发板 | P4C5 板级规格 | v1.0 |
| esp32-p4.md | ESP32-P4 | 主控规格（待创建）| — |
| esp32-c5.md | ESP32-C5 | RISC-V 协处理器（待创建）| — |

---

## 6. 关联文档

| 文档 | 路径 | 关系 |
|---|---|---|
| 硬件规格书 | `docs/hw/p4c5-spec.md` | 总规格 |
| 引脚定义 | `docs/hw/p4c5-pins.csv` | 引脚映射 |
| AXP2101 0x64 调查 | `docs/hw/axp2101-0x64-bug-investigation.md` | T11 详细分析 |
| ALDO4 调查 | `docs/hw/axp2101-aldo4-investigation.md` | T12 详细分析 |
| **xiaozhi vs p4c5 对比** | **`docs/hw/xiaozhi-power-comparison.md`** | **T14 完整对比** |
| PMIC 审计 | `docs/hw/pmic-registers-audit.md` | T3 原始审计 |

---

**维护者**：CCB（科研助理）
**下次审阅**：M12 实机验证后
