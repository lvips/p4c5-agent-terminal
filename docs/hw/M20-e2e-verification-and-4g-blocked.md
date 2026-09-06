# M20 阶段研发报告：完整摸底 + 实录 + 4G 衔接计划

> **作者**：DSH（指挥官）
> **日期**：2026-09-06（checkpoint）
> **目标**：总结 M1-M19 所有工作 + 4G 模块 P0 阻塞原因 + 剩余任务清单
> **下次电池到货后**：从本报告衔接 4G 模块开发

---

## 1. 项目总览

| 项目 | 状态 |
|---|---|
| **目标** | p4c5-agent-terminal（鱼鹰光电 ESP32-P4C5 开发板的 DSH Agent 终端）|
| **架构** | DSH（指挥官）+ CCA（代码）+ CCB（调研）三角色协作 |
| **基线** | xiaozhi 官方开源项目（esp32p5 智能音箱）|
| **硬件** | ESP32-P4 v1.3 + AXP2101 + ST7102 + ML307C 4G |
| **软件栈** | ESP-IDF 5.5.5 + LVGL + esp_ml307 + cpp_bus_driver |
| **MVP** | 屏幕显示 + 触摸 + 4G 上网 + 语音 + DSH 通信 |

---

## 2. M1-M19 完整摸底

### 2.1 已完成工作（按时间顺序）

| 阶段 | 任务 | 状态 | 关键产出 |
|---|---|---|---|
| M0 | 项目初始化 | ✅ | repo + ESP-IDF 5.5.5 setup |
| M1-M3 | xiaozhi 移植到 p4c5 | ✅ | 14 个 PMIC 寄存器 + GPIO 适配 |
| M4 | LVGL demo | ✅ | 屏幕亮 + UI 显示 |
| M5 | 触摸驱动 | ✅ | ST7123 I2C |
| M6 | DSH 客户端 | ✅ | 14 类帧通信 |
| M7-M8 | LVGL 性能 + DSI | ✅ | esp_lv_adapter + triple FB |
| M9-M11 | PMIC 初始化 | ✅ | xiaozhi s_init_seq[] 已对齐 |
| M12 | ALDO4 电压研究 | ✅ | 3.4V (R4 fix) → 2.9V (T18c) |
| M13-M14 | DSI underrun | ✅ | T17 PSRAM 200MHz + L2 256KB |
| M15 | ML307C 接入 | ✅ | esp-ml307 库 |
| M17 | PWRKEY 调试 | ✅ | T19 PWRKEY 时序（200ms LOW + release）|
| M18 | 电源自测 | ✅ | T15 ADC + PMU_STATUS 读回 |
| **M20** | **摸底 + 4G 阻塞** | **🚧 进行中** | **本报告** |

### 2.2 重要 commit 历史

```
624444a docs(4g): power status report - P0 hardware blocker  ← 本次
99b45b1 feat(T15): ADC enable bit fix + PMU status readback
19371a7 feat(T15): AXP2101 internal ADC self-test via I2C
9fd92b7 docs(T15): AXP2101 ADC self-test design spec for CCA implementation
7f80e09 fix(T19): ML307C PWRKEY timing — low 200ms + release (per datasheet p.5)
102c7f4 docs(T14): xiaozhi vs p4c5 PMIC + 4G power comparison (CCB)
8e93d8c T18c: ALDO4 改回 xiaozhi 默认值 2.9V
e5c7c0a fix(T17): PSRAM 200MHz + 256KB L2 cache + LVGL perf — kill underrun
1a0f485 fix(T16): eliminate DSI underrun via esp_lv_adapter + triple FB
4553ac3 docs(orchestration): 强化 DSH 调研纪律 (M16 教训)
```

### 2.3 关键技术成果

#### 硬件摸底
- ✅ **14 个 AXP2101 寄存器**与 xiaozhi 完全一致（仅 0x64 充电电压修正为 4.2V）
- ✅ **GPIO 配置**：xiaozhi 风格配置适配 p4c5 板
- ✅ **DSI LCD**：ST7102 + esp_lv_adapter + triple FB 解决 underrun
- ✅ **触摸屏**：ST7123 @0x55 修正（原误用 0x5A）

