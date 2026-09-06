#!/bin/bash
# ============================================================================
# cc-spawn.sh - 启动或 resume 一个 CC 进程
#
# 用法: bash cc-spawn.sh <cca|ccb> [--force-fresh]
#
# 行为:
#   1. 读 state/<role>.session-id (如无则生成新 UUID)
#   2. nohup 启动 claude --resume <sid> (或 --session-id <sid> 首次)
#   3. 加载 templates/<role>-system-prompt.md
#   4. 写 state/<role>.pid
#   5. 等 5s 检测存活
#
# 输出:
#   state/<role>.pid         - claude 进程 PID
#   state/<role>.session-id  - UUID
#   logs/<role>.log          - claude 实时输出
#
# 注意: ${VAR} 加花括号, 避免 $VAR 紧跟中文全角标点导致 unbound variable
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
ORCH_DIR="${PROJ_DIR}/.dsh-orchestration"
STATE_DIR="${ORCH_DIR}/state"
LOG_DIR="${ORCH_DIR}/logs"
TPL_DIR="${ORCH_DIR}/templates"

ROLE="${1:-}"
FORCE_FRESH="${2:-}"

if [ -z "${ROLE}" ] || { [ "${ROLE}" != "cca" ] && [ "${ROLE}" != "ccb" ]; }; then
  echo "用法: $0 <cca|ccb> [--force-fresh]" >&2
  exit 1
fi

PID_FILE="${STATE_DIR}/${ROLE}.pid"
SID_FILE="${STATE_DIR}/${ROLE}.session-id"
LOG_FILE="${LOG_DIR}/${ROLE}.log"
PROMPT_FILE="${TPL_DIR}/${ROLE}-system-prompt.md"

# 检查依赖
if ! command -v claude >/dev/null 2>&1; then
  echo "[spawn] 错误: claude 命令未找到。请安装 Claude Code CLI。" >&2
  echo "[spawn] 参考: https://docs.anthropic.com/en/docs/claude-code" >&2
  exit 1
fi

if [ ! -f "${PROMPT_FILE}" ]; then
  echo "[spawn] 错误: 系统提示模板不存在: ${PROMPT_FILE}" >&2
  exit 1
fi

# 检查现有进程是否还活着
if [ -f "${PID_FILE}" ]; then
  EXISTING_PID=$(cat "${PID_FILE}")
  if kill -0 "${EXISTING_PID}" 2>/dev/null; then
    echo "[spawn] 警告: ${ROLE} 进程已存在 (PID=${EXISTING_PID})" >&2
    echo "[spawn] 如需重启请先: bash cc-kill.sh ${ROLE}" >&2
    exit 1
  fi
  # PID 文件存在但进程死了，清理
  rm -f "${PID_FILE}"
fi

# 决定 session-id
if [ -f "${SID_FILE}" ] && [ "${FORCE_FRESH}" != "--force-fresh" ]; then
  SID=$(cat "${SID_FILE}")
  RESUME_FLAG="--resume ${SID}"
  echo "[spawn] ${ROLE}: resuming session ${SID}"
else
  SID=$(uuidgen | tr 'A-Z' 'a-z')
  echo "${SID}" > "${SID_FILE}"
  RESUME_FLAG="--session-id ${SID}"
  echo "[spawn] ${ROLE}: new session ${SID}"
fi

# 启动 claude（后台 + nohup + 重定向）
cd "${PROJ_DIR}"
nohup zsh -c "source ~/.zshrc 2>/dev/null; \
  claude -p \
    ${RESUME_FLAG} \
    --system-prompt \"\$(cat ${PROMPT_FILE})\" \
    --add-dir ${PROJ_DIR} \
    --output-format stream-json \
    --verbose \
    --dangerously-skip-permissions" \
  > "${LOG_FILE}" 2>&1 &

NEW_PID=$!
echo "${NEW_PID}" > "${PID_FILE}"
echo "[spawn] ${ROLE}: PID=${NEW_PID}, log=${LOG_FILE}"

# 5s 存活检测
sleep 5
if ! kill -0 "${NEW_PID}" 2>/dev/null; then
  echo "[spawn] 警告: ${ROLE} 进程启动后立即死亡" >&2
  echo "[spawn] stderr 预览:" >&2
  tail -10 "${LOG_FILE}" >&2 || true
  exit 1
fi

echo "[spawn] ${ROLE}: alive and ready"