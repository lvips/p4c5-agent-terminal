#!/usr/bin/env python3
"""
P4C5 ASR Adapter — Mac 端语音识别代理 (OMT 同等配置)

完全复刻 OMT tab5-adapter/asr.js 的设计:
  - 阿里云 NLS 一句话识别 (ISI, Intelligent Speech Interaction)
  - REST POST: https://nls-gateway.cn-shanghai.aliyuncs.com/stream/v1/asr
  - POP API signature v1 (HMAC-SHA1) 获取 token
  - opusscript/opusscript-decoder 解码 OPUS → PCM Int16
  - VAD: RMS 80/50, 静音 900ms 结束, 硬截止 20s

P4C5 适配:
  - Python 替代 Node.js (DSH 偏好)
  - 完整复刻 OMT asr.js 的 VAD 逻辑
  - 支持 mock 模式 (无阿里云 key 也能跑)

使用方法:
  # Mock 模式 (不需要阿里云 key)
  python3 tools/p4c5_asr_adapter.py --mock --port 8766

  # 真实 ASR 模式 (需要阿里云 NLS 凭证)
  export ALIYUN_ACCESS_KEY_ID=xxx
  export ALIYUN_ACCESS_KEY_SECRET=xxx
  export ALIYUN_NLS_APPKEY=xxx
  python3 tools/p4c5_asr_adapter.py --port 8766

架构:
  P4C5 ──WS Binary (opus)──> 本 ASR Adapter ──REST──> 阿里云 NLS ISI
        <──WS JSON (text)─────                         ↓ 识别文字
                                                       ↓
        <──WS JSON (text)───── mock_dsh_server (8765) <┘
"""

import argparse
import asyncio
import base64
import hashlib
import hmac
import json
import logging
import os
import struct
import sys
import time
import urllib.parse
import uuid
from typing import Optional, Callable

import numpy as np
import websockets

# 第三方库 (与 OMT 相同, 都需要 OPUS 解码)
try:
    import opuslib  # type: ignore
    HAS_OPUS = True
except ImportError:
    HAS_OPUS = False
    logging.warning("opuslib 未安装, 将使用 mock decoder")

# HTTP 客户端 (REST 调用阿里云)
try:
    import requests  # type: ignore
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False
    logging.warning("requests 未安装, 真实 ASR 模式需要: pip3 install requests")

# ══════════════════════════════════════════════════════════
# 常量 (与 OMT asr.js 完全一致)
# ══════════════════════════════════════════════════════════

SAMPLE_RATE = 16000
CHANNELS = 1
FRAME_SIZE_20MS = 320  # 16000 * 0.020 = 320 samples per 20ms frame

# VAD 参数 (OMT 文中已注释: 16-bit PCM, RMS 阈值与静音超时)
VAD_RMS_START = 80          # ~ -57 dBFS on 16-bit
VAD_RMS_END = 50
VAD_END_HOLD_MS = 900       # 900ms 静音后判定为说话结束
VAD_MAX_DURATION_MS = 20000 # 20s 硬截止 (ISI one-shot limit ~60s)
VAD_MIN_DURATION_MS = 300   # 短于此忽略

# 阿里云 NLS 配置
NLS_ASR_URL = "https://nls-gateway.cn-shanghai.aliyuncs.com/stream/v1/asr"
NLS_TOKEN_URL = "https://nls-meta.cn-shanghai.aliyuncs.com/"
NLS_API_VERSION = "2019-02-28"
NLS_REGION = "cn-shanghai"

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
# OMT 复刻: 阿里云 NLS Token (POP API signature v1)
# ══════════════════════════════════════════════════════════

def special_url_encode(s: str) -> str:
    """阿里云 POP 签名特殊 URL 编码

    与 RFC3986 相同, 除了:
      space → %20 (不是 +)
      *     → %2A
      %7E   → ~    (反转义波浪号)
    """
    return (urllib.parse.quote(s, safe='')
            .replace('+', '%20')
            .replace('*', '%2A')
            .replace('%7E', '~'))


def build_canonicalized_query(params: dict) -> str:
    """构建规范化查询字符串 (按 key 字典序排序)"""
    keys = sorted(params.keys())
    pairs = [f"{special_url_encode(k)}={special_url_encode(str(v))}" for k, v in keys]
    return "&".join(pairs)


