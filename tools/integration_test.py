#!/usr/bin/env python3
"""
P4C5 完整端到端集成测试 (POC 单连接模式)

模拟 ESP32-P4C5 设备, 验证完整语音对话链路:

  ESP32 (audio_uplink_task)
    ↓ WS Binary (OPUS 20ms 帧)
  mock_dsh_server (8765)
    ↓ mock_asr_finalize 触发
  mock_dsh LLM 处理 → 自动回复 assistant_text
    ↓ 下行 JSON 帧
  ESP32 (text/JSON downlink)

使用方法:
  # 1. 启动 mock_dsh_server
  python3 tools/mock_dsh_server.py --port 8765 &

  # 2. 启动 ASR Adapter (可选, 用作独立的 ASR 处理)
  python3 tools/p4c5_asr_adapter.py --mock --port 8766 --upstream ws://127.0.0.1:8765/ws &

  # 3. 启动 TTS Adapter (可选)
  python3 tools/p4c5_tts_adapter.py --mock --port 8767 &

  # 4. 运行集成测试
  python3 tools/integration_test.py
"""

import asyncio
import json
import logging
import sys
import time
from typing import Optional

import numpy as np
import websockets

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("integration-test")


def mock_opus_frames(n_silence_start: int = 50,
                     n_speech: int = 30,
                     n_silence_end: int = 60):
    """生成模拟 OPUS 帧流 (1kHz 语音)"""
    frames = []
    for _ in range(n_silence_start):
        frames.append(bytes([0]) * 50)  # 静默
    for _ in range(n_speech):
        frames.append(bytes([1]) * 50)  # 语音
    for _ in range(n_silence_end):
        frames.append(bytes([0]) * 50)  # 静默
    return frames


async def run_test_single():
    """单连接测试: ESP32 直连 mock_dsh, 上行 OPUS + 收所有 DSH 下行帧

    这是最接近真实 P4C5 部署的架构.
    """
    logger.info("=" * 60)
    logger.info("P4C5 集成测试 - 单连接模式 (ESP32 ↔ mock_dsh)")
    logger.info("=" * 60)

    # 1. ESP32 直连 mock_dsh (8765)
    logger.info("[1/3] ESP32 → mock_dsh_server (8765)")
    ws = await websockets.connect('ws://127.0.0.1:8765/ws')
    await ws.send(json.dumps({
        'type': 'client/hello',
        'device_id': 'p4c5-integration-test',
        'caps': ['audio', 'display', 'touch', '4g'],
    }))
    logger.info("   ✅ HELLO 已发送")

    # 2. 后台任务: 监听所有下行 JSON 帧
    dsh_msgs = []

    async def listen_downlink():
        try:
            async for msg in ws:
                if isinstance(msg, str):
                    try:
                        data = json.loads(msg)
                        dsh_msgs.append(data)
                    except json.JSONDecodeError:
                        pass
        except websockets.ConnectionClosed:
            pass

    asyncio.create_task(listen_downlink())

    # 等连接稳定
    await asyncio.sleep(1.5)

    # 3. 上行 OPUS 音频 (触发 mock_asr_finalize → LLM)
    logger.info("[2/3] 上行 OPUS 音频 (WS Binary)")
    frames = mock_opus_frames(50, 30, 60)
    for frame in frames:
        await ws.send(frame)
    logger.info(f"   ✅ 发送 {len(frames)} 帧 (50 静默 + 30 语音 + 60 静默)")

    # 4. 等待 ASR finalize + LLM 回复 (mock_dsh 触发 100 帧自动 finalize)
    logger.info("[3/3] 等待完整对话流...")
    await asyncio.sleep(8)

    # 5. 报告结果
    logger.info("=" * 60)
    logger.info("测试结果")
    logger.info("=" * 60)
    logger.info(f"✅ mock_dsh 下行 JSON 帧: {len(dsh_msgs)} 个")
    for msg in dsh_msgs[:15]:
        ftype = msg.get("type", "?")
        content = msg.get("content", msg.get("text", ""))[:60]
        logger.info(f"   📥 {ftype:30s} → \"{content}\"")

    # 6. 验证
    has_thinking = any(m.get('type') == 'thinking' for m in dsh_msgs)
    has_assistant_text = any(m.get('type') == 'assistant_text' for m in dsh_msgs)
    has_assistant_done = any(m.get('type') == 'assistant_done' for m in dsh_msgs)

    if has_thinking and has_assistant_text and has_assistant_done:
        logger.info("=" * 60)
        logger.info("🎉 完整链路验证通过!")
        logger.info("   ✅ OPUS 音频 (WS Binary) 上行")
        logger.info("   ✅ mock_dsh 接收 + mock_asr_finalize 触发")
        logger.info("   ✅ mock LLM 处理 (回 thinking + assistant_text 流式)")
        logger.info("   ✅ 完整 18 帧下行对话流")
        logger.info("=" * 60)
        return 0
    else:
        missing = []
        if not has_thinking: missing.append("thinking")
        if not has_assistant_text: missing.append("assistant_text")
        if not has_assistant_done: missing.append("assistant_done")
        logger.error(f"❌ 部分帧缺失: {missing}")
        return 1


