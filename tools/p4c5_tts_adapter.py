#!/usr/bin/env python3
"""
P4C5 TTS Adapter — Mac 端语音合成代理 (阿里百炼 DashScope CosyVoice)

接收 mock_dsh_server.py (或其他 DSH Adapter) 下行的 assistant_text 帧
  → 调用阿里百炼 DashScope CosyVoice 语音合成 (REST)
  → 接收 PCM 24kHz 音频
  → 下行 WS Binary 给 ESP32-P4C5 ES8311 DAC 播放

设计参考:
  - 阿里百炼 TTS: https://dashscope.aliyuncs.com/api/v1/services/audio/tts/generation
  - CosyVoice 模型: cosyvoice-v1 (支持 49+ 中文音色)
  - 采样率 24kHz 与 ESP32 ES8311 默认一致

OMT 项目没有 TTS 实现, 这是 P4C5 扩展。
OMT 仅做 ASR (阿里云 NLS ISI), 用户需求是双向语音对话故加 TTS。

使用方法:
  # Mock 模式 (不需要 key)
  python3 tools/p4c5_tts_adapter.py --mock

  # 真实 TTS 模式 (需要阿里百炼 API key)
  export DASHSCOPE_API_KEY=sk-xxx
  python3 tools/p4c5_tts_adapter.py --port 8767

架构:
  mock_dsh_server (8765) ──WS JSON (assistant_text)──> 本 TTS Adapter
                                                          ↓ REST
                                                       阿里百炼 CosyVoice
                                                          ↓ PCM 24kHz
  ESP32 (8767) ──WS Binary (PCM 下行)─────────────────────────┘
"""

import argparse
import asyncio
import base64
import json
import logging
import os
import re
import sys
import time
from typing import Optional, Callable

import numpy as np
import websockets

# HTTP 客户端 (调用阿里百炼 REST)
try:
    import requests  # type: ignore
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False
    logging.warning("requests 未安装, 真实 TTS 需要: pip3 install requests")

# ══════════════════════════════════════════════════════════
# 配置
# ══════════════════════════════════════════════════════════

# 阿里百炼 TTS
DASHSCOPE_TTS_URL = "https://dashscope.aliyuncs.com/api/v1/services/audio/tts/generation"
DASHSCOPE_DEFAULT_MODEL = "cosyvoice-v1"
DASHSCOPE_DEFAULT_VOICE = "longxiaochun"  # 龙小淳, 中文女声, 阿里百炼默认

# 音频参数 (与 ESP32 ES8311 24kHz 默认匹配)
TTS_SAMPLE_RATE = 24000
TTS_CHANNELS = 1
TTS_BITS_PER_SAMPLE = 16
TTS_FRAME_MS = 60                     # 60ms / 帧 (与 ESP32 tts_player 队列一致)
TTS_FRAME_SAMPLES = TTS_SAMPLE_RATE * TTS_FRAME_MS // 1000  # 1440 samples
TTS_FRAME_BYTES = TTS_FRAME_SAMPLES * TTS_BITS_PER_SAMPLE // 8  # 2880 bytes

# 日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("p4c5-tts")
logger.setLevel(logging.INFO)
logger.propagate = True


# ══════════════════════════════════════════════════════════
# TTS 引擎抽象
# ══════════════════════════════════════════════════════════

class TTSEngine:
    """TTS 引擎基类"""

    async def synthesize(self, text: str, on_audio_chunk: Callable) -> bool:
        """合成文字 → 音频流 (回调下发)

        on_audio_chunk: async callback(pcm_bytes) -> None
        返回: True=成功
        """
        raise NotImplementedError


class MockTTSEngine(TTSEngine):
    """Mock TTS (无 key 时)

    生成 800Hz 正弦波代替语音, 用于联调测试
    """

    def __init__(self):
        self.call_count = 0

    async def synthesize(self, text: str, on_audio_chunk: Callable) -> bool:
        self.call_count += 1
        logger.info(f"🔊 [mock-tts #{self.call_count}] 合成: \"{text[:50]}\"")

        # 生成 1 秒音频 = ~17 帧 @ 60ms
        n_frames = 17
        for i in range(n_frames):
            t = np.arange(TTS_FRAME_SAMPLES, dtype=np.float32) + i * TTS_FRAME_SAMPLES
            # beep 音: 800Hz 正弦波
            wave = (np.sin(2 * np.pi * 800 * t / TTS_SAMPLE_RATE) * 4000).astype(np.int16)
            pcm = wave.tobytes()
            await on_audio_chunk(pcm)
            await asyncio.sleep(0.01)  # 模拟网络延迟

        return True


