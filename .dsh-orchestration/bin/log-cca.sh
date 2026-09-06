#!/bin/bash
# Usage: log-cca.sh <event_type> <message>
# event_type: 派发 / 完成 / 失败 / killed
EVENT=$1
shift
MSG="$*"
STREAM=/Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/state/cca-stream.log
echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${EVENT}: ${MSG}" >> "$STREAM"
