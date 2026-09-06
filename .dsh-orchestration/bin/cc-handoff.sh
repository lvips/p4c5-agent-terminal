#!/bin/bash
# ============================================================================
# cc-handoff.sh - HANDOFF 机制: 压缩 CC 上下文并重启
#
# 用法: bash cc-handoff.sh <cca|ccb>
#
# 行为:
#   1. 让 CC 输出"工作摘要 + 文件变更 + 未决问题"（通过 stdin 喂 prompt）
#   2. 写 docs/handovers/HANDOFF-<role>-<ts>.md
#   3. kill 旧 CC
#   4. spawn 新 CC (用新 session-id)
#   5. 新 CC 启动后, DSH 在下一次 dispatch 时把 HANDOFF 作为上下文喂给它
#
# 注意: 这个脚本本身不读取 HANDOFF (那是 DSH 的事). 它只生成 HANDOFF.
# ============================================================================

set -uo pipefail

PROJ_DIR="/Volumes/ZT-1T/项目开发/ESP32-P4C5"
ORCH_DIR="${PROJ_DIR}/.dsh-orchestration"
HANDOVER_DIR="${PROJ_DIR}/docs/handovers"
TPL_DIR="${ORCH_DIR}/templates"

ROLE="${1:-}"

if [ -z "${ROLE}" ] || { [ "${ROLE}" != "cca" ] && [ "${ROLE}" != "ccb" ]; }; then
  echo "用法: $0 <cca|ccb>" >&2
  exit 1
fi

TS=$(date +%Y%m%d-%H%M%S)
HANDOFF_FILE="${HANDOVER_DIR}/HANDOFF-${ROLE}-${TS}.md"
TEMPLATE="${TPL_DIR}/HANDOFF-template.md"

# 1. 生成 HANDOFF prompt (让 CC 输出摘要)
PROMPT_TMP=$(mktemp /tmp/handoff-prompt.XXXXXX)
cat > "${PROMPT_TMP}" <<EOF
请生成一份 HANDOFF 文档, 用于交接给下一个接手的 CC 会话。

输出格式 (严格按以下结构, Markdown):

$(cat "${TEMPLATE}")

请填写所有字段, 然后输出完整的 Markdown 内容 (不要用代码块包裹, 直接输出)。EOF

# 2. 调用 CC 生成 HANDOFF 内容 (使用 dispatch 模式)
RESULT_TMP=$(mktemp /tmp/handoff-result.XXXXXX)
echo "[handoff] 让 ${ROLE} 生成 HANDOFF 内容..."
bash "${ORCH_DIR}/bin/cc-dispatch.sh" "${PROJ_DIR}" "${PROMPT_TMP}" "handoff-${ROLE}-${TS}" --role "$( [ ${ROLE} = cca ] && echo manager || echo assistant )" \
  > "${RESULT_TMP}" 2>&1 || true

# 3. 写入 HANDOFF 文件
{
  echo "# HANDOFF ${ROLE} — $(date '+%Y-%m-%d %H:%M:%S')"
  echo ""
  echo "> **角色**: ${ROLE} ( $([ ${ROLE} = cca ] && echo '科研主管' || echo '科研助理') )"
  echo "> **时间**: $(date '+%Y-%m-%d %H:%M:%S')"
  echo ""
  echo "---"
  echo ""
  cat "${RESULT_TMP}" 2>/dev/null || echo "(CC 未生成内容, 手动填写)"
} > "${HANDOFF_FILE}"

echo "[handoff] 写入: ${HANDOFF_FILE}"

# 4. 清理临时
rm -f "${PROMPT_TMP}" "${RESULT_TMP}"

# 5. kill 旧 CC
echo "[handoff] 停止 ${ROLE}..."
bash "${ORCH_DIR}/bin/cc-kill.sh" "${ROLE}" || true
sleep 2

# 6. spawn 新 CC (新 session-id)
echo "[handoff] 启动新 ${ROLE} (--force-fresh)..."
bash "${ORCH_DIR}/bin/cc-spawn.sh" "${ROLE}" --force-fresh

# 7. 更新 docs/handovers/HANDOFF.md 指向最新
LATEST_LINK="${HANDOVER_DIR}/HANDOFF-${ROLE}-latest.md"
cp "${HANDOFF_FILE}" "${LATEST_LINK}"
echo "[handoff] 最新 HANDOFF 链接: ${LATEST_LINK}"

echo "[handoff] 完成。DSH 在下一次 dispatch ${ROLE} 时, 应把以下文件作为上下文:"
echo "  - ${HANDOFF_FILE}"
echo "  - ${LATEST_LINK}"