def compute_pop_signature(access_key_secret: str, params: dict) -> str:
    """计算 POP API v1 签名 (HMAC-SHA1)"""
    canonical = build_canonicalized_query(params)
    string_to_sign = f"POST&{special_url_encode('/')}&{special_url_encode(canonical)}"
    hmac_obj = hmac.new(
        f"{access_key_secret}&".encode('utf-8'),
        string_to_sign.encode('utf-8'),
        hashlib.sha1
    )
    return base64.b64encode(hmac_obj.digest()).decode('utf-8')


def get_nls_token(access_key_id: str, access_key_secret: str) -> dict:
    """获取阿里云 NLS 访问 token (含过期时间)

    返回: {"id": "token-string", "expire_at": unix-seconds}
    """
    public_params = {
        "Format": "JSON",
        "Version": NLS_API_VERSION,
        "AccessKeyId": access_key_id,
        "SignatureMethod": "HMAC-SHA1",
        "Timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "SignatureVersion": "1.0",
        "SignatureNonce": str(uuid.uuid4()),
        "Action": "CreateToken",
        "RegionId": NLS_REGION,
    }
    public_params["Signature"] = compute_pop_signature(access_key_secret, public_params)

    resp = requests.post(NLS_TOKEN_URL, params=public_params, timeout=10)
    data = resp.json()
    if "Token" not in data:
        raise Exception(f"GetToken failed: {data}")
    return {
        "id": data["Token"]["Id"],
        "expire_at": int(data["Token"]["ExpireTime"]),
    }


# ══════════════════════════════════════════════════════════
# OMT 复刻: 阿里云 NLS 一句话识别 (ISI)
# ══════════════════════════════════════════════════════════

def call_isi_one_shot(pcm_bytes: bytes, token: str, appkey: str) -> str:
    """调用阿里云 NLS ISI 一次性识别

    Request:
      POST https://nls-gateway.cn-shanghai.aliyuncs.com/stream/v1/asr
        ?appkey=...&format=pcm&sample_rate=16000
        &enable_punctuation_prediction=true
        &enable_inverse_text_normalization=true
      Headers:
        X-NLS-Token: <token>
        Content-Type: application/octet-stream
      Body: raw PCM bytes (Int16 little-endian, 16kHz, mono)

    Response (JSON):
      { result: "识别文字", status: 20000000, message: "OK" }
    """
    params = {
        "appkey": appkey,
        "format": "pcm",
        "sample_rate": str(SAMPLE_RATE),
        "enable_punctuation_prediction": "true",
        "enable_inverse_text_normalization": "true",
    }
    headers = {
        "X-NLS-Token": token,
        "Content-Type": "application/octet-stream",
    }
    resp = requests.post(
        NLS_ASR_URL,
        params=params,
        headers=headers,
        data=pcm_bytes,
        timeout=15,
    )
    data = resp.json()
    if data.get("status") != 20000000:
        raise Exception(f"ISI error: status={data.get('status')}, "
                        f"message={data.get('message')}")
    return data.get("result", "")


# ══════════════════════════════════════════════════════════
# Token 缓存 (避免每次都调用 GetToken)
# ══════════════════════════════════════════════════════════

class TokenCache:
    """阿里云 NLS Token 缓存 (提前 5 分钟过期, 与 OMT 一致)"""

    def __init__(self, access_key_id: str, access_key_secret: str):
        self.ak_id = access_key_id
        self.ak_secret = access_key_secret
        self.token = None
        self.expire_at = 0

    async def get(self) -> str:
        now = int(time.time())
        if self.token and now < self.expire_at - 300:
            return self.token
        logger.info("🔑 刷新阿里云 NLS token...")
        # requests 是同步, 但调用很快, 用 asyncio.to_thread 不阻塞事件循环
        result = await asyncio.to_thread(
            get_nls_token, self.ak_id, self.ak_secret
        )
        self.token = result["id"]
        self.expire_at = result["expire_at"]
        logger.info(f"✅ token 有效期到 {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(self.expire_at))}")
        return self.token


# ══════════════════════════════════════════════════════════
# OPUS 解码 (与 OMT opusscript 等价)
# ══════════════════════════════════════════════════════════

class OpusDecoder:
    """OPUS 解码器 (libopus via opuslib)"""

    def __init__(self):
        if not HAS_OPUS:
            raise RuntimeError("opuslib 未安装, 请运行: pip3 install opuslib")
        self.decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)

    def decode(self, opus_frame: bytes) -> np.ndarray:
        """解码一个 OPUS 帧 (20ms @ 16kHz) → PCM Int16"""
        pcm_bytes = self.decoder.decode(opus_frame, FRAME_SIZE_20MS)
        return np.frombuffer(pcm_bytes, dtype=np.int16)


