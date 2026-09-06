# HANDOFF — 项目当前进度

> **版本**：v0.1.0-Day0 + M0.5 + M0-A + M1 + M3 文档（2026-09-06）
> **角色**：DSH → 接手者
> **状态**：Day 0 + M0.5 + M0-A + M1 + M3 文档全部完成，待 M2 实测 + GitHub push

---

## 🎯 项目定位

**便携式 AI Agent 远程终端**（基于 ESP32-P4C5 酷世DIY 开发板）

**与 OMT 关系**：硬件差异 30%，但产品定位 / 协议层 / 协作方法论 / MVP 验收清单 100% 相同。

---

## ✅ 已完成（6 个 commit，13+ 个 Markdown，6 个 shell 脚本，2 个 Cordis 插件，29 个 PDF）

### 1. Day 0：项目基础架构
- ✅ Git 仓库 + 远程（`https://github.com/lvips/p4c5-agent-terminal`）
- ✅ 目录结构（docs/ + hardware/ + .dsh-orchestration/）
- ✅ 29 份 IC 规格书 PDF + 1 份原理图 + 3D 外壳
- ✅ README.md / LICENSE / docs/README.md / docs/hw/README.md
- ✅ p4c5-spec.md (7 章节完整规格)
- ✅ p4c5-pins.csv (100+ 引脚定义)
- ✅ 00-新硬件差异矩阵.md (vs OMT Tab5)
- ✅ 首次 git commit

### 2. M0.5：CC 编排基础设施
- ✅ 6 个 shell 脚本（cc-orchestrator / spawn / kill / status / dispatch / handoff）
- ✅ 3 份提示词模板（cca / ccb / HANDOFF）
- ✅ 30min 硬超时 + 15min 防停滞 + 启动加固

### 3. M0-A：5 份 datasheets-summary 调研
- ✅ p4c5-board.md（开发板本体，1031 字）
- ✅ ml307c.md（4G 模组，896 字）
- ✅ audio-codecs.md（音频三件套，1037 字）
- ✅ pmic-axp2101.md（PMIC，969 字）
- ✅ peripherals.md（外设，975 字）
- ✅ 5 份 subagent 失败，DSH 用工具直接产出（兜底成功）

### 4. M1：Cordis 插件集成
- ✅ code.host.js（注册 cca.session/ccb.session Service + agent_cca/agent_ccb/cc_status Tool + heartbeat Timer）
- ✅ package.json（dsh.bundle manifest，项目本地）
- ✅ run-cordis-overlay.sh（启动 + 加载）
- ✅ RUNBOOK.md（5 分钟上手 + 启动/停止/排错）
- ✅ SUBAGENT-BEST-PRACTICES.md（M0-A 失败教训 + 修复方案）

### 5. M3 文档（最终）
- ✅ ARCHITECTURE.md（系统架构 + 协议层 + 协作方法论）
- ✅ CHANGELOG.md（v0.1.0 变更日志）
- ✅ 实践日志.md（5 subagent 失败根因 + 10 条改进项）
- ✅ RELEASE_NOTES.md（v0.1.0 Genesis）
- ✅ ES7210 风险解除：用 Espressif `esp_audio_codec` 库即可

---

## 🚦 待办（按优先级）

### P0：阻塞项（需要你行动）

| # | 任务 | 阻塞原因 | 解决路径 |
|---|---|---|---|
| 1 | **在 GitHub 创建空 repo `p4c5-agent-terminal`** | 本地 git remote 配置好了，但远程不存在 | 登录 GitHub → New repo → 名字 `p4c5-agent-terminal` → 不勾 README/License/.gitignore → 创建后 `git push -u origin main` |
| 2 | **手动验证 CC orchestrator 启动** | 需要 `claude` CLI 工作 | `bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/bin/cc-orchestrator.sh start` |

### P1：M2 实测

| # | 任务 | 状态 | 备注 |
|---|---|---|---|
| 3 | M2 HANDOFF 实测 | ⏳ 待启动 | 跑 5 个任务后触发 handoff 验证 |
| 4 | Cordis 插件手动验证（加载后看工具是否出现）| ⏳ 待启动 | `bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/run-cordis-overlay.sh` |

### P2：M4+ 真机开发

| # | 任务 | 状态 | 备注 |
|---|---|---|---|
| 5 | 真机验证电压轨（DCDC1/ALDO1/ALDO3/ALDO4）| ⏳ M1 | 万用表测 |
| 6 | 真机验证 ML307C 4G 拨号 | ⏳ M1 | esp-ml307 集成 |
| 7 | AXP2101 14 寄存器逐个核对 | ⏳ M1 | 双源对照 xiaozhi + 规格书 |
| 8 | 真机识别 LSM6DS3 芯片丝印 | ⏳ M1 | 读 WHO_AM_I=0x6A |
| 9 | OCR 原理图 + ML307C 硬件规格书 | ⏳ M3 | 已是图片 PDF |
| 10 | 写 ES8311 驱动 | ⏳ M1 | 用 es8311_audio_codec.cc 移植 |
| 11 | 集成 esp-ml307 (4G) | ⏳ M1 | `78/esp-ml307` v3.6.4 |
| 12 | 集成 ST7102 LCD + ST7123 触摸 | ⏳ M1 | xiaozhi `esp_lcd_st7102` 组件 |
| 13 | 集成 AXP2101 PMIC | ⏳ M1 | xiaozhi `pmic_axp2101` 组件 |
| 14 | LVGL UI 框架 | ⏳ M4 | xiaozhi `lvgl_st7102_display` |
| 15 | WebSocket Client + dsh_client 协议 | ⏳ M4 | 100% 复用 OMT |

---

## 📊 关键数据

- **资料完整度**：优于 OMT（⭐⭐⭐⭐⭐ vs ⭐⭐⭐⭐）
- **DSH 累计 commits**：6
- **DSH 累计产出**：13+ Markdown 文件 + 6 shell 脚本 + 2 Cordis 插件 + 29 PDF
- **M0-A subagent 失败率**：5/5（100%，DSH 全部兜底）
- **关键技术风险**：1 个已解除（ES7210 规格书缺失），6 个待实测
- **M1 启动就绪度**：95%（只差 AXP2101 14 寄存器双源核对）

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

**下次更新**：M2 实测完成后
**接手者**：CCA / CCB（M1 启动后）