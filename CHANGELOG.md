# 变更日志

> 格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
> 本项目使用 [语义化版本](https://semver.org/lang/zh-CN/)。

---

## [Unreleased]

### Added (本会话新增)

**W4: 语音唤醒 (P4 自实现)**
- `audio_pipeline/wake_word_detector/` 新组件 (RMS-based 唤醒 + 自适应阈值)
- audio_uplink_task 集成 wake 状态机 (IDLE ↔ ACTIVE 自动切换)
- 串口命令 `wake enable/disable/status`
- 6 个单元测试 (tools/wake_detector_test.py, 6/6 通过)
- `tools/esp32_test.py` ESP32 真机验证脚本
- `docs/hw/P4C5-deploy-guide.md` 完整部署指南 (W1-W4 汇总)

**W3: 完整音频链路 (上一会话已完成, 本会话整合)**
- 4 麦软件 AEC (aec_sw NLMS)
- 24k→16k 重采样 (resampler_24_16)
- Opus 编码 (16kbps/20ms)
- tts_player 组件 (WS Binary → ES8311 DAC)
- dsh_client WS Binary 支持 (W3: send + receive)
- mock_dsh_server broadcast 模式
- 3 种集成测试 (单/双/4 组件)

### Planned
- W5: 火山豆包 TTS 真实接入 (需 API key)
- W4+: 真实 WakeNet9 (需 ESP32-S3 协处理)
- speexdsp AEC 升级 (可选)

---

## [0.1.0] - 2026-09-06

### 🎉 Day 0 里程碑：项目基础架构建立

**Day 0 目标**：建立项目仓库 + 整理资料 + 建立工具链 + 首次 commit。

### Added
- 项目根目录结构（docs/ + hardware/ + .dsh-orchestration/）
- `README.md`（项目总览 + 与 OMT 对比 + 工具栈 + 红线）
- `LICENSE`（Apache 2.0）
- `ARCHITECTURE.md`（系统架构 + 协议层 + 协作方法论）
- `docs/README.md`（文档总索引）
- `docs/hw/README.md`（硬件资料 5 分钟速查）
- `docs/hw/p4c5-spec.md`（7 章节完整规格书）
- `docs/hw/p4c5-pins.csv`（100+ 引脚定义表）
- `docs/hw/00-新硬件差异矩阵.md`（与 OMT Tab5 24 项差异）
- `docs/hw/可信度评估报告-v1.md`（5 大模块评分）
- `docs/hw/datasheets/`（29 份 IC 规格书 PDF）
- `docs/hw/datasheets-summary/`（5 份 DSH 调研报告）
- `docs/hw/schematic/原理图.pdf`
- `docs/hw/shell/`（3 STL + 1 STEP 3D 外壳）
- `docs/handovers/HANDOFF.md`（v0.1.0-Day0 当前进度）

### M0.5 里程碑：CC 编排基础设施

**目标**：让 DSH 能在项目内调度 2 个长驻 CC（CCA + CCB）。

### Added
- `.dsh-orchestration/bin/cc-orchestrator.sh`（主控：start/stop/status/restart/dispatch/handoff）
- `.dsh-orchestration/bin/cc-spawn.sh`（启动单个 CC，支持 --resume <uuid> 或 --force-fresh）
- `.dsh-orchestration/bin/cc-kill.sh`（优雅停止 + 强制 kill 二级）
- `.dsh-orchestration/bin/cc-status.sh`（查 PID + session-id + alive）
- `.dsh-orchestration/bin/cc-dispatch.sh`（同步派发任务，含 30min 硬超时 + 15min 防停滞 + 启动加固）
- `.dsh-orchestration/bin/cc-handoff.sh`（写 HANDOFF + 压缩上下文 + 重启）
- `.dsh-orchestration/templates/cca-system-prompt.md`（CCA 科研主管系统提示）
- `.dsh-orchestration/templates/ccb-system-prompt.md`（CCB 科研助理系统提示）
- `.dsh-orchestration/templates/HANDOFF-template.md`（HANDOFF 模板）

### M0-A 里程碑：5 模块 datasheets-summary 调研

**目标**：抽 5 份调研报告，识别风险，决定 M1 启动策略。

### Added
- `docs/hw/datasheets-summary/p4c5-board.md`（P4C5 开发板本体，1031 字）
- `docs/hw/datasheets-summary/ml307c.md`（ML307C 4G 模组，896 字）
- `docs/hw/datasheets-summary/audio-codecs.md`（音频三件套，1037 字）
- `docs/hw/datasheets-summary/pmic-axp2101.md`（AXP2101 PMIC，969 字）
- `docs/hw/datasheets-summary/peripherals.md`（外设与接口，975 字）

### 关键风险
- **R1**: ES7210 规格书缺失（立即补）
- **R2**: AXP2101 14 个寄存器未逐一核对（CCA 写驱动时必须双源核对）
- **R3**: LSM6DS3 芯片版本不一致（TR-C vs L，实机读 WHO_AM_I 确认）
- **R4**: ML307C 电压问题（规格书 3.4-4.2V vs xiaozhi ALDO4=2.9V，M1 实测）
- **R5**: GPIO4 冲突（CH343P ESP_RX vs ML307C POWER，xiaozhi 已处理）
- **R6**: 所有电气参数 0% 实测（每 commit 前真机验证）

### M1 里程碑：Cordis 插件集成

**目标**：让 DSH 主对话能看到 `agent_cca` / `agent_ccb` / `cc_status` 工具。

### Added
- `.dsh-orchestration/cordis-plugin/package.json`（dsh.bundle manifest，项目本地）
- `.dsh-orchestration/cordis-plugin/code.host.js`（注册 Service + Tool + Timer）
- `run-cordis-overlay.sh`（启动 CC + dsh --patch 加载插件）
- `docs/cc-orchestration/RUNBOOK.md`（启动 / 停止 / 排错手册）
- `docs/cc-orchestration/SUBAGENT-BEST-PRACTICES.md`（M0-A 失败教训 + 修复方案）

### 关键技术决策
- 通过 bash `cc-dispatch.sh` 调用 claude -p（30min 硬超时 + 15min 防停滞）
- Service 状态由 `state/<role>.{pid,session-id}` 管理
- Plugin 不依赖全局 `~/.dsh/`，完全项目本地

### Changed
- N/A（Day 0）

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- 敏感信息（.env*、*.key、*.pem）通过 .gitignore 屏蔽

---

## Commit 列表

```
8e749d0 docs(cc-orchestration): M0-A subagent 失败复盘 + 最佳实践
06d15c8 docs(hw): M0-A 5 份 datasheets-summary (DSH 直接产出)
f6f136e docs(hw): M0-A 硬件资料可信度评估报告 v1
cf9d144 feat(cc-orchestration): M1 Cordis 插件集成 + RUNBOOK
406a4b3 feat(project): Day 0 初始化 + M0.5 CC 编排脚本与模板
```

---

## Contributors

- **DSH** (DeepSeek Harness) — 项目指挥 + 协调 + 兜底产出
- **CCA** — 主力科研（计划中，M1 启动后实际写代码）
- **CCB** — 辅助科研（计划中，M1 启动后实际写文档）

---

## 链接

- **项目仓库**：https://github.com/lvips/p4c5-agent-terminal（待 push）
- **参考项目**：[OMT](https://github.com/lvips/omt)（占位）
- **协作系统**：[multi-agent-collab-system (macs)](https://github.com/lvips/macs)（占位）