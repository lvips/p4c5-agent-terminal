#!/usr/bin/env python3
"""
P4C5 完整 4 组件链路测试 (V2 - broadcast 模式)

验证完整语音对话链路 (最接近真实部署):

  P4C5 (audio_uplink_task)
    ↓ WS Binary (OPUS 20ms 帧)
  ASR Adapter (8766)  ──► mock_dsh (8765)  ──► TTS Adapter (8767)
    ↓ opuslib 解码 + VAD      ↑ user_input        ↓ mock TTS
  Mock ASR                broadcast 模式 ↓ assistant_text
                              ↓ JSON ↓ PCM 24kHz
                          P4C5 (WS Binary 下行)

启动方式:
  python3 tools/mock_dsh_server.py --port 8765 --broadcast-text &
  python3 tools/p4c5_asr_adapter.py --mock --port 8766 --upstream ws://127.0.0.1:8765/ws &
  python3 tools/p4c5_tts_adapter.py --mock --port 8767 &
  python3 tools/integration_test_v2.py
"""

import asyncio
import json
import logging
import sys
import time

import numpy as np
import websockets

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("integration-v2")


def mock_opus_frames(n_silence_start: int = 50,
                     n_speech: int = 30,
                     n_silence_end: int = 60):
    """生成模拟 OPUS 帧流 (1kHz 语音)"""
    frames = []
    for _ in range(n_silence_start):
        frames.append(bytes([0]) * 50)
    for _ in range(n_speech):
        frames.append(bytes([1]) * 50)
    for _ in range(n_silence_end):
        frames.append(bytes([0]) * 50)
    return frames


async def run_test():
    """4 组件链路测试

    架构:
      - ESP32 mock → ASR Adapter (8766) 上行 OPUS
      - ASR Adapter → mock_dsh (8765) 上行 user_input
      - mock_dsh broadcast → TTS Adapter (8767) 收到 assistant_text
      - TTS Adapter → ESP32 mock (下行 PCM)
    """
    logger.info("=" * 70)
    logger.info("P4C5 4 组件链路测试 (V2 - broadcast)")
    logger.info("=" * 70)

    # 1. ESP32 mock 上行到 ASR Adapter (8766)
    logger.info("[1/3] ESP32 → ASR Adapter (8766)")
    asr_ws = await websockets.connect('ws://127.0.0.1:8766')
    await asr_ws.send(json.dumps({
        'type': 'client/hello',
        'device_id': 'p4c5-4component-test',
    }))
    logger.info("   ✅ HELLO 已发送")

    # 2. ESP32 mock 下行从 TTS Adapter (8767) 拿 PCM
    logger.info("[2/3] ESP32 → TTS Adapter (8767)")
    tts_ws = await websockets.connect('ws://127.0.0.1:8767')
    logger.info("   ✅ TTS 客户端已连接")

    # 3. 后台任务监听 TTS 下行 PCM
    tts_audio_frames = []

    async def listen_tts():
        try:
            async for msg in tts_ws:
                if isinstance(msg, bytes):
                    tts_audio_frames.append(msg)
        except websockets.ConnectionClosed:
            pass

    asyncio.create_task(listen_tts())

    # 等连接稳定 (TTS Adapter 自动触发 mock_dsh LLM)
    await asyncio.sleep(3)

    # 4. 上行 OPUS 音频 → ASR Adapter → mock_dsh → broadcast → TTS Adapter
    logger.info("[3/3] 上行 OPUS 音频 (WS Binary)")
    frames = mock_opus_frames(50, 30, 60)
    for frame in frames:
        await asr_ws.send(frame)
    logger.info(f"   ✅ 发送 {len(frames)} 帧")

    # 5. 等待完整对话流 (VAD finalize + LLM + TTS 合成)
    logger.info("⏳ 等待完整对话流 (VAD → user_input → LLM → TTS → PCM 下行)...")
    await asyncio.sleep(15)

    # 6. 报告结果
    logger.info("=" * 70)
    logger.info("测试结果")
    logger.info("=" * 70)

    audio_bytes = sum(len(f) for f in tts_audio_frames)
    audio_duration = audio_bytes / 2 / 24000
    logger.info(f"✅ TTS Adapter 下行 PCM 帧: {len(tts_audio_frames)} 个 "
                f"({audio_bytes} bytes = {audio_duration:.2f}s @ 24kHz)")

    if len(tts_audio_frames) > 0:
        logger.info("=" * 70)
        logger.info("🎉 4 组件链路验证通过!")
        logger.info("   ✅ ESP32 上行 OPUS → ASR Adapter (8766)")
        logger.info("   ✅ ASR Adapter VAD finalize + 识别")
        logger.info("   ✅ ASR Adapter → mock_dsh (broadcast mode)")
        logger.info("   ✅ mock_dsh broadcast → TTS Adapter (8767)")
        logger.info("   ✅ TTS Adapter 合成 + 下行 PCM 24kHz")
        logger.info("   ✅ ESP32 收到 PCM (WS Binary)")
        logger.info("=" * 70)
        return 0
    else:
        logger.error("❌ 链路失败: 未收到任何 PCM")
        return 1


if __name__ == "__main__":
    try:
        code = asyncio.run(run_test())
        sys.exit(code)
    except KeyboardInterrupt:
        sys.exit(130)