class CosyVoiceTTSEngine(TTSEngine):
    """阿里百炼 DashScope CosyVoice 语音合成

    REST POST:
      POST https://dashscope.aliyuncs.com/api/v1/services/audio/tts/generation
      Headers:
        Authorization: Bearer ${DASHSCOPE_API_KEY}
        Content-Type: application/json
      Body:
        {
          "model": "cosyvoice-v1",
          "voice": "longxiaochun",
          "input": {"text": "..."},
          "parameters": {"format": "pcm", "sample_rate": 24000}
        }

    Response:
      {
        "output": {
          "audio": {
            "data": "<base64 PCM bytes>",
            "sample_rate": 24000
          }
        },
        "usage": {...}
      }
    """

    def __init__(self, api_key: str, model: str = DASHSCOPE_DEFAULT_MODEL,
                 voice: str = DASHSCOPE_DEFAULT_VOICE,
                 sample_rate: int = TTS_SAMPLE_RATE):
        self.api_key = api_key
        self.model = model
        self.voice = voice
        self.sample_rate = sample_rate
        self.call_count = 0

    async def synthesize(self, text: str, on_audio_chunk: Callable) -> bool:
        self.call_count += 1

        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        body = {
            "model": self.model,
            "voice": self.voice,
            "input": {"text": text},
            "parameters": {
                "format": "pcm",
                "sample_rate": self.sample_rate,
            },
        }

        # 同步 requests 调用, 用 to_thread 不阻塞事件循环
        try:
            resp = await asyncio.to_thread(
                requests.post,
                DASHSCOPE_TTS_URL,
                headers=headers,
                json=body,
                timeout=30,
            )
        except Exception as e:
            logger.exception(f"DashScope TTS 请求失败: {e}")
            return False

        if resp.status_code != 200:
            logger.error(f"DashScope TTS 返回 {resp.status_code}: {resp.text[:200]}")
            return False

        try:
            data = resp.json()
            audio_b64 = data["output"]["audio"]["data"]
            audio_bytes = base64.b64decode(audio_b64)
        except (KeyError, ValueError) as e:
            logger.exception(f"DashScope 响应解析失败: {e}")
            return False

        # 按帧切分下发 (每帧 60ms = 2880 bytes @ 24kHz mono Int16)
        # 实际响应可能不是整数帧, 用 padding 补齐最后一帧
        n_frames = len(audio_bytes) // TTS_FRAME_BYTES
        remainder = len(audio_bytes) % TTS_FRAME_BYTES
        logger.info(f"🔊 [cosyvoice #{self.call_count}] 合成 {len(audio_bytes)} bytes "
                    f"= {n_frames} 帧 + {remainder} bytes 残余")

        for i in range(n_frames):
            chunk = audio_bytes[i * TTS_FRAME_BYTES:(i + 1) * TTS_FRAME_BYTES]
            await on_audio_chunk(chunk)
            await asyncio.sleep(TTS_FRAME_MS / 1000 * 0.5)  # 加速下行

        # 残余补 0 凑满一帧
        if remainder > 0:
            tail = audio_bytes[n_frames * TTS_FRAME_BYTES:]
            tail += b'\x00' * (TTS_FRAME_BYTES - remainder)
            await on_audio_chunk(tail)

        return True


# ══════════════════════════════════════════════════════════
# 监听下行帧 (mock_dsh → TTS Adapter)
# ══════════════════════════════════════════════════════════

class DSHListener:
    """监听 mock_dsh_server 下行的 assistant_text 帧"""

    def __init__(self, url: str, on_text: Callable):
        self.url = url
        self.on_text = on_text
        self.running = False

    async def run(self):
        self.running = True
        backoff = 1.0
        while self.running:
            try:
                logger.info(f"🔗 连接 DSH: {self.url}")
                async with websockets.connect(self.url) as ws:
                    await ws.send(json.dumps({
                        "type": "client/hello",
                        "device_id": "p4c5_tts_adapter",
                        "caps": ["tts", "audio"],
                    }))
                    logger.info("👋 HELLO 已发送")
                    backoff = 1.0

                    async for msg in ws:
                        if isinstance(msg, bytes):
                            continue
                        try:
                            data = json.loads(msg)
                        except json.JSONDecodeError:
                            continue

                        ftype = data.get("type", "")
                        if ftype == "assistant_text":
                            text = data.get("content", "")
                            if text:
                                logger.info(f"📝 收到文本: \"{text[:50]}\"")
                                await self.on_text(text)
                        elif ftype == "thinking":
                            logger.info(f"💭 thinking: \"{data.get('content','')[:30]}\"")
                        elif ftype == "assistant_done":
                            logger.info("✅ assistant_done")
                        elif ftype == "session_state":
                            if (data.get("state") == "idle"
                                    and not getattr(self, "_test_triggered", False)):
                                # 首次连接后, 主动触发一次 user_input 用于自测
                                self._test_triggered = True
                                logger.info("▶  session idle → 发送 user_input 触发 LLM (测试)")
                                await asyncio.sleep(0.5)
                                await ws.send(json.dumps({
                                    "type": "client/user_input",
                                    "text": "测试 TTS 链路",
                                    "session_id": data.get("session_id", ""),
                                }))

            except (ConnectionRefusedError, OSError,
                    websockets.ConnectionClosed) as e:
                logger.warning(f"DSH 连接断开: {e}, {backoff}s 后重连")
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 30.0)
            except Exception as e:
                logger.exception(f"DSH 监听异常: {e}")
                await asyncio.sleep(backoff)


