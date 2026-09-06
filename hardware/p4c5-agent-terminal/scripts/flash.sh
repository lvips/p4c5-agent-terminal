#!/bin/bash
# p4c5-agent-terminal 一键烧录脚本
# 用法: bash scripts/flash.sh [PORT]
#
# 前提：
#   - ESP-IDF 已安装并配置环境变量
#   - 已执行过 idf.py build
#
# 环境变量：
#   IDF_PATH  — ESP-IDF 安装路径
#   PORT      — 串口设备（默认 /dev/cu.usbmodem1401）

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PORT="${1:-${PORT:-/dev/cu.usbmodem1401}}"
BAUD="${BAUD:-921600}"

echo "========================================"
echo " p4c5-agent-terminal Flash Script"
echo "========================================"
echo " Port: $PORT"
echo " Baud: $BAUD"
echo " Project: $PROJECT_DIR"
echo "========================================"

# 检查 ESP-IDF
if [ -z "${IDF_PATH:-}" ]; then
    echo "[WARN] IDF_PATH not set. Trying default..."
    export IDF_PATH="/Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5"
    if [ ! -d "$IDF_PATH" ]; then
        echo "[ERROR] ESP-IDF not found at $IDF_PATH"
        echo "Please set IDF_PATH to your ESP-IDF installation."
        exit 1
    fi
fi

# 加载 ESP-IDF 环境
if [ -f "$IDF_PATH/export.sh" ]; then
    echo "Loading ESP-IDF environment..."
    . "$IDF_PATH/export.sh"
fi

cd "$PROJECT_DIR"

# 检查构建产物
if [ ! -f "build/p4c5_agent_terminal.bin" ]; then
    echo "[INFO] No build found. Running idf.py build first..."
    idf.py build
fi

# 烧录
echo ""
echo "[INFO] Flashing to $PORT at ${BAUD}bps..."
idf.py -p "$PORT" -b "$BAUD" flash

echo ""
echo "[INFO] Flash complete!"
echo "[INFO] Use 'idf.py -p $PORT monitor' to view serial output."
