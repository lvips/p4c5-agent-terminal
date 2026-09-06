#!/bin/bash
# ============================================================================
# run-cordis-overlay.sh - 启动 DSH 时加载本项目 CC 编排插件
#
# 用法: bash run-cordis-overlay.sh
#
# 行为:
#   1. 检查 .dsh-orchestration/bin/cc-orchestrator.sh 是否可执行
#   2. 自动启动 CCA + CCB（如果还没启动）
#   3. 用 dsh --patch 加载本项目的 cordis-plugin/
#
# 注意:
#   - 必须在 /Volumes/ZT-1T/项目开发/ESP32-P4C5/ 下运行
#   - 需要 dsh 命令在 PATH 中（`which dsh` 验证）
#   - 不会自动 commit / push
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
ORCH_DIR="${PROJ_DIR}/.dsh-orchestration"
PLUGIN_DIR="${ORCH_DIR}/cordis-plugin"

# 检查路径
if [ ! -d "${PLUGIN_DIR}" ]; then
  echo "[overlay] 错误: 插件目录不存在: ${PLUGIN_DIR}" >&2
  exit 1
fi

if [ ! -f "${PLUGIN_DIR}/code.host.js" ]; then
  echo "[overlay] 错误: code.host.js 不存在" >&2
  exit 1
fi

# 检查 dsh 命令
if ! command -v dsh >/dev/null 2>&1; then
  echo "[overlay] 错误: dsh 命令未找到" >&2
  echo "[overlay] 安装: 参考 https://github.com/deepseek-ai/dsh" >&2
  exit 1
fi

# 自动启动 CCA + CCB (如未运行)
echo "[overlay] 检查 CC 状态..."
if ! bash "${ORCH_DIR}/bin/cc-orchestrator.sh" status >/dev/null 2>&1; then
  echo "[overlay] 启动 CC orchestrator..."
  bash "${ORCH_DIR}/bin/cc-orchestrator.sh" start
fi

echo "[overlay] 当前 CC 状态:"
bash "${ORCH_DIR}/bin/cc-orchestrator.sh" status

# 加载 Cordis patch
echo ""
echo "[overlay] 加载 Cordis 插件: ${PLUGIN_DIR}"
echo "[overlay] 工具列表将出现: agent_cca / agent_ccb / cc_status"
echo ""

# 调用 dsh 加载 patch (DSH 接管后, 工具即可见)
exec dsh --patch "${PLUGIN_DIR}" "$@"