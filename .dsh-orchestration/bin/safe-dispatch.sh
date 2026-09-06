#!/bin/bash
# safe-dispatch.sh — DSH 安全派发 claude-p
# 加 30s 重试 + 间隔保护，避免触发 Claude Code 速率限制

RETRY_INTERVAL=30
MAX_RETRIES=3

dispatch_with_retry() {
    local PROMPT_FILE="$1"
    local SID="$2"
    local EXTRA_ARGS="${@:3}"

    for attempt in $(seq 1 $MAX_RETRIES); do
        echo "[$(date '+%H:%M:%S')] attempt $attempt/$MAX_RETRIES"
        cat "$PROMPT_FILE" | timeout 1800 claude -p \
            --resume "$SID" \
            --system-prompt-file /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/templates/cca-system-prompt.md \
            --add-dir /Volumes/ZT-1T/项目开发/ESP32-P4C5 \
            --add-dir /Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup \
            --output-format text \
            --dangerously-skip-permissions $EXTRA_ARGS 2>&1 | tail -30

        local EXIT=$?
        if [ $EXIT -eq 0 ]; then
            echo "[$(date '+%H:%M:%S')] success"
            return 0
        fi

        if grep -q "429\|quota" <(cat "$PROMPT_FILE" | timeout 1800 claude -p --resume "$SID" --system-prompt-file /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/templates/cca-system-prompt.md --add-dir /Volumes/ZT-1T/项目开发/ESP32-P4C5 --dangerously-skip-permissions 2>&1); then
            echo "[$(date '+%H:%M:%S')] 429 detected, sleeping ${RETRY_INTERVAL}s..."
            sleep $RETRY_INTERVAL
        else
            echo "[$(date '+%H:%M:%S')] non-429 error, exit=$EXIT"
            return $EXIT
        fi
    done
    return 1
}

# Main
PROMPT_FILE="${1:-/tmp/cca_t18c_xiaozhi_power.md}"
SID=$(cat /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/state/cca.session-id)
dispatch_with_retry "$PROMPT_FILE" "$SID"