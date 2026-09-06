#!/bin/bash
# Usage: log-ccb.sh <event_type> <message>
EVENT=$1
shift
MSG="$*"
STREAM=/Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/state/ccb-stream.log
echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${EVENT}: ${MSG}" >> "$STREAM"
