#!/bin/bash
# ============================================================================
# cc-kill.sh - 优雅停止单个 CC 进程
#
# 用法: bash cc-kill.sh <cca|ccb> [--force]
#
# 行为:
#   1. 读 state/<role>.pid
#   2. 先 SIGTERM (优雅)
#   3. 等 5s
#   4. 还活着 → SIGKILL (强制)
#
# 输出:
#   删除 state/<role>.pid (让 spawn.sh 知道可以重启)
#   保留 state/<role>.session-id (用于 resume)
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
STATE_DIR="${PROJ_DIR}/.dsh-orchestration/state"

ROLE="${1:-}"
FORCE="${2:-}"

if [ -z "${ROLE}" ] || { [ "${ROLE}" != "cca" ] && [ "${ROLE}" != "ccb" ]; }; then
  echo "用法: $0 <cca|ccb> [--force]" >&2
  exit 1
fi

PID_FILE="${STATE_DIR}/${ROLE}.pid"

if [ ! -f "${PID_FILE}" ]; then
  echo "[kill] ${ROLE}: 无 PID 文件, 无需停止"
  exit 0
fi

PID=$(cat "${PID_FILE}")

if ! kill -0 "${PID}" 2>/dev/null; then
  echo "[kill] ${ROLE}: PID=${PID} 已不存活, 清理 PID 文件"
  rm -f "${PID_FILE}"
  exit 0
fi

echo "[kill] ${ROLE}: 发送 SIGTERM 到 PID=${PID}"
kill "${PID}" 2>/dev/null || true

# 等 5s
for i in 1 2 3 4 5; do
  sleep 1
  if ! kill -0 "${PID}" 2>/dev/null; then
    echo "[kill] ${ROLE}: 已优雅退出 (${i}s)"
    rm -f "${PID_FILE}"
    exit 0
  fi
done

# 还活着, 强制 kill
if [ "${FORCE}" = "--force" ]; then
  echo "[kill] ${ROLE}: 强制 SIGKILL PID=${PID}"
  kill -9 "${PID}" 2>/dev/null || true
  rm -f "${PID_FILE}"
else
  echo "[kill] ${ROLE}: 5s 内未退出, 用 --force 强制 kill"
  exit 1
fi