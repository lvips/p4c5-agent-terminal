# CC Orchestration RUNBOOK — 启动 / 停止 / 排错手册

> **版本**：v1.0（2026-09-06）
> **目的**：让任何接手者 5 分钟内启动 / 停止 / 排错 CCA + CCB 工作流

---

## 一、5 分钟上手

```bash
# 1. 进入项目
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5

# 2. 启动 CC（后台）
nohup bash .dsh-orchestration/bin/cc-orchestrator.sh start &

# 3. 验证
bash .dsh-orchestration/bin/cc-orchestrator.sh status
# 期望输出:
#   cca: pid=12345 sid=xxx-xxx alive
#   ccb: pid=12346 sid=yyy-yyy alive
#   并发槽使用: 2 / 2

# 4. 加载 DSH Cordis 插件（让 DSH 看到 tool_agent_cca 等）
bash run-cordis-overlay.sh
```

启动后 DSH 主对话会自动出现 3 个工具：
- `tool_agent_cca` — 派任务给 CCA
- `tool_agent_ccb` — 派任务给 CCB
- `tool_cc_status` — 查询 CC 状态

---

## 二、核心命令

### 2.1 启动 / 停止

| 命令 | 说明 |
|---|---|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh start` | 启动 cca + ccb（后台 daemon）|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh stop` | 停止 cca + ccb（保留 session-id）|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh status` | 查询状态 |
| `bash .dsh-orchestration/bin/cc-orchestrator.sh restart cca` | 重启 CCA（保留 session-id）|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh restart ccb` | 重启 CCB（保留 session-id）|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh handoff cca` | 触发 HANDOFF（压缩 + 重启）|
| `bash .dsh-orchestration/bin/cc-orchestrator.sh handoff ccb` | 同上 CCB |

### 2.2 派发任务

DSH 主对话内调用工具（**推荐**）：
```javascript
// CCA - 主力科研（硬件代码 + 规格书）
tool_agent_cca({
  prompt: "写 ES8311 驱动到 hardware/p4c5-agent-terminal/components/audio_codec_es8311/",
  timeout_min: 30
})

// CCB - 辅助科研（调研 + 文档 + commit + benchmark）
tool_agent_ccb({
  prompt: "读 docs/hw/datasheets/C3036461_*.PDF，写 docs/research/axp2101-summary.md",
  timeout_min: 15
})

// 查询状态
tool_cc_status()
```

命令行同步派发（**测试用**）：
```bash
bash .dsh-orchestration/bin/cc-orchestrator.sh dispatch cca /tmp/my-task.md
bash .dsh-orchestration/bin/cc-orchestrator.sh dispatch ccb /tmp/my-task.md
```

### 2.3 单独操作

```bash
# 启动单个 CC
bash .dsh-orchestration/bin/cc-spawn.sh cca
bash .dsh-orchestration/bin/cc-spawn.sh cca --force-fresh  # 不 resume, 用新 session

# 停止单个 CC
bash .dsh-orchestration/bin/cc-kill.sh cca
bash .dsh-orchestration/bin/cc-kill.sh cca --force  # 强制 kill

# 查询状态
bash .dsh-orchestration/bin/cc-status.sh
bash .dsh-orchestration/bin/cc-status.sh cca
```

---

## 三、文件位置速查

| 内容 | 路径 |
|---|---|
| 主控脚本 | `.dsh-orchestration/bin/cc-orchestrator.sh` |
| spawn | `.dsh-orchestration/bin/cc-spawn.sh` |
| kill | `.dsh-orchestration/bin/cc-kill.sh` |
| status | `.dsh-orchestration/bin/cc-status.sh` |
| dispatch | `.dsh-orchestration/bin/cc-dispatch.sh` |
| handoff | `.dsh-orchestration/bin/cc-handoff.sh` |
| CCA 系统提示 | `.dsh-orchestration/templates/cca-system-prompt.md` |
| CCB 系统提示 | `.dsh-orchestration/templates/ccb-system-prompt.md` |
| HANDOFF 模板 | `.dsh-orchestration/templates/HANDOFF-template.md` |
| **PID 文件** | `.dsh-orchestration/state/<role>.pid` |
| **session-id 文件** | `.dsh-orchestration/state/<role>.session-id` |
| **日志** | `.dsh-orchestration/logs/<role>.log` |
| **心跳日志** | `.dsh-orchestration/logs/heartbeat.log` |
| **Cordis 插件** | `.dsh-orchestration/cordis-plugin/code.host.js` |
| **HANDOFF 归档** | `docs/handovers/HANDOFF-<role>-<ts>.md` |
| **当前主 HANDOFF** | `docs/handovers/HANDOFF.md` |
| **DSH 加载脚本** | `run-cordis-overlay.sh` |

---

## 四、常见排错

### 4.1 CC 启动失败

**症状**：`cc-orchestrator.sh start` 后 `status` 显示 `dead`

**排查**：
```bash
# 1. 看日志
tail -50 .dsh-orchestration/logs/cca.log

# 2. 手动启动看错误
bash .dsh-orchestration/bin/cc-spawn.sh cca