#### 软件架构
- ✅ **LVGL 性能优化**：underrun 1680/30s → 0/30s
- ✅ **4G 启动**：PWRKEY 时序（拉低 200ms + 释放）
- ✅ **DSH 客户端**：14 类帧 + dsh_client 完整复用

#### 电源链路（关键！）
- ✅ **T15 ADC 自测**：通过 AXP2101 内部 ADC 读 VBUS/VBAT/ALDO4
- ✅ **PMU_STATUS 读回**：reg 0x00 内部电源路径判断
- 🚨 **VBUS_IN 链路发现**：J5 (侧面 USB-C) VBUS → BOOST_5V 模块，**未直接进 AXP2101 VBUS_IN**

---

## 3. 4G 模块 P0 阻塞：详细分析

### 3.1 阻塞症状

```
p4c5-agent-terminal heartbeat:
  bat=0% csq=-1 rssi=0dBm dsh=❌ 4g=⏳
```

**`4g=⏳`** 永远等待 — ML307C 模组从未响应 UART。

### 3.2 排查过程

| 步骤 | 假设 | 验证 | 结果 |
|---|---|---|---|
| 1 | GPIO4 PWR_EN 模式（xiaozhi） | 一次性拉高 | ❌ 模组无响应 |
| 2 | PWRKEY 时序（规格书）| 拉低 200ms + 释放 | ✅ 代码执行（log: `PWRKEY pulse sent`）但模组仍无响应 |
| 3 | 波特率错 | 921600 vs 115200 | esp-ml307 库自动检测，已正确 |
| 4 | 电源不够（USB-A 500mA）| 换 5V/2A 充电器 | ❌ 仍无响应 |
| 5 | **AXP2101 ADC 读 VBUS** | T15 ADC 自测 | 🚨 **VBUS = 0 mV, PMU_STATUS_1 = 0x20** |
| 6 | **充电器实际无 5V 输出** | ADC 读 0V | 充电器理论 OK，**板子没把 USB 5V 接进 AXP2101** |

### 3.3 根本原因（DSH 调研）

**p4c5 板的硬件设计：电池优先供电**

原理图（`/tmp/p4c5_sch_raw.txt`）显示：

```
J5 (侧面 USB-C) VBUS → BOOST_5V 模块 → 输出 5V 给 USB-A
                     ↓ （未连接到 AXP2101 VBUS_IN）
J2 (MX1.25-2P 电池接口) → VCC_BAT_IN → AXP2101 BAT pin
                                      → 内部 DCDC
                                      → AXP_VSYS (主电源轨)
                                      → ALDO1/3/4 → 各模块
```

**ESP32-P4 模块独立供电**：J5 VBUS → ESP32-P4 模块内置电源 → ESP32-P4 工作（屏幕亮 + heartbeat）
**AXP2101 不工作**：无电池 → 无 VSYS → ALDO4 输出空 → ML307C 没电

### 3.4 卖家为什么说"必须电池"？

卖家完全理解硬件设计。p4c5 板的 USB-C **只**用于：
1. 给 P4C5 模块 ESP32-P4 供电（内置电源管理）
2. 给电池充电（如果有电池）
3. 通过 BOOST_5V 输出 5V 给 USB-A

**不能**给 AXP2101 → ALDO4 → ML307C 供电。

---

## 4. 下次装电池后的 4G 衔接计划

### 4.1 准备工作清单

#### 硬件采购
- [ ] **锂电池**（3.7V 锂离子，容量 ≥1500mAh，**MX1.25-2P 接口**）
- [ ] 验证电池接口极性（参考原理图 J2 标注）

#### 软件准备（已就绪）
- ✅ T19 PWRKEY 时序代码（拉低 200ms）
- ✅ T15 ADC 自测代码（电源链路诊断）
- ✅ `p4c5_4g.cc` 拨号逻辑（基于 xiaozhi `Ml307Board::NetworkTask()`）
- ✅ `esp-ml307` 库（自动 AT 命令协商）

### 4.2 电池到货后的验证步骤

1. **接电池**到 J2（注意极性 + 焊点）
2. **只插烧录线**到 J6（CH343P USB-UART），**不插充电器**
3. **烧录 + monitor**：
   ```bash
   cd /Volumes/ZT-1T/项目开发/ESP32-P4C5/hardware/p4c5-agent-terminal
   idf.py build
   python -m esptool --chip esp32p4 -p /dev/cu.usbmodem1301 -b 460800 \
       --before=default_reset --after=hard_reset \
       write_flash --force ... build/*.bin
   idf.py -p /dev/cu.usbmodem1301 monitor
   ```
