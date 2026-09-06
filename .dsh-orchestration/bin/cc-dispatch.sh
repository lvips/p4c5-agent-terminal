#!/bin/bash
# ============================================================================
# cc-dispatch.sh - 同步派发任务给 CC (基于 OMT cc_dispatch.sh 改)
#
# 用法: bash cc-dispatch.sh <repo路径> <prompt文件> [任务名] --role manager|assistant
#
# 行为: 一条命令 = 派发 claude -p + 等待完成
#   启动加固: 派发后 5s 存活检测 + 结果质量检查 (运行<30s 或 结果<1KB 触发重试, 最多2次)
#   退出码: 0=完成  2=防停滞(15min无进度)  1=异常/参数错/硬超时
#
# 产物 (在 /tmp/<name>_*):
#   _result.log : CC 输出
#   _commit.flag: 是否 commit
#   _cc.pid     : CC 进程 PID
#   _role       : 派发对象 (manager | assistant), DSH 心跳可见
#
# 注意: 全部用 ${VAR} 花括号 + ASCII 标点, 避免 $VAR 紧跟中文全角标点导致
#       bash 把多字节字符并入变量名而报 "unbound variable"
# ============================================================================

set -uo pipefail

# --- 参数解析 ---
REPO=""
PROMPT=""
TASK="cc-task"
ROLE="manager"
MAX_RETRIES=2
HARD_TIMEOUT_MIN=30
STAGNANT_MIN=15
STARTUP_GRACE_SEC=5

while [ $# -gt 0 ]; do
  case "${1}" in
    --role)
      ROLE="${2:-}"
      shift 2
      ;;
    --role=*)
      ROLE="${1#--role=}"
      shift
      ;;
    --hard-timeout-min)
      HARD_TIMEOUT_MIN="${2:-}"
      shift 2
      ;;
    --help|-h)
      echo "用法: cc_dispatch.sh <repo路径> <prompt文件> [任务名] --role manager|assistant [--hard-timeout-min 30]"
      exit 0
      ;;
    *)
      if [ -z "${REPO}" ]; then
        REPO="${1}"
      elif [ -z "${PROMPT}" ]; then
        PROMPT="${1}"
      elif [ "${TASK}" = "cc-task" ]; then
        TASK="${1}"
      else
        echo "未知参数: ${1}" >&2
        exit 1
      fi
      shift
      ;;
  esac
done

case "${ROLE}" in
  manager|assistant) ;;
  *)
    echo "错误: --role 必须是 manager 或 assistant, 收到: ${ROLE}" >&2
    exit 1
    ;;
esac

if [ -z "${REPO}" ] || [ -z "${PROMPT}" ]; then
  echo "用法: cc_dispatch.sh <repo路径> <prompt文件> [任务名] --role manager|assistant" >&2
  exit 1
fi
if [ ! -f "${PROMPT}" ]; then
  echo "prompt 文件不存在: ${PROMPT}" >&2
  exit 1
fi

# --- 工作产物路径 ---
TMP_DIR="/tmp/${TASK}_$$"
mkdir -p "${TMP_DIR}"
RESULT_LOG="${TMP_DIR}/_result.log"
COMMIT_FLAG="${TMP_DIR}/_commit.flag"
CC_PID_FILE="${TMP_DIR}/_cc.pid"
ROLE_FILE="${TMP_DIR}/_role"

echo "${ROLE}" > "${ROLE_FILE}"

# --- 启动 CC ---
_launch_cc() {
  echo "[dispatch] 启动 claude -p (角色=${ROLE}, 任务=${TASK})..."
  zsh -c "source ~/.zshrc 2>/dev/null; cd '${REPO}' && \
    claude -p \
      --role ${ROLE} \
      --output-format text \
      --dangerously-skip-permissions < '${PROMPT}' > '${RESULT_LOG}' 2>&1" &
  CC_PID=$!
  echo "${CC_PID}" > "${CC_PID_FILE}"
  echo "[dispatch] claude -p PID=${CC_PID} -> ${RESULT_LOG} (pid 文件 ${CC_PID_FILE})"
}