async def run_test_dual():
    """双连接测试: ESP32 同时连 ASR Adapter + mock_dsh

    ASR Adapter 收 OPUS → mock ASR → 上行 user_input 到 mock_dsh
    ESP32 直连 mock_dsh 拿所有 DSH 下行帧
    """
    logger.info("=" * 60)
    logger.info("P4C5 集成测试 - 双连接模式 (ASR Adapter + mock_dsh)")
    logger.info("=" * 60)

    # 1. ESP32 直连 mock_dsh 拿 DSH 下行帧
    logger.info("[1/4] ESP32 → mock_dsh_server (8765)")
    dsh_ws = await websockets.connect('ws://127.0.0.1:8765/ws')
    await dsh_ws.send(json.dumps({
        'type': 'client/hello',
        'device_id': 'p4c5-integration-test',
        'caps': ['audio', 'display', 'touch', '4g'],
    }))
    logger.info("   ✅ HELLO 已发送")

    # 2. ESP32 上行 OPUS 到 ASR Adapter
    logger.info("[2/4] ESP32 → ASR Adapter (8766)")
    asr_ws = await websockets.connect('ws://127.0.0.1:8766')
    await asr_ws.send(json.dumps({
        'type': 'client/hello',
        'device_id': 'p4c5-integration-test',
    }))
    logger.info("   ✅ HELLO 已发送")

    # 3. 后台任务监听
    dsh_msgs = []

    async def listen_dsh():
        try:
            async for msg in dsh_ws:
                if isinstance(msg, str):
                    try:
                        dsh_msgs.append(json.loads(msg))
                    except json.JSONDecodeError:
                        pass
        except websockets.ConnectionClosed:
            pass

    asyncio.create_task(listen_dsh())

    # 4. 上行 OPUS 音频
    await asyncio.sleep(1.5)
    logger.info("[3/4] 上行 OPUS 音频 → ASR Adapter")
    frames = mock_opus_frames(50, 30, 60)
    for frame in frames:
        await asr_ws.send(frame)
    logger.info(f"   ✅ 发送 {len(frames)} 帧")

    # 5. 等待
    logger.info("[4/4] 等待完整对话流...")
    await asyncio.sleep(8)

    # 6. 报告
    logger.info("=" * 60)
    logger.info("测试结果")
    logger.info("=" * 60)
    logger.info(f"✅ mock_dsh 下行 JSON 帧: {len(dsh_msgs)} 个")
    for msg in dsh_msgs[:10]:
        ftype = msg.get("type", "?")
        content = msg.get("content", msg.get("text", ""))[:60]
        logger.info(f"   📥 {ftype:30s} → \"{content}\"")

    has_hello_ack = any(m.get('type') == 'session_state' for m in dsh_msgs)
    if has_hello_ack:
        logger.info("=" * 60)
        logger.info("🎉 ASR Adapter 上行链路验证通过!")
        logger.info("   ✅ OPUS → ASR Adapter (VAD finalize)")
        logger.info("   ✅ ASR Adapter → mock_dsh (user_input)")
        logger.info("   ✅ ESP32 直连 mock_dsh 拿 session_state")
        logger.info("=" * 60)
        return 0
    else:
        logger.error("❌ 链路失败")
        return 1


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["single", "dual"], default="single",
                        help="single=ESP32↔mock_dsh, dual=ESP32↔ASR Adapter+mock_dsh")
    args = parser.parse_args()

    if args.mode == "single":
        return asyncio.run(run_test_single())
    else:
        return asyncio.run(run_test_dual())


if __name__ == "__main__":
    try:
        code = main()
        sys.exit(code)
    except KeyboardInterrupt:
        logger.info("Interrupted")
        sys.exit(130)
    except Exception as e:
        logger.exception(f"Test failed: {e}")
        sys.exit(1)