class MockOpusDecoder:
    """Mock 解码器 (无 opuslib 时)

    根据 opus_frame 第一字节决定输出:
      - 0x00 = 静默 (RMS=0)
      - 其他 = 1kHz 语音 (RMS~12000)
    """

    def __init__(self):
        self.frame_idx = 0

    def decode(self, opus_frame: bytes) -> np.ndarray:
        self.frame_idx += 1
        if opus_frame and opus_frame[0] == 0x00:
            return np.zeros(FRAME_SIZE_20MS, dtype=np.int16)
        else:
            t = np.arange(FRAME_SIZE_20MS, dtype=np.float32) + self.frame_idx * FRAME_SIZE_20MS
            wave = (np.sin(2 * np.pi * 1000 * t / SAMPLE_RATE) * 12000).astype(np.int16)
            return wave


# ══════════════════════════════════════════════════════════
# ASR 引擎抽象 (OMT 直接调 ISI, 我们做一层抽象)
# ══════════════════════════════════════════════════════════

class ASREngine:
    """ASR 引擎基类"""
    async def recognize(self, pcm_int16: np.ndarray) -> str:
        raise NotImplementedError


class MockASREngine(ASREngine):
    """Mock ASR (无 key 时)"""

    def __init__(self):
        self.call_count = 0

    async def recognize(self, pcm_int16: np.ndarray) -> str:
        self.call_count += 1
        duration_ms = len(pcm_int16) * 1000 / SAMPLE_RATE
        rms = float(np.sqrt(np.mean(pcm_int16.astype(np.float32) ** 2)))
        # 与 OMT test-asr-direct.js 输出格式一致
        return f"[Mock-ASR #{self.call_count}] 识别到语音 ({int(duration_ms)}ms, RMS={int(rms)})"


class NLSASREngine(ASREngine):
    """阿里云 NLS 一句话识别 (ISI) - 与 OMT 完全一致"""

    def __init__(self, access_key_id: str, access_key_secret: str, appkey: str):
        self.token_cache = TokenCache(access_key_id, access_key_secret)
        self.appkey = appkey
        self.call_count = 0

    async def recognize(self, pcm_int16: np.ndarray) -> str:
        self.call_count += 1
        token = await self.token_cache.get()
        # Int16 little-endian (原生字节序)
        pcm_bytes = pcm_int16.tobytes()
        # OMT 也是直接 requests.post 同步调用, 用 to_thread 不阻塞事件循环
        result = await asyncio.to_thread(
            call_isi_one_shot, pcm_bytes, token, self.appkey
        )
        return result


# ══════════════════════════════════════════════════════════
# VAD 会话 (复刻 OMT asr.js 完整逻辑)
# ══════════════════════════════════════════════════════════

