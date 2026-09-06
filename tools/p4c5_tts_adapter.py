#!/usr/bin/env python3
"""
P4C5 TTS Adapter — Mac 端语音合成代理

接收 mock_dsh_server (或真实 DSH Adapter) 下行的 assistant_text 帧
  → 调用火山豆包双向流式 TTS (WebSocket)
  → 接收 PCM 24kHz 音频
  → 下行 WS Binary 给 ESP32-P4C5 ES8311 DAC 播放

设计参考: subagent 249bae11 火山 TTS 调研
  - URL: wss://openspeech.bytedance.com/api/v3/tts/bidirection
  - 4 字节协议头: (pv << 4) | hdr_size, (msg_type << 4) | flags, ...
  - 事件流程: StartSession(100) → TaskRequest(200) → FinishSession(102)
  - 服务端音频: TTSResponse(352) → SessionFinished(152)

POC 简化:
  - Mock TTS 模式 (无需火山 App-Key/Access-Key)
  - 生成静默 PCM + beep 提示音
  - 监听 mock_dsh_server 下行帧 (默认 ws://127.0.0.1:8765/ws)
  - 下行到 ESP32 (默认 ws://127.0.0.1:8767)

完整版 (待写):
  - 火山豆包真实接入 (需 X-Api-App-Key / X-Api-Access-Key / X-Api-Resource-Id)
  - 双向流式 (边合成边下发, 降低首音延迟)

使用方法:
  # Mock 模式
  python3 tools/p4c5_tts_adapter.py --mock

  # 真实火山 TTS (需环境变量)
  export VOLC_APP_KEY=xxx
  export VOLC_ACCESS_KEY=xxx
  export VOLC_RESOURCE_ID=volc.service_type.10029
  python3 tools/p4c5_tts_adapter.py
"""

import argparse
import asyncio
import base64
import json
import logging
import os
import struct
import sys
import time
from typing import Optional

import numpy as np
import websockets

# 第三方库
try:
    import websockets.client  # type: ignore
    HAS_WS = True
except ImportError:
    HAS_WS = False
    logging.warning("websockets 未安装")

# ══════════════════════════════════════════════════════════
# 配置
# ══════════════════════════════════════════════════════════

# 火山豆包 TTS
TTS_SAMPLE_RATE = 24000  # 24kHz (与 ESP32 ES8311 一致)
TTS_CHANNELS = 1
TTS_BITS_PER_SAMPLE = 16
TTS_FRAME_MS = 60        # 60ms / 帧 (小智配置)

# 火山豆包协议
VOLC_TTS_URL = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
PROTOCOL_VERSION = 0b0001  # v1
HEADER_SIZE = 0b0001       # 1 个 4 字节
MSG_TYPE_FULL_CLIENT = 0b0001   # full-client request
MSG_TYPE_AUDIO_ONLY = 0b0010   # audio-only server response
SERIALIZATION_JSON = 0b0001
SERIALIZATION_RAW = 0b0000
COMPRESSION_NONE = 0b0000

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
# 火山豆包协议头 (4 字节位打包)
# ══════════════════════════════════════════════════════════

def make_volc_header(msg_type: int, flags: int = 0,
                     serialization: int = SERIALIZATION_JSON,
                     compression: int = COMPRESSION_NONE) -> bytes:
    """生成火山豆包 4 字节协议头

    byte0: (protocol_version << 4) | header_size
    byte1: (message_type << 4) | flags
    byte2: (serialization << 4) | compression
    byte3: reserved (0)
    """
    b0 = (PROTOCOL_VERSION << 4) | HEADER_SIZE
    b1 = (msg_type << 4) | (flags & 0x0F)
    b2 = (serialization << 4) | (compression & 0x0F)
    return bytes([b0, b1, b2, 0x00])


def parse_volc_header(data: bytes) -> dict:
    """解析火山豆包协议头"""
    if len(data) < 4:
        return {}
    b0, b1, b2, b3 = data[0], data[1], data[2], data[3]
    return {
        'protocol_version': (b0 >> 4) & 0x0F,
        'header_size': b0 & 0x0F,
        'message_type': (b1 >> 4) & 0x0F,
        'flags': b1 & 0x0F,
        'serialization': (b2 >> 4) & 0x0F,
        'compression': b2 & 0x0F,
        'reserved': b3,
    }


# ══════════════════════════════════════════════════════════
# TTS 引擎抽象
# ══════════════════════════════════════════════════════════

