#!/bin/bash
# DSH CC 状态轮询脚本
# 每 3 分钟执行一次
# 用法: nohup bash .dsh-orchestration/bin/cc-poll.sh &

POLL_INTERVAL=180  # 3 分钟
LOG_DIR=/Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/state
CCA_STREAM=$LOG_DIR/cca-stream.log
CCB_STREAM=$LOG_DIR/ccb-stream.log
POLL_LOG=$LOG_DIR/poll.log

echo "[$(date '+%Y-%m-%d %H:%M:%S')] 🔄 CC 轮询启动 (每 ${POLL_INTERVAL}s)" >> "$POLL_LOG"

while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

    # 检查 CCA 进程
    CCA_PID=$(pgrep -f "claude -p.*cca-system-prompt" 2>/dev/null | head -1)
    if [ -n "$CCA_PID" ]; then
        CCA_ELAPSED=$(ps -o etime= -p "$CCA_PID" 2>/dev/null | tr -d ' ')
        CCA_CPU=$(ps -o pcpu= -p "$CCA_PID" 2>/dev/null | tr -d ' ')
        CCA_TASK=$(ps -o command= -p "$CCA_PID" 2>/dev/null | grep -oE "tmp/[a-z0-9_]+\.md" | head -1)
        echo "[$TIMESTAMP] 🟡 CCA 运行中 PID=$CCA_PID elapsed=$CCA_ELAPSED cpu=$CCA_CPU% task=$CCA_TASK" >> "$CCA_STREAM"
    else
        echo "[$TIMESTAMP] ⏸️ CCA idle (等待派活)" >> "$CCA_STREAM"
    fi

    # 检查 CCB 进程
    CCB_PID=$(pgrep -f "claude -p.*ccb-system-prompt" 2>/dev/null | head -1)
    if [ -n "$CCB_PID" ]; then
        CCB_ELAPSED=$(ps -o etime= -p "$CCB_PID" 2>/dev/null | tr -d ' ')
        CCB_CPU=$(ps -o pcpu= -p "$CCB_PID" 2>/dev/null | tr -d ' ')
        CCB_TASK=$(ps -o command= -p "$CCB_PID" 2>/dev/null | grep -oE "tmp/[a-z0-9_]+\.md" | head -1)
        echo "[$TIMESTAMP] 🟡 CCB 运行中 PID=$CCB_PID elapsed=$CCB_ELAPSED cpu=$CCB_CPU% task=$CCB_TASK" >> "$CCB_STREAM"
    else
        echo "[$TIMESTAMP] ⏸️ CCB idle (等待派活)" >> "$CCB_STREAM"
    fi

    # 统计 commit 数
    COMMIT_COUNT=$(git -C /Volumes/ZT-1T/项目开发/ESP32-P4C5 log --oneline 2>/dev/null | wc -l | tr -d ' ')
    echo "[$TIMESTAMP] 📊 当前总 commit: $COMMIT_COUNT" >> "$POLL_LOG"

    sleep $POLL_INTERVAL
done