#!/bin/bash
# ============================================================================
# cc-orchestrator.sh - CC 长驻编排主控
#
# 用法: bash cc-orchestrator.sh <start|stop|status|restart|dispatch>
#
#   start             启动 cca + ccb (后台 daemon 模式)
#   stop              停止 cca + ccb
#   status            查询状态 (转发到 cc-status.sh)
#   restart <role>    重启指定 CC (保留 session-id)
#   dispatch <role> <prompt-file>
#                     同步派发任务 (等结果)
#
# 设计: 主控只是脚手架, 真正的工作在 cc-spawn/cc-kill/cc-status/cc-dispatch
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
ORCH_DIR="${PROJ_DIR}/.dsh-orchestration"
BIN_DIR="${ORCH_DIR}/bin"
LOG_DIR="${ORCH_DIR}/logs"
STATE_DIR="${ORCH_DIR}/state"

CMD="${1:-status}"

log() {
  local MSG="${1}"
  echo "[orch] ${MSG}"
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${MSG}" >> "${LOG_DIR}/orchestrator.log"
}

case "${CMD}" in
  start)
    log "starting cca + ccb..."
    bash "${BIN_DIR}/cc-spawn.sh" cca
    bash "${BIN_DIR}/cc-spawn.sh" ccb
    log "start complete"
    echo ""
    bash "${0}" status
    ;;

  stop)
    log "stopping cca + ccb..."
    for r in cca ccb; do
      bash "${BIN_DIR}/cc-kill.sh" "${r}" || true
    done
    log "stop complete"
    ;;

  status)
    bash "${BIN_DIR}/cc-status.sh"
    ;;

  restart)
    ROLE="${2:?用法: $0 restart <cca|ccb>}"
    log "restarting ${ROLE}..."
    bash "${BIN_DIR}/cc-kill.sh" "${ROLE}" || true
    sleep 2
    bash "${BIN_DIR}/cc-spawn.sh" "${ROLE}"
    log "restart ${ROLE} complete"
    ;;

  dispatch)
    # 同步派发任务: 等 CC 完成
    ROLE="${2:?用法: $0 dispatch <cca|ccb> <prompt-file>}"
    PROMPT_FILE="${3:?用法: $0 dispatch <cca|ccb> <prompt-file>}"
    ROLE_FLAG=$([ "${ROLE}" = "cca" ] && echo "manager" || echo "assistant")
    TASK_NAME="dispatch-$(date +%s)"
    log "dispatching to ${ROLE} (${ROLE_FLAG}): task=${TASK_NAME}"
    bash "${BIN_DIR}/cc-dispatch.sh" "${PROJ_DIR}" "${PROMPT_FILE}" "${TASK_NAME}" --role "${ROLE_FLAG}"
    ;;

  handoff)
    ROLE="${2:?用法: $0 handoff <cca|ccb>}"
    log "handoff for ${ROLE}..."
    bash "${BIN_DIR}/cc-handoff.sh" "${ROLE}"
    ;;

  *)
    echo "用法: $0 <start|stop|status|restart <role>|dispatch <role> <prompt>|handoff <role>>"
    exit 1
    ;;
esac