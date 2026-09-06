#!/usr/bin/env python3
"""
ASR 冒烟测试 - 模拟 ESP32 上行 OPUS, 验证 ASR Adapter echo 模式

端到端冒烟测试:
  1. 连接到 ASR Adapter (8766) - 模拟 ESP32
  2. 上行 OPUS 帧 (WS Binary)
  3. 监听下行 assistant_text 帧 (echo 模式)
  4. 打印识别结果

用法:
  # 1. 启动 ASR Adapter (冒烟测试模式)
  python3 tools/p4c5_asr_adapter.py --mock --no-upstream --echo --port 8766

  # 2. 跑冒烟测试
  python3 tools/asr_smoke_test.py

真机测试 (用户 ESP32):
  1. 设置阿里云凭证 + 启动 ASR Adapter (去掉 --mock)
  2. ESP32 录音 + 上行 OPUS
  3. ASR Adapter 调阿里云 NLS 识别
  4. ESP32 串口显示识别结果 (assistant_text echo)
"""

import asyncio
import json
import logging
import sys

import websockets

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("asr-smoke")


def mock_opus_silence():
    return bytes([0]) * 50


def mock_opus_speech():
    return bytes([1]) * 50


async def run_smoke_test():
    """ASR 冒烟测试 (模拟 ESP32 + 监听 echo)"""
    logger.info("=" * 60)
    logger.info("ASR 冒烟测试 (模拟 ESP32 → ASR Adapter echo)")
    logger.info("=" * 60)

    ws = await websockets.connect('ws://127.0.0.1:8766')
    logger.info("✅ 已连接到 ASR Adapter 8766")

    await ws.send(json.dumps({
        'type': 'client/hello',
        'device_id': 'p4c5-smoke-test',
    }))

    echo_messages = []

    async def listen_echo():
        """后台任务: 监听下行 echo 帧"""
        try:
            async for msg in ws:
                if isinstance(msg, str):
                    try:
                        data = json.loads(msg)
                        if data.get('echo') or data.get('type') == 'assistant_text':
                            content = data.get('content', '')
                            echo_messages.append(content)
                            logger.info(f"📥 ECHO 收到: \"{content}\"")
                    except json.JSONDecodeError:
                        pass
        except websockets.ConnectionClosed:
            pass

    listen_task = asyncio.create_task(listen_echo())
    # 给 listen 任务时间开始
    await asyncio.sleep(0.3)

    # 模拟 ESP32 真实发送间隔 (20ms/帧 = 0.020s)
    FRAME_INTERVAL = 0.020

    # 阶段 1: 50 帧静默 (1s)
    logger.info("[1/3] 上行 50 帧静默...")
    for _ in range(50):
        await ws.send(mock_opus_silence())
        await asyncio.sleep(FRAME_INTERVAL)

    # 阶段 2: 30 帧语音 (600ms)
    logger.info("[2/3] 上行 30 帧语音...")
    for _ in range(30):
        await ws.send(mock_opus_speech())
        await asyncio.sleep(FRAME_INTERVAL)

    # 阶段 3: 80 帧静默 (1.6s) - 触发 VAD finalize
    logger.info("[3/3] 上行 80 帧静默 (等 VAD finalize)...")
    for _ in range(80):
        await ws.send(mock_opus_silence())
        await asyncio.sleep(FRAME_INTERVAL)

    # 等 echo
    logger.info("⏳ 等待 echo 文本...")
    for _ in range(30):  # 最多等 3s
        if echo_messages:
            break
        await asyncio.sleep(0.1)

    listen_task.cancel()
    await ws.close()

    logger.info("=" * 60)
    if echo_messages:
        logger.info(f"🎉 冒烟测试通过! 收到 {len(echo_messages)} 条 echo:")
        for msg in echo_messages:
            logger.info(f"   📝 \"{msg}\"")
        logger.info("=" * 60)
        return 0
    else:
        logger.error("❌ 冒烟测试失败: 没收到 echo")
        logger.error("   可能原因:")
        logger.error("   1. ASR Adapter 没启动 echo 模式 (需要 --echo)")
        logger.error("   2. VAD 未触发 (RMS 帧数不够)")
        logger.error("   3. Mock decoder 与 ASR Adapter 不一致")
        logger.info("=" * 60)
        return 1


if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(run_smoke_test()))
    except KeyboardInterrupt:
        sys.exit(130)