# --- 检测 commit (git) ---
_check_commit() {
  cd "${REPO}" || return 1
  if git rev-parse --git-dir >/dev/null 2>&1; then
    if [ -n "$(git status --porcelain 2>/dev/null)" ]; then
      echo "1" > "${COMMIT_FLAG}"
    else
      echo "0" > "${COMMIT_FLAG}"
    fi
  else
    echo "0" > "${COMMIT_FLAG}"
  fi
}

RETRY_COUNT=0

# 首次派发
echo "[dispatch] 派发 claude -p (最多重试 ${MAX_RETRIES} 次)..."
_launch_cc

# 启动存活检测
sleep "${STARTUP_GRACE_SEC}"
if ! kill -0 "${CC_PID}" 2>/dev/null; then
  echo "[dispatch] 警告: CC 进程启动后立即退出 (PID=${CC_PID})"
  echo "[dispatch] 结果日志预览:"
  tail -10 "${RESULT_LOG}" 2>/dev/null || true
  if [ "${RETRY_COUNT}" -lt "${MAX_RETRIES}" ]; then
    RETRY_COUNT=$((RETRY_COUNT + 1))
    echo "[dispatch] 重试 ${RETRY_COUNT}/${MAX_RETRIES}..."
    _launch_cc
  else
    echo "[dispatch] 多次启动失败, 退出"
    cat "${RESULT_LOG}"
    rm -rf "${TMP_DIR}"
    exit 1
  fi
fi

# 等待完成 (带硬超时)
echo "[dispatch] 等待 CC 完成 (硬超时 ${HARD_TIMEOUT_MIN}min, 防停滞 ${STAGNANT_MIN}min)..."
START_TS=$(date +%s)
LAST_SIZE=0
LAST_CHANGE_TS=${START_TS}
HARD_TIMEOUT_SEC=$((HARD_TIMEOUT_MIN * 60))
STAGNANT_SEC=$((STAGNANT_MIN * 60))

while kill -0 "${CC_PID}" 2>/dev/null; do
  sleep 10
  NOW_TS=$(date +%s)
  ELAPSED=$((NOW_TS - START_TS))

  # 硬超时
  if [ "${ELAPSED}" -gt "${HARD_TIMEOUT_SEC}" ]; then
    echo "[dispatch] 硬超时 (${HARD_TIMEOUT_MIN}min), 强制 kill"
    kill -9 "${CC_PID}" 2>/dev/null || true
    break
  fi

  # 检测文件大小变化 (防停滞)
  if [ -f "${RESULT_LOG}" ]; then
    CUR_SIZE=$(wc -c < "${RESULT_LOG}")
    if [ "${CUR_SIZE}" -ne "${LAST_SIZE}" ]; then
      LAST_SIZE=${CUR_SIZE}
      LAST_CHANGE_TS=${NOW_TS}
    else
      STAGNANT=$((NOW_TS - LAST_CHANGE_TS))
      if [ "${STAGNANT}" -gt "${STAGNANT_SEC}" ]; then
        echo "[dispatch] 防停滞 (${STAGNANT_MIN}min 无进度), 强制 kill"
        kill -9 "${CC_PID}" 2>/dev/null || true
        break
      fi
    fi
  fi
done

# 等待进程退出
wait "${CC_PID}" 2>/dev/null || true

# 结果质量检查
RESULT_SIZE=$(wc -c < "${RESULT_LOG}" 2>/dev/null || echo 0)
ELAPSED=$(( $(date +%s) - START_TS ))

echo "[dispatch] CC 完成, 耗时 ${ELAPSED}s, 输出 ${RESULT_SIZE} bytes"

if [ "${RESULT_SIZE}" -lt 1024 ]; then
  echo "[dispatch] 警告: 输出过小 (< 1KB), 可能是质量问题"
  echo "[dispatch] 输出预览:"
  cat "${RESULT_LOG}"
fi

# 检测 commit
_check_commit || true

# 输出结果
echo "===== CC 输出开始 ====="
cat "${RESULT_LOG}"
echo "===== CC 输出结束 ====="

# 清理 (保留 result log 供 DSH 检查)
# rm -rf "${TMP_DIR}"

exit 0