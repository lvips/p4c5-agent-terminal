# HANDOFF — 项目当前进度

> **版本**：M7 实测阶段（2026-09-06）
> **角色**：DSH → 接手者
> **状态**：M5 启动验证 ✅ → M6 Audio 修复 ✅ → M7 实测模板已就绪，待 CCA T7 填写

---

## 🚀 M5/M6/M7 最新进展 (2026-09-06)

### M5：启动验证 ✅

- ✅ 编译成功：IDF 5.5.5 + `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
- ✅ 启动成功：ESP32-P4 v1.3 chip，无 Guru Meditation panic
- ✅ R8（chip revision 不兼容）P0 阻塞已消除
- ✅ PMIC 14 寄存器写入+读回验证 100% 通过（0x64=0x2B→4.2V 充电修正）
- ✅ Display ST7102 MIPI-DSI 初始化成功，背光 80%
- ❌ Audio init 失败：`ESP_ERR_NO_MEM`（`audio_codec_new_i2s_data`）
- ⊘ 4G 未执行（被 Audio 阻塞）
- 📄 报告：`docs/hw/M5-verification-report.md`（commit `0187ca5`）

### M6：Audio 修复 ✅

- ✅ 根因定位：I2C 地址格式不匹配
  - `esp_codec_dev` 使用旧格式（8-bit 含 R/W 位），内部 `addr >> 1`
  - 我们传 0x18（7-bit），被 shift 成 0x0C → 错误地址 → I2C 失败
- ✅ 修复方案：config.h 中预 shift
  - `P4C5_ES8311_I2C_ADDR = (0x18 << 1) = 0x30`
  - `P4C5_ES7210_I2C_ADDR = (0x40 << 1) = 0x80`
- ✅ Audio init OK：ES8311 DAC + ES7210 ADC 全部初始化成功
- ✅ 所有子系统初始化通过
- 📄 修复 commit：`3b51cd5`

### M7：实测阶段（进行中）

- ✅ 实测数据记录模板已写：`docs/hw/M7-test-log.md`（1854 字，commit `80e5f73`）
  - Audio TC-01~12 测试用例（含填空字段）
  - Display 背光/LVGL/触摸测试
  - PMIC 万用表电压测量表
  - 4G 模块测试（如已启用）
  - Watchdog 测试
  - 风险状态汇总表
- ✅ 24h 长稳测试方案已写：`docs/hw/M7-long-run-plan.md`（1964 字，commit `b8067d8`）
  - 4 阶段测试：播放(6h) → 录音(6h) → 交替(6h) → 双工(6h)
  - 自动监控脚本（heap/PMIC/温度每 60s）
  - 人工检查点（每 6h 万用表 + 红外测温）
  - 异常处理预案 + 失败判定标准
- ⏳ CCA T7 待完成：
  - 填写 M7-test-log.md 实测数据
  - 执行 24h 长稳测试
  - 生成测试报告

### 风险状态（最新）

| 风险 | 等级 | 状态 | 说明 |
|---|---|---|---|
| R1 ES7210 规格书缺失 | ⚠️→✅ | 已消除 | 用 esp_audio_codec 封装 |
| R2 PMIC 14 寄存器 | 🔴 | ✅ 已验证 | 13/13 读回通过，0x64=0x2B 生效 |
| R3 LSM6DS3 版本差异 | ⚠️ | ⊘ 未测试 | IMU 未集成 |
| **R4 ALDO4 vs ML307C** | **⚠️** | **⚠️ 待实测** | ALDO4=2.9V，ML307C 要求 ≥3.4V |
| R5 GPIO4 冲突 | ⚠️ | ✅ 已验证 | 无冲突告警 |
| R6 实测覆盖率 | 🔴 | ⚠️ 60% | PMIC+Display+启动已实测 |
| R7 4G 流量费用 | ⚠️ | ⊘ 未启用 | 4G 未初始化 |
| **R8 chip revision** | **🔴 P0** | **✅ 已消除** | `SELECTS_REV_LESS_V3=y` 解决 |

---

## 📊 项目总体进度

### 已完成（里程碑）

| 里程碑 | 状态 | 关键产出 |
|---|---|---|
| Day 0 | ✅ | Git 仓库 + 目录结构 + 29 PDF + 规格书 |
| M0.5 | ✅ | CC 编排基础设施（6 脚本 + 3 模板） |
| M0-A | ✅ | 5 份 datasheets-summary 调研 |
| M1 | ✅ | Cordis 插件集成 |
| M3 | ✅ | 架构文档 + ES7210 风险解除 |
| M4 | ✅ | ESP-IDF 项目脚手架（commit `70e508b`） |
| M5 | ✅ | 启动验证 + R8 消除（commit `0187ca5`） |
| M6 | ✅ | Audio 修复（commit `3b51cd5`） |
| **M7** | **⏳ 进行中** | 实测模板就绪，待 CCA T7 填写 |

### CCB 文档产出（T1-T7）

| 任务 | 文件 | commit | 字数 |
|---|---|---|---|
| T1 | `docs/hw/可信度评估报告-v1.md` | — | — |
| T2 | `docs/hw/audio-test-spec.md` | — | 313 |
| T3 | `docs/hw/pmic-registers-audit.md` | `281aef8` | — |
| T4 | `docs/hw/4g-flow-guide.md` | `a938078` | 1567 |
| T5 | `docs/hw/chip-revision-history.md` | `323e380` | 1516 |
| T6 | `docs/hw/M5-verification-report.md` | `0187ca5` | 1533 |
| T7a | `docs/hw/M7-test-log.md` | `80e5f73` | 1854 |
| T7b | `docs/hw/M7-long-run-plan.md` | `b8067d8` | 1964 |

### 待办（按优先级）

| # | 任务 | 优先级 | 状态 | 阻塞原因 |
|---|---|---|---|---|
| 1 | CCA T7 填写 M7 实测数据 | 🔴 P0 | ⏳ | 需真机操作 |
| 2 | 24h 长稳测试 | 🔴 P0 | ⏳ | 需 M7 实测通过 |
| 3 | 4G 模块启用 + 实测 | P1 | ⏳ | Audio 已修复，可启用 |
| 4 | ALDO4 电压实测（R4） | P1 | ⏳ | 需万用表 |
| 5 | GitHub push | P2 | ⏳ | 需创建远程 repo |
| 6 | IMU (LSM6DS3) 集成 | P3 | ⊘ | 低优先级 |
| 7 | LVGL UI 框架 | P3 | ⊘ | 需 Display 实测通过 |
| 8 | WebSocket dsh_client | P4 | ⊘ | 需 4G 先通 |

---

## 🎯 项目定位

**便携式 AI Agent 远程终端**（基于 ESP32-P4C5 酷世DIY 开发板）

**与 OMT 关系**：硬件差异 30%，但产品定位 / 协议层 / 协作方法论 / MVP 验收清单 100% 相同。

---

## 📊 关键数据

- **资料完整度**：优于 OMT（⭐⭐⭐⭐⭐ vs ⭐⭐⭐⭐）
- **DSH 累计 commits**：12+
- **CCB 文档产出**：8 份（T1-T7，含审计/指南/报告/模板）
- **M7 实测就绪度**：90%（模板就绪，待 CCA T7 真机填写）
- **关键技术风险**：2 个已消除（R1 ES7210、R8 chip rev），1 个待实测（R4 ALDO4）
- **实测覆盖率**：60%（PMIC+Display+启动已实测，Audio/4G 待 M7）

---

## 🔑 关键文件路径

| 用途 | 路径 |
|---|---|
| 项目总览 | `README.md` |
| 系统架构 | `ARCHITECTURE.md` |
| 变更日志 | `CHANGELOG.md` |
| 发布说明 | `RELEASE_NOTES.md` |
| 实践日志 | `docs/cc-orchestration/实践日志.md` |
| 5 分钟上手 | `docs/hw/README.md` |
| 完整规格 | `docs/hw/p4c5-spec.md` |
| 引脚表 | `docs/hw/p4c5-pins.csv` |
| OMT 差异 | `docs/hw/00-新硬件差异矩阵.md` |
| 可信度评估 | `docs/hw/可信度评估报告-v1.md` |
| 5 份调研 | `docs/hw/datasheets-summary/` |
| PMIC 审计 | `docs/hw/pmic-registers-audit.md` |
| Audio 测试规范 | `docs/hw/audio-test-spec.md` |
| 4G 流量指南 | `docs/hw/4g-flow-guide.md` |
| Chip revision 报告 | `docs/hw/chip-revision-history.md` |
| M5 启动验证报告 | `docs/hw/M5-verification-report.md` |
| **M7 实测数据模板** | **`docs/hw/M7-test-log.md`** |
| **M7 24h 长稳方案** | **`docs/hw/M7-long-run-plan.md`** |
| CC 运行手册 | `docs/cc-orchestration/RUNBOOK.md` |
| Subagent 最佳实践 | `docs/cc-orchestration/SUBAGENT-BEST-PRACTICES.md` |
| CC 主控脚本 | `.dsh-orchestration/bin/cc-orchestrator.sh` |
| DSH 启动 | `run-cordis-overlay.sh` |

---

## 🚨 红线（不可破坏）

- 🚫 最大 2 个 `claude -p` 并发
- 🚫 不修改 OMT / macs 仓库
- 🚫 不修改 DSH 全局配置（`~/.dsh/`、`/opt/homebrew/lib/node_modules/@deepseek-ai/`）
- 🚫 大文件不入仓（用 LFS）
- 🚫 敏感信息不上 commit
- 🚫 文件边界：CCA 写 `hardware/` + `docs/hw/`；CCB 写 `docs/research/` + 文档归档

---

**下次更新**：M7 实测数据填写完成后（CCA T7）
**接手者**：CCA T7（M7 实测）→ DSH（M8 规划）