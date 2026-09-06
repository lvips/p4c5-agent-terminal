# HANDOFF — p4c5-agent-terminal

> **当前版本**：v0.1.0-Day0（2026-09-06）
> **作者**：DSH（DeepSeek Harness）
> **下次更新**：M0.5 完成 + M0-A 完成后

---

## 一、项目当前状态

**Day 0 已完成**（2026-09-06）：
- ✅ Git 仓库初始化（remote: `https://github.com/lvips/p4c5-agent-terminal`）
- ✅ 目录骨架建立（`.dsh-orchestration/` + `docs/`）
- ✅ 15 份 IC 规格书 PDF 整理入库
- ✅ 原理图 + 3D 外壳入库
- ✅ README + LICENSE + .gitignore
- ✅ `docs/hw/README.md`（5 分钟速查）
- ✅ `docs/hw/p4c5-spec.md`（7 章节完整规格）
- ✅ `docs/hw/p4c5-pins.csv`（完整引脚定义表）
- ✅ `docs/hw/00-新硬件差异矩阵.md`（vs OMT Tab5）
- ✅ `docs/README.md`、`docs/cc-orchestration/README.md`（骨架）

**进行中**：M0.5 CC 编排脚本 + 模板

**待启动**：
- ⏳ M0-A 5 个并行 subagent 资料采集
- ⏳ M1 Cordis 插件集成
- ⏳ M2 HANDOFF 机制
- ⏳ M3 试运行 + 项目集成

---

## 二、下次接手者的第一件事

1. 读 `README.md`（项目总览）
2. 读 `docs/hw/README.md`（5 分钟硬件速查）
3. 读 `docs/hw/p4c5-spec.md`（完整规格）
4. 读 `docs/hw/p4c5-pins.csv`（引脚定义）
5. 读 `docs/hw/00-新硬件差异矩阵.md`（vs OMT）
6. 读 `docs/handovers/HANDOFF.md`（当前进度，**本文件**）
7. 读 `docs/cc-orchestration/README.md`（工作流说明）
8. `bash .dsh-orchestration/bin/cc-orchestrator.sh start`

---

## 三、关键决策记录（11 11 项）

| # | 决策 | 选定 | 备注 |
|---|---|---|---|
| 1 | CCA 范围 | `hardware/` + `docs/hw/` | 主管：硬件代码 + 规格书 |
| 2 | CCB 范围 | `docs/research/` + 文档归档 | 助理：调研 + 文档 + commit + benchmark |
| 3 | 第三 CC | ❌ 不纳入 | 红线 11：最多 2 个 CC |
| 4 | 限流保护 | ❌ 不启用 | 靠 claude CLI 自带重试 |
| 5 | Cordis 插件路径 | `.dsh-orchestration/cordis-plugin/` | 项目本地 |
| 6 | M0.5 启动 | ✅ 立即 | Day 0 已启动 |
| 7 | docs/hw/ 索引 | ✅ 立即建立 | Day 0 已完成 |
| 8 | M0-A 资料采集 | ✅ 启动 | 待 dispatch |
| 9 | 三轨并行 | ✅ 完全采纳 | Day 0-2 完成 |
| 10 | Git 仓库 | `https://github.com/lvips/p4c5-agent-terminal` | — |
| 11 | 仓库名 | `p4c5-agent-terminal` | 与 OMT 命名一致 |

---

## 四、文件边界（强约束）

| 角色 | 可写 | 不可写 |
|---|---|---|
| **DSH** | 所有（但不直接写代码）| — |
| **CCA** | `hardware/` + `docs/hw/` | `docs/research/` `docs/cc-orchestration/` |
| **CCB** | `docs/research/` + `docs/cc-orchestration/` + 文档归档 + commit 撰写 + benchmark | `hardware/` |

---

## 五、当前任务队列

| 优先级 | 任务 | 派给 | 状态 |
|---|---|---|---|
| P1 | M0.5 写 6 个 CC 脚本 + 3 份模板 | DSH | 🔄 进行中 |
| P2 | M0-A 派 5 个 subagent 调研 | DSH → 5 subagents | ⏳ 待启动 |
| P3 | M0-A 汇总 5 份 summary + 写《可信度评估报告 v1》| CCB | ⏳ 待启动 |
| P4 | M1 写 Cordis 插件 + 实测 | DSH + CCA | ⏳ 待 M0-A |
| P5 | M2 HANDOFF 机制 | DSH + CCA | ⏳ 待 M1 |
| P6 | M3 试运行 + 文档 | DSH + CCA + CCB | ⏳ 待 M2 |

---

## 六、待你（用户）确认

- ⏳ 在 GitHub 创建空 repo `p4c5-agent-terminal`（不勾 README/License/.gitignore）
- ⏳ 在 GitHub 配置 SSH key（如未配）
- ⏳ 验收 Day 0 产出（看 git log + 文件树）

---

## 七、版本

- v0.1.0-Day0 (2026-09-06): 初版，DSH Day 0 启动