class VADSession:
    """会话状态机: 累积 PCM → VAD 检测 → 触发 ASR

    完全复刻 OMT tab5-adapter/asr.js 的逻辑:
      - rmsOfInt16() 整数平方根
      - VAD_RMS_START / VAD_RMS_END 阈值
      - VAD_END_HOLD_MS 静音超时
      - VAD_MAX_DURATION_MS 硬截止
    """

    def __init__(self, asr: ASREngine, on_text: Callable):
        self.asr = asr
        self.on_text = on_text

        self.pcm_chunks = []
        self.samples = 0
        self.frames = 0
        self.speech_started = False
        self.silence_started_at: Optional[float] = None  # OMT 风格: 时间戳
        self.session_start_at: Optional[float] = None
        self.recognizing = False

    def reset(self):
        self.pcm_chunks = []
        self.samples = 0
        self.frames = 0
        self.speech_started = False
        self.silence_started_at = None
        self.session_start_at = None

    @staticmethod
    def rms_of_int16(buf: np.ndarray) -> float:
        """OMT 原文:
            function rmsOfInt16(buf) {
              let sumSq = 0;
              for (let i = 0; i < buf.length; i++) {
                const v = buf[i];
                sumSq += v * v;
              }
              return Math.sqrt(sumSq / buf.length);
            }
        """
        sum_sq = float(np.sum(buf.astype(np.float64) ** 2))
        return (sum_sq / len(buf)) ** 0.5

    async def feed(self, pcm_int16: np.ndarray, now: float):
        """处理一个 20ms PCM 帧"""
        if self.recognizing:
            return  # OMT: 识别期间丢弃帧

        rms = self.rms_of_int16(pcm_int16)
        if self.session_start_at is None:
            self.session_start_at = now

        # 静音期: 等待 speech start
        if not self.speech_started:
            if rms > VAD_RMS_START:
                logger.info(f"🎙  speech start (RMS={rms:.0f}, frame={self.frames})")
                self.speech_started = True
                self.silence_started_at = None
                self.frames = 1
                self.pcm_chunks.append(pcm_int16)
                self.samples += len(pcm_int16)
            else:
                if self.frames % 20 == 0:
                    logger.info(f"   ⏳ 等待语音... (frame={self.frames}, RMS={rms:.0f})")
            return

        # 说话中: 累积
        self.pcm_chunks.append(pcm_int16)
        self.samples += len(pcm_int16)
        self.frames += 1
        duration_ms = self.samples * 1000 / SAMPLE_RATE

        # 检测结束: 静音持续 >= VAD_END_HOLD_MS (完全复刻 OMT)
        if rms < VAD_RMS_END:
            if self.silence_started_at is None:
                self.silence_started_at = now
                logger.info(f"   🤫 静音开始 (frame={self.frames}, RMS={rms:.0f})")
            else:
                silence_ms = (now - self.silence_started_at) * 1000
                if silence_ms >= VAD_END_HOLD_MS:
                    logger.info(f"🤫 speech end (silence {silence_ms:.0f}ms >= {VAD_END_HOLD_MS}ms)")
                    await self._finalize()
                    return
        else:
            self.silence_started_at = None

        # 硬截止: 超过 VAD_MAX_DURATION_MS
        if duration_ms >= VAD_MAX_DURATION_MS:
            logger.info(f"⏱  hard cutoff at {duration_ms:.0f}ms")
            await self._finalize()
            return

    async def _finalize(self):
        """触发识别 (复刻 OMT recognizeAndEmit)"""
        if self.recognizing or not self.pcm_chunks:
            self.reset()
            return

        self.recognizing = True

        # 拼接 PCM
        pcm_full = np.concatenate(self.pcm_chunks)
        duration_ms = len(pcm_full) / SAMPLE_RATE * 1000
        frames_total = self.frames
        self.reset()

        # 太短忽略 (OMT: pcm.length < (SAMPLE_RATE * VAD_MIN_DURATION_MS) / 1000)
        if duration_ms < VAD_MIN_DURATION_MS:
            logger.info(f"⏭  skip too short ({duration_ms:.0f}ms)")
            self.recognizing = False
            return

        # 调 ASR
        logger.info(f"🔄 ASR 识别中: {len(pcm_full)} samples ({duration_ms:.0f}ms)...")
        text = await self.asr.recognize(pcm_full)
        self.recognizing = False

        if text:
            logger.info(f"📝 ASR 识别: \"{text}\" ({duration_ms:.0f}ms, {frames_total} frames)")
            try:
                await self.on_text(text, {"duration_ms": int(duration_ms),
                                          "frames": frames_total})
            except Exception as e:
                logger.error(f"on_text callback failed: {e}")


# ══════════════════════════════════════════════════════════
# P4C5 ASR Adapter 服务端 (与 OMT 不同, 我们用 WS 而非 OPC UA)
# ══════════════════════════════════════════════════════════

