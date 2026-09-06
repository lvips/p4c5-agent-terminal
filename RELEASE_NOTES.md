# Release Notes — v0.1.0

> **项目**：p4c5-agent-terminal
> **版本**：v0.1.0（Day 0 + M0.5 + M0-A + M1 里程碑）
> **发布日期**：2026-09-06
> **代号**：Genesis（创世纪）

---

## 🎉 概述

**v0.1.0** 是 p4c5-agent-terminal 项目的**第一个里程碑**：
- ✅ Day 0：项目仓库 + 资料整理 + 首次 commit
- ✅ M0.5：CC 编排基础设施（让 DSH 调度 2 个长驻 CC）
- ✅ M0-A：5 大模块 datasheets-summary 调研（识别风险）
- ✅ M1：Cordis 插件集成（让 DSH 看到 agent_cca / agent_ccb 工具）

**当前状态**：项目基础架构建立完成，等待 M2 实测和真机验证。

---

## ✨ 新增功能

### 文档体系

| 文档 | 路径 | 说明 |
|---|---|---|
| 项目总览 | `README.md` | 项目介绍 + 工具栈 + 红线 |
| 系统架构 | `ARCHITECTURE.md` | 硬件 + 软件 + 协议 + 协作方法论 |
| 变更日志 | `CHANGELOG.md` | 详细变更记录 |
| 发布说明 | `RELEASE_NOTES.md` | 本文件 |
| 硬件资料 5 分钟速查 | `docs/hw/README.md` | 速查表 |
| 完整规格书 | `docs/hw/p4c5-spec.md` | 7 章节硬件规格 |
| 引脚定义表 | `docs/hw/p4c5-pins.csv` | 100+ 引脚 |
| OMT 差异矩阵 | `docs/hw/00-新硬件差异矩阵.md` | 24 项差异 |
| 可信度评估 | `docs/hw/可信度评估报告-v1.md` | 5 大模块评分 |
| 实践日志 | `docs/cc-orchestration/实践日志.md` | 踩坑记录 |
| 5 份 datasheets-summary | `docs/hw/datasheets-summary/` | DSH 调研产出 |

### 资料整理

- **29 份 IC 规格书 PDF**（15 份 datasheets + 14 份 ML307C AT 文档）
- **1 份原理图 PDF**（KSDIY P4C5 开发板）
- **4 份 3D 外壳文件**（3 STL + 1 STEP）

### CC 编排基础设施

- **6 个 shell 脚本**（`.dsh-orchestration/bin/`）：
  - `cc-orchestrator.sh`（主控：start/stop/status/restart/dispatch/handoff）
  - `cc-spawn.sh`（启动单个 CC）
  - `cc-kill.sh`（停止单个 CC）
  - `cc-status.sh`（查状态）
  - `cc-dispatch.sh`（同步派发任务，含 30min 硬超时 + 15min 防停滞）
  - `cc-handoff.sh`（写 HANDOFF + 压缩上下文 + 重启）
- **3 份提示词模板**（`.dsh-orchestration/templates/`）：
  - `cca-system-prompt.md`（CCA 科研主管）
  - `ccb-system-prompt.md`（CCB 科研助理）
  - `HANDOFF-template.md`（HANDOFF 模板）

### Cordis 插件集成

- **1 个 DSH Cordis 插件**（`.dsh-orchestration/cordis-plugin/`）：
  - `package.json`（dsh.bundle manifest）
  - `code.host.js`（注册 cca.session / ccb.session Service + agent_cca / agent_ccb / cc_status Tool + 30s heartbeat Timer）
- **1 个启动脚本**：`run-cordis-overlay.sh`
- **2 份文档**：
  - `docs/cc-orchestration/RUNBOOK.md`（启动 / 停止 / 排错手册）
  - `docs/cc-orchestration/SUBAGENT-BEST-PRACTICES.md`（M0-A 失败教训 + 修复方案）

---

## ⚠️ 已知风险（来自可信度评估）

| # | 风险 | 缓解 |
|---|---|---|
| R1 | **ES7210 规格书缺失** | Day 1 补（从 xiaozhi 工程或 Espressif 官网）|
| R2 | AXP2101 14 个寄存器未逐一核对 | CCA 写驱动时**必须**双源对照 xiaozhi 代码 + 规格书 |
| R3 | LSM6DS3 芯片版本不一致（TR-C vs L）| M1 实机读 WHO_AM_I 寄存器确认 |
| R4 | ML307C 电压问题（规格书 3.4-4.2V vs xiaozhi ALDO4=2.9V）| M1 实机验证 |
| R5 | GPIO4 冲突（CH343P ESP_RX vs ML307C POWER）| xiaozhi 已处理，M1 验证 |
| R6 | 所有电气参数 0% 实测 | M1 阶段每个 commit 前真机验证 |
| R7 | 4G 流量费用 | 用定向流量包，限制并发连接数 |

---

## 🔧 升级说明

无（首版）。

---

## 🔮 后续计划

| 版本 | 目标 | 时间 |
|---|---|---|
| v0.2.0 | M2 HANDOFF 实测 + 第一次真机验证（仅供电）| 1 周内 |
| v0.3.0 | M4 PoC：屏幕点亮 + 4G 拨号 + 1 个传感器 | 2 周内 |
| v0.4.0 | M4 集成：LVGL UI + WebSocket Client | 3 周内 |
| v0.5.0 | M4 完成：语音交互 + 协议层 | 4 周内 |
| v1.0.0 | M5：长稳测试 + 低功耗 + 量产就绪 | 6-8 周内 |

---

## 👥 贡献者

- **DSH** (DeepSeek Harness) — 项目指挥 + 协调 + 兜底产出
- **CCA** — 主力科研（M1 启动后实际写代码）
- **CCB** — 辅助科研（M1 启动后实际写文档）

---

## 📦 仓库地址

- **远程**：`https://github.com/lvips/p4c5-agent-terminal`（待 push，需用户在 GitHub 创建空 repo）

---

## 🙏 致谢

- **kevincoooool / 酷世DIY** — 完整 xiaozhi-esp32 参考工程
- **乐鑫 (Espressif)** — ESP32-P4 / C5 完整规格书
- **中移 OneMO** — ML307C 完整 AT 文档
- **OMT 项目** — 协议层 + 协作方法论

---

**项目代号**：Genesis（创世纪）
**项目副标题**：便携式 AI Agent 远程终端
**项目口号**：让你的 DSH 装进口袋