# ══════════════════════════════════════════════════════════
# P4C5 TTS 服务端
# ══════════════════════════════════════════════════════════

class P4C5TTSServer:
    """TTS Adapter WS 服务端 - 给 ESP32 下行 PCM 音频"""

    def __init__(self, port: int = 8767, tts_engine: TTSEngine = None,
                 dsh_url: str = "ws://127.0.0.1:8765/ws"):
        self.port = port
        self.tts = tts_engine or MockTTSEngine()
        self.dsh_url = dsh_url
        self.clients = set()
        self.synth_lock = asyncio.Lock()

    async def handle_esp32_client(self, ws):
        """处理 ESP32 下行客户端"""
        remote = ws.remote_address
        logger.info(f"✅ ESP32 连接: {remote}")
        self.clients.add(ws)
        try:
            async for _msg in ws:
                pass
        except websockets.ConnectionClosed:
            pass
        finally:
            self.clients.discard(ws)
            logger.info(f"❌ ESP32 断开: {remote}")

    async def on_dsh_text(self, text: str):
        """DSH 下行 assistant_text → 合成 → 下行 ESP32"""
        logger.info(f"🔊 [tts] 开始合成: \"{text[:50]}\"")

        async with self.synth_lock:
            async def on_chunk(pcm: bytes):
                if not self.clients:
                    logger.warning("⚠  无 ESP32 客户端, 丢弃音频")
                    return
                for client in list(self.clients):
                    try:
                        await client.send(pcm)
                    except websockets.ConnectionClosed:
                        self.clients.discard(client)

            await self.tts.synthesize(text, on_chunk)
            logger.info(f"✅ [tts] 合成完成: \"{text[:50]}\"")

    async def run(self, host: str = "0.0.0.0"):
        logger.info(f"🚀 P4C5 TTS Adapter 启动 (阿里百炼 CosyVoice)")
        logger.info(f"   ESP32 监听: ws://{host}:{self.port}")
        logger.info(f"   TTS 引擎: {type(self.tts).__name__}")
        logger.info(f"   DSH 上游: {self.dsh_url}")

        listener = DSHListener(self.dsh_url, self.on_dsh_text)
        asyncio.create_task(listener.run())

        async with websockets.serve(self.handle_esp32_client, host, self.port):
            await asyncio.Future()


# ══════════════════════════════════════════════════════════
# CLI 入口
# ══════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="P4C5 TTS Adapter (阿里百炼 CosyVoice)")
    parser.add_argument("--port", type=int, default=8767,
                        help="ESP32 监听端口 (default: 8767)")
    parser.add_argument("--host", default="0.0.0.0",
                        help="监听地址 (default: 0.0.0.0)")
    parser.add_argument("--dsh-url", default="ws://127.0.0.1:8765/ws",
                        help="DSH 上游地址")
    parser.add_argument("--mock", action="store_true",
                        help="Mock TTS 模式 (无需 API key)")
    parser.add_argument("--voice", default=DASHSCOPE_DEFAULT_VOICE,
                        help=f"语音角色 (default: {DASHSCOPE_DEFAULT_VOICE})")
    parser.add_argument("--model", default=DASHSCOPE_DEFAULT_MODEL,
                        help=f"模型 (default: {DASHSCOPE_DEFAULT_MODEL})")
    args = parser.parse_args()

    if args.mock or not os.environ.get("DASHSCOPE_API_KEY"):
        logger.warning("⚠  使用 Mock TTS 模式 (无 key 或显式 --mock)")
        tts = MockTTSEngine()
    else:
        logger.info("✅ 使用 阿里百炼 CosyVoice 真实 TTS")
        tts = CosyVoiceTTSEngine(
            api_key=os.environ["DASHSCOPE_API_KEY"],
            model=args.model,
            voice=args.voice,
        )

    server = P4C5TTSServer(
        port=args.port,
        tts_engine=tts,
        dsh_url=args.dsh_url,
    )
    try:
        asyncio.run(server.run(args.host))
    except KeyboardInterrupt:
        logger.info("Bye")


if __name__ == "__main__":
    main()