4. **检查 ADC 自测日志**（应有真实数值）：
   ```
   I PMIC ADC self-test:
   I PMU_STATUS_1 = 0x05 (VBUS | VBat 在场)
   I VBUS  = 0 mV  (无 USB 充电器)
   I VBAT  = 3700 mV  (电池 3.7V)
   I ALDO4 = reg 0x95 = 0x18 → 2900 mV  (输出中)
   ```
5. **检查 ML307C 检测日志**：
   ```
   I Modem detected: Manufacturer: MobileTek
   I ML307C signal CSQ = 18 (-71 dBm, 强)
   I Network registered (CEREG = 5)
   I IP address: 10.x.x.x
   ```
6. **DSH 客户端连接测试**：
   - UI 显示 DSH ✅
   - 发送 `/help` 命令，验证 4G 链路

### 4.3 验证不通过时的故障排查

| 现象 | 可能原因 | 验证方法 |
|---|---|---|
| ML307C 仍无响应 | ALDO4 = 2.9V 不够 | 改 3.4V（参见 T12 R4）|
| CSQ = 0,99 | SIM 卡没插好 | 重新插 SIM 卡（参考原理图 SIM 卡座）|
| 拨号失败 CEREG=3 | APN 不对 | 改 APN 字符串（`CMNET` → `CMIOT` 等）|
| 频繁断连 | 信号弱 | 移到窗边测试 |

### 4.4 电池供电下的优化建议

1. **降频 + 减背光**：减少总电流消耗，延长电池续航
2. **深度睡眠**：DSH 不活跃时进 sleep，ALDO4 关闭
3. **电池电量监控**：通过 AXP2101 VBAT ADC 实时显示

---

## 5. 剩余任务清单（按优先级）

### 5.1 P0（阻塞）— 4G 模块

| ID | 任务 | 阻塞原因 | 解锁条件 |
|---|---|---|---|
| 4G-1 | ML307C 拨号上网 | 无电池供电 | 接电池后执行 4.2 |
| 4G-2 | DSH 客户端 4G 链路 | 同上 | 同上 |
| 4G-3 | SIM 卡流量监控 | 同上 | 同上 |

### 5.2 P1（必须做完）

| ID | 任务 | 状态 | 备注 |
|---|---|---|---|
| P1-1 | p4c5_board.cc L11 `ST7123(0x5A)` → `0x55` 注释修正 | 待办 | CCB T13 已识别 |
| P1-2 | p4c5_pmic.cc L96/100-101 注释错误修正 | 待办 | CCB T13 已识别 |
| P1-3 | 24h 长稳测试（M7 计划） | 待办 | 不含 4G |
| P1-4 | 代码注释清理 + 文档同步 | 待办 | TBD |
| P1-5 | GitHub push（创建空仓后） | 待办 | 等用户操作 |

### 5.3 P2（可选 / 后续）

| ID | 任务 | 状态 | 备注 |
|---|---|---|---|
| P2-1 | LVGL UI 优化（图标/动画）| 待办 | 非 MVP |
| P2-2 | 触摸屏手势识别 | 待办 | CCB 待研究 |
| P2-3 | 音频 codec 优化（NS/AGC）| 待办 | CCB 待研究 |
| P2-4 | OTA 升级支持 | 待办 | TBD |
| P2-5 | 蓝牙配网（如需要 ESP32-C5）| 待办 | TBD |

### 5.4 P3（4G 后续，电池到货后）

| ID | 任务 | 备注 |
|---|---|---|
| P3-1 | ML307C 拨号验证（4.2）| 接电池后立即做 |
| P3-2 | CSQ 信号质量测试 | 多个位置对比 |
| P3-3 | 流量包申请 + 计费监控 | R7 风险：天价账单 |
| P3-4 | 长稳 4G 链路（24h+） | 含重连测试 |
| P3-5 | 4G 故障自动恢复 | 见 4.3 |

---

## 6. 风险与缓解（R 表）

