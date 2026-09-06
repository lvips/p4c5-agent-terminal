#!/usr/bin/env python3
"""
P4C5 ASR Adapter — Mac 端语音识别代理

接收 ESP32-P4C5 WS Binary 上行的 OPUS 音频流
  → 解码 (opuslib) 到 PCM Int16
  → VAD (RMS 能量 + 静音超时)
  → 调用阿里云 DashScope qwen3-asr-flash-realtime 流式 ASR
  → 识别文字通过 WS JSON 帧 client/user_input 上行到 mock_dsh_server (或其他 DSH Adapter)

设计参考: OMT tab5-adapter/asr.js (Node.js, ~250 行)
P4C5 适配:
  - 用 Python 替代 Node.js (DSH 偏好)
  - 阿里云 DashScope (替代阿里云 NLS ISI, 阿里云官方推荐更新)
  - 完整的 mock mode (无阿里云 key 时也能跑)

使用方法:
  # Mock 模式 (不需要阿里云 key)
  python3 tools/p4c5_asr_adapter.py --mock

  # 真实 ASR 模式
  export DASHSCOPE_API_KEY=sk-xxx
  python3 tools/p4c5_asr_adapter.py --port 8766

架构:
  P4C5 ──WS Binary (opus)──> 本 ASR Adapter ──HTTP/WS──> 阿里云 DashScope
       <──WS JSON (text)─────                            ↓ 识别文字
                                                       ↓
       <──WS JSON (text)───── mock_dsh_server (8765) <──┘
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
from collections import deque
from typing import Optional, Callable

import numpy as np
import websockets

# 第三方库
try:
    import opuslib  # type: ignore
    HAS_OPUS = True
except ImportError:
    HAS_OPUS = False
    logging.warning("opuslib 未安装, 将使用 mock decoder")

try:
    import dashscope  # type: ignore
    from dashscope.audio.asr import Recognition  # type: ignore
    HAS_DASHSCOPE = True
except ImportError:
    HAS_DASHSCOPE = False
    logging.warning("dashscope 未安装, 将使用 mock ASR")

# ══════════════════════════════════════════════════════════
# 配置
# ══════════════════════════════════════════════════════════

SAMPLE_RATE = 16000
CHANNELS = 1
FRAME_SIZE_20MS = 320  # 16kHz × 0.020s

# VAD 参数 (与 OMT asr.js 一致)
VAD_RMS_START = 80     # ~ -57 dBFS on 16-bit, 触发"开始说话"
VAD_RMS_END = 50       # 触发"说话结束"
VAD_END_HOLD_MS = 900  # 持续 900ms 静音才认为结束
VAD_MAX_DURATION_MS = 20000  # 20s 硬截止
VAD_MIN_DURATION_MS = 300    # 短于此忽略
NO_FRAME_TIMEOUT_MS = 1000   # 1s 无新帧强制结束
FRAME_MS = 20                # 每帧 20ms (16kHz × 320 samples)
VAD_END_HOLD_FRAMES = VAD_END_HOLD_MS // FRAME_MS  # 45 帧 = 900ms
VAD_MAX_DURATION_FRAMES = VAD_MAX_DURATION_MS // FRAME_MS  # 1000 帧 = 20s
VAD_MIN_DURATION_FRAMES = VAD_MIN_DURATION_MS // FRAME_MS  # 15 帧 = 300ms

# OPUS 帧格式 (与 ESP32 opus_encoder 配置一致)
OPUS_BITRATE = 16000
OPUS_COMPLEXITY = 5
OPUS_FRAME_SIZE = FRAME_SIZE_20MS

# 日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger("p4c5-asr")
logger.setLevel(logging.INFO)
logger.propagate = True


# ══════════════════════════════════════════════════════════
# 工具函数
# ══════════════════════════════════════════════════════════

def rms_int16(pcm: np.ndarray) -> float:
    """计算 Int16 PCM 的 RMS (与 OMT asr.js rmsOfInt16 一致)"""
    if len(pcm) == 0:
        return 0.0
    return float(np.sqrt(np.mean(pcm.astype(np.float64) ** 2)))


def rms_int16_buf(pcm: bytes) -> float:
    """bytes → RMS"""
    arr = np.frombuffer(pcm, dtype=np.int16)
    return rms_int16(arr)


def pcm_to_wav(pcm_data: bytes, sample_rate: int = SAMPLE_RATE) -> bytes:
    """PCM Int16 → WAV (用于调试/记录)"""
    import io
    import wave
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm_data)
    return buf.getvalue()


# ══════════════════════════════════════════════════════════
# OPUS 解码器
# ══════════════════════════════════════════════════════════

class OpusDecoder:
    """opuslib 解码 OPUS → PCM Int16 (20ms 帧)"""

    def __init__(self):
        if not HAS_OPUS:
            raise RuntimeError("opuslib 未安装, 请运行: pip install opuslib")
        self.decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)

    def decode(self, opus_frame: bytes) -> bytes:
        """解码单帧 OPUS → 640 bytes PCM (320 samples × 2 bytes)"""
        return self.decoder.decode(opus_frame, OPUS_FRAME_SIZE)


class MockOpusDecoder:
    """Mock 解码器 (无 opuslib 时) - 生成伪语音 PCM

    根据 opus_frame 第一字节决定输出:
      - 0x00 = 静默 (RMS=0)
      - 其他 = 1kHz 语音 (RMS~12000)

    这样测试可以模拟 "静默 → 语音 → 静默" 完整会话
    """

    def __init__(self):
        self.frame_idx = 0

    def decode(self, opus_frame: bytes) -> bytes:
        self.frame_idx += 1
        if opus_frame and opus_frame[0] == 0x00:
            # 静默
            return b'\x00\x00' * OPUS_FRAME_SIZE
        else:
            # 1kHz 正弦波
            t = np.arange(OPUS_FRAME_SIZE, dtype=np.float32) + self.frame_idx * OPUS_FRAME_SIZE
            wave = (np.sin(2 * np.pi * 1000 * t / SAMPLE_RATE) * 12000).astype(np.int16)
            return wave.tobytes()


# ══════════════════════════════════════════════════════════
# ASR 引擎 (抽象基类)
# ══════════════════════════════════════════════════════════

class ASREngine:
    """ASR 引擎抽象基类"""

    async def recognize(self, pcm_data: bytes) -> str:
        """识别一段 PCM → 文字"""
        raise NotImplementedError

    async def close(self):
        pass


class MockASREngine(ASREngine):
    """Mock ASR (无 DashScope 时) - 返回带统计信息的伪识别文字"""

    def __init__(self):
        self.call_count = 0

    async def recognize(self, pcm_data: bytes) -> str:
        self.call_count += 1
        samples = len(pcm_data) // 2
        duration_ms = samples * 1000 // SAMPLE_RATE
        rms = rms_int16_buf(pcm_data)
        # 伪识别: 根据 RMS 判断"是否有声", 决定返回有意义文字
        if rms > VAD_RMS_START:
            return (
                f"[Mock-ASR #{self.call_count}] 识别到语音 "
                f"({duration_ms}ms, RMS={rms:.0f})"
            )
        else:
            return ""


class DashScopeASREngine(ASREngine):
    """阿里云 DashScope qwen3-asr-flash-realtime 一次性识别"""

    def __init__(self, api_key: str):
        if not HAS_DASHSCOPE:
            raise RuntimeError("dashscope 未安装")
        dashscope.api_key = api_key
        self.api_key = api_key

    async def recognize(self, pcm_data: bytes) -> str:
        """调用 DashScope 同步 ASR API (POC: 一次性 REST 而非流式 WS)"""
        try:
            import dashscope
            from dashscope.audio.asr import Recognition

            # POC: 把 PCM 包装成临时 WAV 文件传给 DashScope
            # 完整版: 用流式 WS API (`dashscope.audio.asr.Recognition`)
            wav_data = pcm_to_wav(pcm_data)

            # 写到临时文件
            import tempfile
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                f.write(wav_data)
                tmp_path = f.name

            try:
                response = Recognition.call(
                    model="paraformer-realtime-v2",
                    file_path=tmp_path,
                    sample_rate=SAMPLE_RATE,
                    format="wav",
                    language="zh",
                )
                if response.status_code == 200:
                    return response.output.get("text", "")
                else:
                    logger.error(f"DashScope error: {response.message}")
                    return ""
            finally:
                os.unlink(tmp_path)
        except Exception as e:
            logger.exception(f"DashScope ASR failed: {e}")
            return ""


# ══════════════════════════════════════════════════════════
# VAD + 会话累积
# ══════════════════════════════════════════════════════════

class VADSession:
    """会话状态机: 累积 PCM → VAD 检测 → 触发 ASR"""

    def __init__(self, asr: ASREngine, on_text: Callable):
        self.asr = asr
        self.on_text = on_text  # (text, meta) -> coroutine

        self.pcm_chunks = []   # list[np.ndarray]
        self.samples = 0
        self.frames = 0        # 会话内帧计数 (用于 VAD)
        self.speech_started = False
        self.silence_started_at_frame: Optional[int] = None  # 静音开始的帧号
        self.session_start_at: Optional[float] = None
        self.recognizing = False

    def reset(self):
        """重置会话状态 (收到 audio start 帧时调用)"""
        self.pcm_chunks = []
        self.samples = 0
        self.frames = 0
        self.speech_started = False
        self.silence_started_at_frame = None
        self.session_start_at = None
        self.recognizing = False

    async def feed_opus_frame(self, opus_buf: bytes, decoder):
        """收到一帧 OPUS → 解码 → 累积 → VAD 检测"""
        if self.recognizing:
            return  # 上一段还在识别, 丢弃新帧

        try:
            pcm_bytes = decoder.decode(opus_buf)
        except Exception as e:
            logger.error(f"OPUS decode failed: {e}")
            return

        pcm = np.frombuffer(pcm_bytes, dtype=np.int16)
        rms = rms_int16(pcm)
        now = time.time()

        # 计数
        if not hasattr(self, '_frame_idx'):
            self._frame_idx = 0
        self._frame_idx += 1

        # 静音期: 等待 speech start
        if not self.speech_started:
            if rms > VAD_RMS_START:
                logger.info(f"🎙  speech start (RMS={rms:.0f}, frame={self._frame_idx})")
                self.speech_started = True
                self.silence_started_at_frame = None
                self.session_start_at = now
                self.frames = 1
                self.pcm_chunks.append(pcm)
                self.samples += len(pcm)
            else:
                if self._frame_idx % 20 == 0:
                    logger.info(f"   ⏳ 等待语音... (frame={self._frame_idx}, RMS={rms:.0f})")
            return

        # 说话中: 累积
        self.pcm_chunks.append(pcm)
        self.samples += len(pcm)
        self.frames += 1

        # 检测结束: 静音持续 >= VAD_END_HOLD_FRAMES 帧
        if rms < VAD_RMS_END:
            if self.silence_started_at_frame is None:
                self.silence_started_at_frame = self.frames
                logger.info(f"   🤫 静音开始 (frame={self._frame_idx}, RMS={rms:.0f})")
            else:
                silence_frames = self.frames - self.silence_started_at_frame
                if silence_frames >= VAD_END_HOLD_FRAMES:
                    logger.info(f"🤫 speech end (silence {silence_frames} frames = "
                                f"{silence_frames*FRAME_MS}ms)")
                    await self._finalize()
                    return
        else:
            self.silence_started_at_frame = None

        # 硬截止: 超过 VAD_MAX_DURATION_FRAMES 强制结束
        if self.frames >= VAD_MAX_DURATION_FRAMES:
            logger.info(f"⏱  hard cutoff at {self.frames} frames")
            await self._finalize()
            return

    async def _finalize(self):
        """触发识别"""
        if self.recognizing or not self.pcm_chunks:
            self.reset()
            return

        self.recognizing = True

        # 拼接 PCM
        pcm_full = np.concatenate(self.pcm_chunks).tobytes()
        duration_ms = len(pcm_full) / 2 / SAMPLE_RATE * 1000
        frames_total = self.frames
        self.reset()

        # 太短忽略
        if frames_total < VAD_MIN_DURATION_FRAMES:
            logger.info(f"⏭  skip too short ({frames_total} frames < {VAD_MIN_DURATION_FRAMES})")
            self.recognizing = False
            return

        # 调 ASR
        text = await self.asr.recognize(pcm_full)
        self.recognizing = False

        if text:
            logger.info(f"📝 ASR 识别: \"{text}\" ({duration_ms:.0f}ms, {frames_total} frames)")
            try:
                await self.on_text(text, {"duration_ms": int(duration_ms), "frames": frames_total})
            except Exception as e:
                logger.error(f"on_text callback failed: {e}")

    async def force_end(self):
        """外部触发结束 (audio 控制帧 action=end)"""
        await self._finalize()


# ══════════════════════════════════════════════════════════
# WS 服务端
# ══════════════════════════════════════════════════════════

class P4C5ASRServer:
    """ASR Adapter WebSocket 服务端"""

    def __init__(self, port: int = 8766, asr_engine: ASREngine = None,
                 upstream_url: Optional[str] = None):
        self.port = port
        self.asr = asr_engine or MockASREngine()
        self.upstream_url = upstream_url  # 上行到 mock_dsh_server 的地址
        self.decoder = OpusDecoder() if HAS_OPUS else MockOpusDecoder()
        self.sessions = {}  # ws -> (VADSession, upstream_ws)

    async def handle_client(self, ws):
        """处理单个 ESP32 客户端连接"""
        remote = ws.remote_address
        logger.info(f"✅ 新连接: {remote}")

        # 上行到 mock_dsh_server (如果配置了)
        upstream_ws = None
        if self.upstream_url:
            try:
                upstream_ws = await websockets.connect(self.upstream_url)
                logger.info(f"  ↗  上行到 mock_dsh: {self.upstream_url}")
            except Exception as e:
                logger.error(f"上行连接失败: {e}")

        session = VADSession(self.asr, lambda t, m: self.on_text(upstream_ws, t, m))
        self.sessions[ws] = (session, upstream_ws)

        try:
            async for msg in ws:
                # 区分文本/二进制
                if isinstance(msg, bytes):
                    # OPUS 帧
                    if not hasattr(self, '_binary_count'):
                        self._binary_count = 0
                    self._binary_count += 1
                    if self._binary_count <= 5 or self._binary_count % 50 == 0:
                        logger.info(f"📦 binary msg #{self._binary_count} len={len(msg)}")
                    await session.feed_opus_frame(msg, self.decoder)
                else:
                    # JSON 控制帧 (audio start/end 等)
                    try:
                        data = json.loads(msg)
                        await self._handle_control(upstream_ws, session, data)
                    except json.JSONDecodeError:
                        logger.warning(f"❌ JSON 解析失败: {msg[:100]}")

        except websockets.ConnectionClosed:
            pass
        finally:
            try:
                if upstream_ws:
                    await upstream_ws.close()
            except Exception:
                pass
            self.sessions.pop(ws, None)
            logger.info(f"❌ 断开: {remote}")

    async def _handle_control(self, upstream_ws, session: VADSession, data: dict):
        """处理 JSON 控制帧"""
        ftype = data.get("type", "")
        if ftype == "audio":
            action = data.get("action", "")
            if action == "start":
                logger.info("▶  audio start")
                session.reset()
            elif action == "end":
                logger.info("⏹  audio end")
                await session.force_end()
        elif ftype == "client/hello":
            logger.info(f"👋 hello: {data.get('device_id', '?')}")

    async def on_text(self, upstream_ws, text: str, meta: dict):
        """ASR 识别文字 → 上行到 mock_dsh_server"""
        if upstream_ws is None:
            logger.info(f"💬 [no upstream] text: \"{text}\"")
            return

        try:
            # 包装成 DSH 协议帧
            frame = {
                "type": "client/user_input",
                "text": text,
                "source": "p4c5_asr",
                "duration_ms": meta.get("duration_ms", 0),
                "session_id": "asr_session",
            }
            await upstream_ws.send(json.dumps(frame))
            logger.info(f"💬 上行 user_input: \"{text[:50]}\"")
        except Exception as e:
            logger.error(f"上行发送失败: {e}")

    async def run(self, host: str = "0.0.0.0"):
        """启动 WS 服务"""
        logger.info(f"🚀 P4C5 ASR Adapter 启动")
        logger.info(f"   监听: ws://{host}:{self.port}")
        logger.info(f"   ASR 引擎: {type(self.asr).__name__}")
        logger.info(f"   OPUS 解码: {'opuslib' if HAS_OPUS else 'mock'}")
        logger.info(f"   上行 upstream: {self.upstream_url or 'NONE'}")

        async with websockets.serve(self.handle_client, host, self.port):
            await asyncio.Future()  # run forever


# ══════════════════════════════════════════════════════════
# CLI 入口
# ══════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="P4C5 ASR Adapter")
    parser.add_argument("--port", type=int, default=8766,
                        help="监听端口 (default: 8766)")
    parser.add_argument("--host", default="0.0.0.0",
                        help="监听地址 (default: 0.0.0.0)")
    parser.add_argument("--mock", action="store_true",
                        help="Mock ASR 模式 (无 DashScope 也能跑)")
    parser.add_argument("--upstream", default=None,
                        help="上行到 mock_dsh_server 地址 (例: ws://127.0.0.1:8765/ws)")
    args = parser.parse_args()

    # 选择 ASR 引擎
    if args.mock or not os.environ.get("DASHSCOPE_API_KEY"):
        asr = MockASREngine()
    else:
        asr = DashScopeASREngine(os.environ["DASHSCOPE_API_KEY"])

    server = P4C5ASRServer(port=args.port, asr_engine=asr, upstream_url=args.upstream)
    try:
        asyncio.run(server.run(args.host))
    except KeyboardInterrupt:
        logger.info("Bye")


if __name__ == "__main__":
    main()