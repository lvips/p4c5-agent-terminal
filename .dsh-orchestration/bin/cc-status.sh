#!/bin/bash
# ============================================================================
# cc-status.sh - 查询 CC 状态
#
# 用法: bash cc-status.sh [cca|ccb]
#
# 输出:
#   - 角色 (cca/ccb)
#   - PID
#   - session-id
#   - alive 状态
#   - 当前任务 (logs 中匹配 "Task:" 模式)
#   - 启动时间 (从 PID 反推)
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
ORCH_DIR="${PROJ_DIR}/.dsh-orchestration"
STATE_DIR="${ORCH_DIR}/state"
LOG_DIR="${ORCH_DIR}/logs"

print_status() {
  local ROLE="${1}"
  local PID_FILE="${STATE_DIR}/${ROLE}.pid"
  local SID_FILE="${STATE_DIR}/${ROLE}.session-id"
  local LOG_FILE="${LOG_DIR}/${ROLE}.log"

  local PID="n/a"
  local SID="n/a"
  local ALIVE="dead"
  local TASK="idle"

  if [ -f "${PID_FILE}" ]; then
    PID=$(cat "${PID_FILE}")
    if kill -0 "${PID}" 2>/dev/null; then
      ALIVE="alive"
      # 计算启动时长
      local START_TIME
      START_TIME=$(ps -o lstart= -p "${PID}" 2>/dev/null | xargs)
      ALIVE="${ALIVE} (uptime: ${START_TIME})"
    fi
  fi

  if [ -f "${SID_FILE}" ]; then
    SID=$(cat "${SID_FILE}")
  fi

  # 从日志最后 50 行匹配任务提示
  if [ -f "${LOG_FILE}" ] && [ "${ALIVE}" = "alive" ]; then
    local CURRENT_TASK
    CURRENT_TASK=$(tail -50 "${LOG_FILE}" 2>/dev/null | grep -E "^Human:|^Task:" | tail -1)
    if [ -n "${CURRENT_TASK}" ]; then
      TASK=$(echo "${CURRENT_TASK}" | head -c 60)
    fi
  fi

  printf "  %-4s: pid=%-8s sid=%-40s status=%-30s task=%s\n" \
    "${ROLE}" "${PID}" "${SID:0:36}" "${ALIVE}" "${TASK}"
}

# 单角色查询
if [ -n "${1:-}" ]; then
  ROLE="${1}"
  if [ "${ROLE}" != "cca" ] && [ "${ROLE}" != "ccb" ]; then
    echo "用法: $0 [cca|ccb]" >&2
    exit 1
  fi
  print_status "${ROLE}"
  exit 0
fi

# 全角色查询
echo "=== CC Status ==="
print_status "cca"
print_status "ccb"
echo ""
echo "并发槽使用: $(ps aux | grep 'claude -p' | grep -v grep | wc -l | tr -d ' ') / 2"