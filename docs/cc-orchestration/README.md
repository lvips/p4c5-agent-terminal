# CC Orchestration — CCA + CCB 双 CC 长驻工作流

> **版本**：v1.0（2026-09-06，DSH Day 0 立项）
> **目的**：让 DSH（科研主任）通过 Cordis 插件 + 项目本地脚本，指挥 CCA（科研主管）+ CCB（科研助理）双 Claude Code 长驻工作流。
> **约束**：项目本地化（不污染全局 DSH 配置）

---

## 一、为什么需要这个工作流

**问题**：
1. DSH 单线程无法并行处理多任务
2. 一次性 `claude -p` 没有连续上下文（每次从0开始）
3. CC 之间容易重复造轮、互相覆盖文件

**解决**：
- **两个长驻 CC**（cca + ccb）= 持续上下文 + 并行执行
- **Cordis Patch Overlay** = 项目本地集成，不污染全局
- **文件级锁 + 角色分工** = 避免冲突
- **HANDOFF 机制** = 上下文压缩，长时间运行不丢工作

---

## 二、架构总览

```
┌─────────────────────────────────────────────────────────────┐
│ L1: DSH 主对话（你正在对话的 AI）                            │
│     - 接收需求 │ 拆解任务 │ 派发任务 │ 验收                │
│     - 调用: tool_agent_cca / tool_agent_ccb / tool_cc_status │
└──────────────────┬──────────────────────────────────────────┘
                   │ (Cordis Plugin 通信)
┌──────────────────▼──────────────────────────────────────────┐
│ L2: CC Orchestrator (项目本地后台进程)                      │
│     .dsh-orchestration/bin/cc-orchestrator.sh start          │
│     ├─ cc-spawn.sh cca/ccb (启动 claude --resume <sid>)    │
│     ├─ cc-kill.sh / cc-status.sh / cc-handoff.sh            │
│     ├─ cc-dispatch.sh (同步派发任务)                        │
│     ├─ heartbeat timer (30s)                                │
│     └─ locks/*.lock (文件级锁)                              │
└──────────────────┬──────────────────────────────────────────┘
                   │ (nohup + 子进程)
┌──────────────────▼──────────────────────────────────────────┐
│ L3: 长驻 CC 进程                                             │
│   ┌────────────────────────┐ ┌────────────────────────┐    │
│   │ cca 进程               │ │ ccb 进程               │    │
│   │ claude --resume <uuid> │ │ claude --resume <uuid> │    │
│   │ 角色: 科研主管         │ │ 角色: 科研助理         │    │
│   │ 可写: hardware/        │ │ 可写: docs/research/   │    │
│   │        docs/hw/        │ │        docs/cc-orch*/  │    │
│   └────────────────────────┘ └────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、角色分工（已确认）

| 角色 | 职责 | 可写 | 不可写 |
|---|---|---|---|
| **DSH（你正在对话的 AI）** | 科研主任：决策 + 派发 + 验收 + HANDOFF | 所有（但不直接写代码）| — |
| **CCA（科研主管）** | 主力研发：写代码、改固件、做集成 | `hardware/` `docs/hw/` | `docs/research/` |
| **CCB（科研助理）** | 辅助调研：查资料、写文档、归档 | `docs/research/` `docs/cc-orchestration/` 文档归档 + commit + benchmark | `hardware/` |

---

## 四、并发约束（红线 11）

- 同一时刻**最多 2 个** `claude -p` 并发
- cca + ccb 永久各占 1 槽 = 满载
- **不允许** 起第 3 个 CC
- DSH 任务只能排队等 cca/ccb 空闲

---

## 五、关键文件

| 文件 | 用途 |
|---|---|
| `.dsh-orchestration/bin/cc-orchestrator.sh` | ★ 主控脚本（start/stop/status/restart/dispatch）|
| `.dsh-orchestration/bin/cc-spawn.sh` | 启动单个 CC（支持 `--resume`）|
| `.dsh-orchestration/bin/cc-kill.sh` | 优雅停止单个 CC |
| `.dsh-orchestration/bin/cc-status.sh` | 查询 PID + session-id + alive |
| `.dsh-orchestration/bin/cc-handoff.sh` | 写 HANDOFF + 压缩上下文 |
| `.dsh-orchestration/bin/cc-dispatch.sh` | 同步派发任务 |
| `.dsh-orchestration/templates/cca-system-prompt.md` | CCA 角色系统提示 |
| `.dsh-orchestration/templates/ccb-system-prompt.md` | CCB 角色系统提示 |
| `.dsh-orchestration/templates/HANDOFF-template.md` | HANDOFF 模板 |
| `.dsh-orchestration/cordis-plugin/code.host.js` | ★ DSH Cordis 插件 |
| `.dsh-orchestration/cordis-plugin/tools/agent_cca.js` | 工具：派任务给 CCA |
| `.dsh-orchestration/cordis-plugin/tools/agent_ccb.js` | 工具：派任务给 CCB |
| `.dsh-orchestration/cordis-plugin/tools/cc_status.js` | 工具：查 CC 状态 |
| `.dsh-orchestration/state/{cca,ccb}.{pid,session-id}` | 运行时状态（gitignore）|
| `.dsh-orchestration/logs/{cca,ccb,orchestrator,heartbeat}.log` | 日志（gitignore）|
| `run-cordis-overlay.sh` | 启动 DSH 时 `--patch` 加载本插件 |
| `docs/cc-orchestration/RUNBOOK.md` | 启动/停止/排错手册 |
| `docs/cc-orchestration/ARCHITECTURE.md` | 详细架构（待 M3 写）|
| `docs/cc-orchestration/CHANGELOG.md` | 编排演进日志 |

---

## 六、DSH 内可用工具

启动 Cordis 插件后，DSH 主对话内会自动出现这些工具：

```javascript
// 派任务给 CCA（科研主管：硬件代码 + 规格书）
tool_agent_cca({
  prompt: "在 docs/hw/p4c5-spec.md 写完整规格书，包含 7 章节，每章 ≤300 字",
  timeout_min: 30
})

// 派任务给 CCB（科研助理：调研 + 文档 + commit + benchmark）
tool_agent_ccb({
  prompt: "抽取 AXP2101 规格书核心章节到 docs/research/axp2101-summary.md",
  timeout_min: 15
})

// 查询 CC 状态
tool_cc_status()
// → { cca: {pid, sid, alive, current_task}, ccb: {...} }
```

---

## 七、快速上手

```bash
# 1. 启动 CC daemon（后台）
nohup bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/bin/cc-orchestrator.sh start &

# 2. 验证
bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/bin/cc-orchestrator.sh status
# → cca: pid=12345 sid=xxx-xxx alive
# → ccb: pid=12346 sid=yyy-yyy alive

# 3. 加载 DSH Cordis 插件（让 DSH 看到 tool_agent_cca 等）
bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/run-cordis-overlay.sh

# 4. DSH 主对话内调用工具派发任务
```

---

## 八、版本

- v1.0 (2026-09-06): 初版，DSH Day 0 立项