class P4C5ASRServer:
    """P4C5 ASR Adapter 服务端

    接收 ESP32 WS Binary (opus) → VAD → ASR → 上行 user_input 到 DSH
    复刻 OMT tab5-adapter/adapter.js 整体架构
    """

    def __init__(self, port: int = 8766, asr_engine: ASREngine = None,
                 upstream_url: str = "ws://127.0.0.1:8765/ws"):
        self.port = port
        self.asr = asr_engine or MockASREngine()
        self.upstream_url = upstream_url
        self.upstream_ws: Optional[websockets.WebSocketClientProtocol] = None

    async def connect_upstream(self):
        """连接到上游 DSH (mock_dsh_server.py 或真实 DSH Adapter)"""
        logger.info(f"  ↗  上行到 mock_dsh: {self.upstream_url}")
        self.upstream_ws = await websockets.connect(self.upstream_url)
        await self.upstream_ws.send(json.dumps({
            "type": "client/hello",
            "device_id": "p4c5_asr_adapter",
        }))
        logger.info(f"👋 hello: p4c5_asr_adapter")

    async def on_text(self, text: str, meta: dict):
        """VAD finalize → ASR 识别 → 上行 user_input 到 mock_dsh"""
        if not self.upstream_ws:
            logger.warning("upstream 未连接, 丢弃识别结果")
            return
        try:
            await self.upstream_ws.send(json.dumps({
                "type": "client/user_input",
                "text": text,
                "duration_ms": meta.get("duration_ms", 0),
            }))
            logger.info(f"💬 上行 user_input: \"{text[:50]}\"")
        except Exception as e:
            logger.error(f"upstream 发送失败: {e}")

    async def handle_p4c5_client(self, ws):
        """处理 ESP32 P4C5 上行 OPUS 音频"""
        remote = ws.remote_address
        logger.info(f"✅ 新连接: {remote}")

        # 准备 OPUS 解码器
        decoder = OpusDecoder() if HAS_OPUS else MockOpusDecoder()
        vad = VADSession(self.asr, self.on_text)

        # 上行帧计数
        frame_count = 0

        try:
            async for message in ws:
                # 文本帧: 控制命令
                if isinstance(message, str):
                    try:
                        data = json.loads(message)
                        if data.get("type") == "client/hello":
                            logger.info(f"👋 hello: {data.get('device_id')}")
                    except json.JSONDecodeError:
                        pass
                    continue

                # 二进制帧: OPUS 音频
                if not isinstance(message, bytes) or len(message) == 0:
                    continue

                frame_count += 1
                if frame_count % 50 == 1:
                    logger.info(f"📦 binary msg #{frame_count} len={len(message)}")

                try:
                    pcm = decoder.decode(message)
                    await vad.feed(pcm, time.time())
                except Exception as e:
                    logger.warning(f"frame {frame_count} 处理失败: {e}")

        except websockets.ConnectionClosed:
            pass
        finally:
            logger.info(f"❌ 断开: {remote}")

    async def run(self, host: str = "0.0.0.0"):
        """启动 ASR Adapter 服务"""
        logger.info(f"🚀 P4C5 ASR Adapter 启动 (OMT 同等配置)")
        logger.info(f"   监听: ws://{host}:{self.port}")
        logger.info(f"   ASR 引擎: {type(self.asr).__name__}")
        logger.info(f"   OPUS 解码: {'opuslib' if HAS_OPUS else 'mock'}")
        logger.info(f"   上行 upstream: {self.upstream_url}")

        await self.connect_upstream()

        async with websockets.serve(self.handle_p4c5_client, host, self.port):
            await asyncio.Future()  # run forever


# ══════════════════════════════════════════════════════════
# CLI 入口
# ══════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="P4C5 ASR Adapter (OMT 同等配置: 阿里云 NLS ISI)")
    parser.add_argument("--port", type=int, default=8766,
                        help="ESP32 监听端口 (default: 8766)")
    parser.add_argument("--host", default="0.0.0.0",
                        help="监听地址 (default: 0.0.0.0)")
    parser.add_argument("--upstream", default="ws://127.0.0.1:8765/ws",
                        help="DSH upstream 地址")
    parser.add_argument("--mock", action="store_true",
                        help="Mock ASR 模式 (无需阿里云 key)")
    args = parser.parse_args()

    # 选择 ASR 引擎 (与 OMT asr.js 的"无 key 报错"对比, 我们加 mock 模式)
    if args.mock or not (os.environ.get("ALIYUN_ACCESS_KEY_ID")
                         and os.environ.get("ALIYUN_ACCESS_KEY_SECRET")
                         and os.environ.get("ALIYUN_NLS_APPKEY")):
        logger.warning("⚠  使用 Mock ASR 模式 (无 key 或显式 --mock)")
        asr = MockASREngine()
    else:
        logger.info("✅ 使用 阿里云 NLS ISI 真实 ASR")
        asr = NLSASREngine(
            access_key_id=os.environ["ALIYUN_ACCESS_KEY_ID"],
            access_key_secret=os.environ["ALIYUN_ACCESS_KEY_SECRET"],
            appkey=os.environ["ALIYUN_NLS_APPKEY"],
        )

    server = P4C5ASRServer(
        port=args.port,
        asr_engine=asr,
        upstream_url=args.upstream,
    )
    try:
        asyncio.run(server.run(args.host))
    except KeyboardInterrupt:
        logger.info("Bye")


if __name__ == "__main__":
    main()