class TTSEngine:
    """TTS 引擎抽象基类"""

    async def synthesize(self, text: str, on_audio_chunk) -> bool:
        """合成文字 → 音频流 (回调下发)

        on_audio_chunk: async callback(pcm_bytes) -> None
        返回: True=成功
        """
        raise NotImplementedError


class MockTTSEngine(TTSEngine):
    """Mock TTS - 生成静默 + beep 音, 不需要火山 API key"""

    def __init__(self):
        self.call_count = 0

    async def synthesize(self, text: str, on_audio_chunk) -> bool:
        """生成模拟 TTS 音频"""
        self.call_count += 1
        logger.info(f"🔊 [mock-tts #{self.call_count}] 合成: \"{text[:50]}\"")

        # 60ms @ 24kHz = 1440 samples = 2880 bytes
        FRAME_SAMPLES = TTS_SAMPLE_RATE * TTS_FRAME_MS // 1000

        # 生成 1 秒音频 = 17 帧 @ 60ms
        n_frames = 17
        for i in range(n_frames):
            # beep 音: 800Hz 正弦波, 模拟语音提示
            t = np.arange(FRAME_SAMPLES, dtype=np.float32) + i * FRAME_SAMPLES
            wave = (np.sin(2 * np.pi * 800 * t / TTS_SAMPLE_RATE) * 4000).astype(np.int16)
            pcm = wave.tobytes()
            await on_audio_chunk(pcm)
            await asyncio.sleep(TTS_FRAME_MS / 1000 * 0.5)  # 加速 mock 下行

        return True


class VolcTTSEngine(TTSEngine):
    """火山豆包双向流式 TTS

    协议流程:
      1. StartSession (100): 发送 session 配置
      2. TaskRequest (200, 可多次): 发送文本片段
      3. FinishSession (102): 结束会话
      4. 接收 TTSResponse (352) 音频数据
      5. 接收 SessionFinished (152)
    """

    def __init__(self, app_key: str, access_key: str, resource_id: str,
                 speaker: str = "BV001_streaming"):
        self.app_key = app_key
        self.access_key = access_key
        self.resource_id = resource_id
        self.speaker = speaker

    async def synthesize(self, text: str, on_audio_chunk) -> bool:
        """调用火山豆包双向流式 TTS"""
        try:
            async with websockets.connect(
                VOLC_TTS_URL,
                extra_headers={
                    "X-Api-App-Key": self.app_key,
                    "X-Api-Access-Key": self.access_key,
                    "X-Api-Resource-Id": self.resource_id,
                    "X-Api-Connect-Id": f"p4c5_{int(time.time()*1000)}",
                },
            ) as ws:
                # 1. StartSession
                session_config = {
                    "user": {"uid": "p4c5_user"},
                    "namespace": "BidirectionalTTS",
                    "req_params": {
                        "speaker": self.speaker,
                        "audio_params": {
                            "format": "pcm",
                            "sample_rate": TTS_SAMPLE_RATE,
                            "speech_rate": 0,
                            "loudness_rate": 0,
                        },
                        "additions": json.dumps({
                            "post_process": {"pitch": 0},
                            "aigc_metadata": {},
                            "cache_config": {},
                        }),
                    },
                }
                header = make_volc_header(MSG_TYPE_FULL_CLIENT, flags=0b0100)
                payload = json.dumps(session_config).encode("utf-8")
                await ws.send(header + payload)
                logger.info("📤 StartSession 已发送")

                # 2. TaskRequest (发送文本)
                task_request = {
                    "event": 200,
                    "namespace": "BidirectionalTTS",
                    "req_params": {},
                    "text": text,
                }
                header = make_volc_header(MSG_TYPE_FULL_CLIENT, flags=0b0100)
                payload = json.dumps(task_request).encode("utf-8")
                await ws.send(header + payload)
                logger.info(f"📤 TaskRequest 已发送: \"{text[:30]}\"")

                # 3. FinishSession
                finish = {"event": 102}
                header = make_volc_header(MSG_TYPE_FULL_CLIENT, flags=0b0100)
                payload = json.dumps(finish).encode("utf-8")
                await ws.send(header + payload)
                logger.info("📤 FinishSession 已发送")

                # 4. 接收响应 (TTSResponse 352 + SessionFinished 152)
                while True:
                    try:
                        msg = await asyncio.wait_for(ws.recv(), timeout=10.0)
                    except asyncio.TimeoutError:
                        logger.warning("⏱ 火山 TTS 响应超时")
                        break
                    if not msg or len(msg) < 4:
                        break
                    h = parse_volc_header(msg)
                    if h.get('message_type') == 0b1010:  # SessionFinished (10)
                        logger.info("✅ SessionFinished 收到")
                        break
                    # TTSResponse 音频数据 (msg_type=0b0011 = 3?)
                    # 火山实际定义: 352 = 0x160 (msg_type=1, flags=6?)
                    # POC: 直接把 payload 当 PCM
                    audio_payload = msg[4:]  # 跳过 4 字节头
                    if audio_payload and len(audio_payload) > 100:
                        await on_audio_chunk(audio_payload)

                return True

        except Exception as e:
            logger.exception(f"火山 TTS 调用失败: {e}")
            return False