# 3. 检查 claude CLI
which claude
claude --version

# 4. 检查 PATH（zsh source ~/.zshrc）
zsh -c "source ~/.zshrc && which claude"
```

**常见原因**：
- `claude` 命令未找到 → 安装 Claude Code CLI
- session-id 文件损坏 → `rm .dsh-orchestration/state/cca.session-id` 重试
- zsh 路径问题 → 编辑 `cc-spawn.sh` 中 `zsh -c "source ~/.zshrc; ..."` 部分

### 4.2 CC 启动后立即死

**症状**：spawn 5s 后 `status` 显示 `dead`

**排查**：
```bash
# 看 stderr
cat .dsh-orchestration/logs/cca.log | tail -30

# 强制启动看完整输出
bash .dsh-orchestration/bin/cc-spawn.sh cca --force-fresh
```

**常见原因**：
- 系统提示模板加载失败 → 检查 `.dsh-orchestration/templates/cca-system-prompt.md` 是否存在
- 内存不足 → 关掉其他进程
- 端口冲突（不可能，因为我们没用端口）

### 4.3 API 限流（429）

**症状**：CC 报 `rate_limit_error`，多次重试

**排查**：
```bash
# 看 CC 输出
tail -100 .dsh-orchestration/logs/cca.log | grep -i "rate"
```

**缓解**：
- 我们**没启用**主动限流（决策 4），靠 claude CLI 自带的重试
- 如果持续限流：停掉 CC，等 1 分钟再启动
- 长期方案：用不同的 API key / 切换模型

### 4.4 DSH 不识别 agent_cca / agent_ccb

**症状**：DSH 主对话工具列表里看不到

**排查**：
```bash
# 1. 检查 patch 路径是否正确
ls /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/cordis-plugin/

# 2. 重新加载
bash run-cordis-overlay.sh

# 3. 检查 DSH 启动日志
# （DSH web GUI 的控制台 / 终端输出）

# 4. 检查 cordis-plugin.log
tail -50 .dsh-orchestration/logs/cordis-plugin.log
```

**常见原因**：
- `--patch` 路径不存在
- `code.host.js` 语法错误（用 `node -c code.host.js` 检查）
- DSH 重启后未重新加载 patch

### 4.5 CC 上下文过大

**症状**：CC 报"上下文超限"或响应变慢

**解决**：
```bash
# 触发 HANDOFF（自动压缩 + 重启）
bash .dsh-orchestration/bin/cc-orchestrator.sh handoff cca

# 或手动
bash .dsh-orchestration/bin/cc-handoff.sh cca
```

这会：
1. 让 CC 输出"工作摘要 + 文件变更 + 未决问题"
2. 写 `docs/handovers/HANDOFF-cca-<ts>.md`
3. kill 旧 CC
4. spawn 新 CC（用新 session-id）
5. 下次 dispatch 时，DSH 应喂新 CC 读最新 HANDOFF

### 4.6 磁盘满

**症状**：CC spawn 失败，日志报 "no space left"

**清理**：
```bash
# 清理老日志（保留最近 7 天）
find .dsh-orchestration/logs -name "*.log" -mtime +7 -delete

# 清理 git 大文件
git gc --aggressive --prune=now

# 清理 ESP-IDF build 产物
rm -rf hardware/*/build
```

---

## 五、最佳实践

### 5.1 派任务时

- ✅ prompt 自包含（含目标 / 输入 / 输出 / 验收）
- ✅ 大任务拆小（每个 ≤ 30 分钟）
- ✅ 明确写文件路径（让 CC 知道写到哪）
- ✅ 明确写 commit 要求（"完成后 commit + push"）
- ❌ 不要让 CC 改其他 CC 的产物（违反文件边界）

### 5.2 HANDOFF 触发时机

- ✅ CC 跑过 5+ 任务后（防止上下文过大）
- ✅ 切换大方向前（例如从调研切到写代码）
- ✅ 每天工作结束前
- ❌ 不要每任务都 handoff（浪费时间）

### 5.3 文件边界（强约束）

| 角色 | 可写 | 不可写 |
|---|---|---|
| DSH | 所有（但不直接写代码）| — |
| CCA | `hardware/` + `docs/hw/` | `docs/research/` `docs/cc-orchestration/` |
| CCB | `docs/research/` + `docs/cc-orchestration/` + 文档归档 + commit + benchmark | `hardware/` `docs/hw/` `docs/handovers/` |

---

## 六、监控指标

```bash
# 1. CC 实时状态
watch -n 5 "bash .dsh-orchestration/bin/cc-orchestrator.sh status"

# 2. 心跳日志（最近 20 条）
tail -20 .dsh-orchestration/logs/heartbeat.log

# 3. CC 日志大小（异常大 = 上下文膨胀）
ls -lh .dsh-orchestration/logs/

# 4. 并发槽使用
ps aux | grep 'claude -p' | grep -v grep | wc -l

# 5. 最近 HANDOFF
ls -lt docs/handovers/ | head -5
```

---

## 七、版本

- v1.0 (2026-09-06): 初版，DSH Day 0 + M0.5 + M1 配套