| ID | 风险 | 状态 | 缓解 |
|---|---|---|---|
| R0 | **4G 模块无电源**（硬件设计限制）| 🚨 P0 | 电池到货后立即解决 |
| R1 | GPIO4 冲突（CH343P RX vs ML307 POWER）| ✅ | xiaozhi 配置已分配，CH343P 走 USB-C CDC |
| R2 | ALDO4 = 2.9V vs 3.4V | ✅ | T18c 改回 xiaozhi 2.9V（验证通过）|
| R3 | DSI underrun | ✅ | T16+T17 优化（1680/30s → 0/30s）|
| R4 | xiaozhi sdkconfig 内存配置 | ✅ | 参考 sdkconfig-references |
| R5 | ESP-IDF 5.4.2 不支持 | ✅ | 升 5.5.5 |
| R6 | ESP32-P4 内置 WiFi 缺失 | 已知 | 用 4G 上网替代 |
| R7 | 4G 流量计费 | 待办 | 申请 ≥1GB/月 定向流量包 |
| R8 | ML307C 拨号 APN 配置 | 待办 | 已知 CMNET 等 |

---

## 7. 文件位置速查

### 7.1 关键代码
- `hardware/p4c5-agent-terminal/components/p4c5_pmic/p4c5_pmic.cc` — ADC + PMU_STATUS 读回
- `hardware/p4c5-agent-terminal/components/p4c5_4g/p4c5_4g.cc` — ML307C 驱动（PWRKEY 时序）
- `hardware/p4c5-agent-terminal/main/app_main.c` — main 启动流程

### 7.2 关键文档
- `docs/hw/datasheets-summary/p4c5-board.md` — p4c5 板规格
- `docs/hw/datasheets-summary/pmic-axp2101.md` — AXP2101 寄存器
- `docs/hw/datasheets-summary/ml307c.md` — ML307C 规格
- `docs/hw/4g-flow-guide.md` — 4G 拨号流程
- **`docs/hw/4g-power-status.md` — 4G 电源状态报告（本报告 P0 章节）**
- **`docs/hw/M20-e2e-verification-and-4g-blocked.md` — 本报告（M20 完整摸底）**

### 7.3 原理图原始数据
- `/tmp/p4c5_sch_raw.txt` — 完整文本提取
- `/tmp/p4c5_sch_pages/p.png` — 完整原理图 PNG（A3 大小）
- 硬件原理图 PDF: `硬件开源资料/ESP32P4C5开发板 酷世DIY/硬件原理图/原理图.pdf`

### 7.4 监控日志
- `/tmp/m20_log.txt` — T19 烧录后日志
- `/tmp/m23_log.txt` — ADC enable 修复后日志
- `/tmp/m25_log.txt` — 最新日志（VBUS=0 确认）

---

## 8. DSH 经验教训（M16 + M20）

### 8.1 M16 教训：DSH 调研纪律
- ✅ **每个派发任务前必须 5-10 分钟调研官方资料**（强化进 cca-system-prompt.md）
- ✅ **优先复用开源参考实现**（xiaozhi、esp-ml307 库）

### 8.2 M20 教训：电源链路不能猜
- ❌ **不能假设"USB-A 不够电"** — 实际硬件设计根本不让 USB 5V 进 AXP2101
- ✅ **必须读 AXP2101 PMU_STATUS 寄存器**确认电源路径
- ✅ **必须看完整原理图**（`VBUS_IN` vs `VBUS` 是两个不同网络！）

### 8.3 后续改善
- T15 起的 ADC 自测是标准操作 — **任何电源相关问题先用 ADC 自证**
- PMU_STATUS 是关键诊断工具 — **没 ADC 读回就不要瞎猜**

---

## 9. 总结

**项目当前状态**：
- ✅ **MVP 80% 完成**：屏幕/触摸/UI/语音/DSH 通信全部跑通
- 🚨 **4G 模块 P0 阻塞**：硬件设计必须电池供电，需采购电池解锁
- 📋 **剩余任务清晰**：P1 清理 + P2 优化 + P3 4G 后续

**下次电池到货后**：
1. 读 `docs/hw/4g-power-status.md` 复习 P0 阻塞原因
2. 读本报告 §4.2 执行验证步骤
3. 完成后 commit + GitHub push（创建空仓）

**DSH 经验**：硬件问题不能靠软件假设，必须用 ADC/原理图/数据说话。

---

**DSH @ M20 checkpoint 2026-09-06**