# ══════════════════════════════════════════════════════════
# 监听下行帧 (mock_dsh_server → TTS Adapter)
# ══════════════════════════════════════════════════════════

class DSHListener:
    """监听 mock_dsh_server 下行的 assistant_text 帧"""

    def __init__(self, url: str, on_text):
        self.url = url
        self.on_text = on_text  # async callback(text: str)
        self.running = False

    async def run(self):
        """持续监听 mock_dsh_server + 主动触发 user_input 测试"""
        self.running = True
        backoff = 1.0
        while self.running:
            try:
                logger.info(f"🔗 连接 DSH: {self.url}")
                async with websockets.connect(self.url) as ws:
                    # HELLO (作为 ESP32 客户端)
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
                            if data.get("state") == "idle":
                                # ★ 连接建立后, 主动发送 user_input 触发 mock LLM 回复
                                # (生产环境由 P4C5 端发送, 这里仅用于测试)
                                logger.info("▶  session idle → 发送 user_input 触发 LLM")
                                await asyncio.sleep(0.5)
                                await ws.send(json.dumps({
                                    "type": "client/user_input",
                                    "text": "测试 TTS 链路",
                                    "session_id": data.get("session_id", ""),
                                }))

            except (ConnectionRefusedError, OSError, websockets.ConnectionClosed) as e:
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
        self.clients = set()  # 已连接的 ESP32 客户端
        self.pending_texts = []  # 待合成的文本队列
        self.synth_lock = asyncio.Lock()

    async def handle_esp32_client(self, ws):
        """处理 ESP32 下行客户端"""
        remote = ws.remote_address
        logger.info(f"✅ ESP32 连接: {remote}")
        self.clients.add(ws)
        try:
            async for msg in ws:
                # 简单保持连接 (可加心跳/控制帧)
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
                """TTS 引擎回调: 下行 PCM 给所有 ESP32 客户端"""
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
        """启动 WS 服务 + DSH 监听"""
        logger.info(f"🚀 P4C5 TTS Adapter 启动")
        logger.info(f"   ESP32 监听: ws://{host}:{self.port}")
        logger.info(f"   TTS 引擎: {type(self.tts).__name__}")
        logger.info(f"   DSH 上游: {self.dsh_url}")

        # 启动 DSH 监听任务
        listener = DSHListener(self.dsh_url, self.on_dsh_text)
        asyncio.create_task(listener.run())

        # 启动 ESP32 服务端
        async with websockets.serve(self.handle_esp32_client, host, self.port):
            await asyncio.Future()  # run forever


# ══════════════════════════════════════════════════════════
# CLI 入口
# ══════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="P4C5 TTS Adapter")
    parser.add_argument("--port", type=int, default=8767,
                        help="ESP32 监听端口 (default: 8767)")
    parser.add_argument("--host", default="0.0.0.0",
                        help="监听地址 (default: 0.0.0.0)")
    parser.add_argument("--dsh-url", default="ws://127.0.0.1:8765/ws",
                        help="DSH 上游地址")
    parser.add_argument("--mock", action="store_true",
                        help="Mock TTS 模式 (无需火山 API key)")
    args = parser.parse_args()

    # 选择 TTS 引擎
    if args.mock or not (os.environ.get("VOLC_APP_KEY")
                         and os.environ.get("VOLC_ACCESS_KEY")):
        tts = MockTTSEngine()
    else:
        tts = VolcTTSEngine(
            app_key=os.environ["VOLC_APP_KEY"],
            access_key=os.environ["VOLC_ACCESS_KEY"],
            resource_id=os.environ.get("VOLC_RESOURCE_ID",
                                       "volc.service_type.10029"),
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