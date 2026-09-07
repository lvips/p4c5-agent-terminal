#!/usr/bin/env python3
"""
Mock DSH WebSocket Server — P4C5 集成测试用

替代真实 DSH Adapter（Mac/PC 端），为 ESP32-P4C5 固件提供
WebSocket 服务端。支持 P4C5 dsh_client 协议层全部 18 类帧。

使用方法:
  pip install websockets
  python3 tools/mock_dsh_server.py [--port 8765] [--host 0.0.0.0]

配合 P4C5 固件测试:
  1. 启动本脚本（默认 :8765）
  2. P4C5 固件设置 dsh_client_config_t.url = "ws://<pc-ip>:8765/ws"
  3. 观察串口日志 + 本脚本输出

协议参考: docs/hw/dsh-client-frame-protocol.md

作者: CCB (科研助理)
版本: v1.0 (2026-09-06)
"""

import asyncio
import json
import time
import uuid
import argparse
import logging
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Optional

try:
    import websockets
    # 新版 websockets (>=12.0) 使用 asyncio.server
    try:
        from websockets.asyncio.server import serve
        from websockets.asyncio.server import ServerConnection
        from websockets.http11 import Response
    except ImportError:
        # 旧版 websockets
        from websockets.server import serve
        ServerConnection = None
        Response = None
except ImportError:
    print("错误: 请先安装 websockets 库")
    print("  pip install websockets")
    sys.exit(1)

# W4: ASR Backend (1:1 OMT port, 见 tools/p4c5_asr_adapter.py)
sys.path.insert(0, str(Path(__file__).parent))
try:
    from p4c5_asr_adapter import (
        ASREngine, MockASREngine, NLSASREngine,
        OpusDecoder, MockOpusDecoder,
        VADSession, SAMPLE_RATE, CHANNELS,
    )
    HAS_ASR_ADAPTER = True
except ImportError as e:
    HAS_ASR_ADAPTER = False
    print(f"⚠️  p4c5_asr_adapter 导入失败: {e}")
    print("   ASR Backend 将不可用, 仅使用内嵌 Mock")

# ── 日志配置 ──
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S',
    force=True,  # 覆盖 urllib3 等第三方库可能已调 basicConfig 设的 handler
)
logger = logging.getLogger('mock-dsh')

# ── 统计 ──
stats = {
    'connections': 0,
    'frames_rx': 0,
    'frames_tx': 0,
    'bytes_rx': 0,
    'bytes_tx': 0,
    'errors': 0,
    'start_time': None,
}


def make_frame(frame_type, **kwargs):
    """构造下行帧"""
    frame = {"type": frame_type}
    frame.update(kwargs)
    return frame


def timestamp_now():
    return int(time.time())


class MockSession:
    """模拟一个 DSH 会话"""

    def __init__(self, session_id=None):
        self.session_id = session_id or str(uuid.uuid4())[:8]
        self.state = "idle"  # idle → running → completed
        self.created_at = datetime.now().isoformat()
        self.tool_call_counter = 0
        self.message_counter = 0

    def next_tool_call_id(self):
        self.tool_call_counter += 1
        return f"tool_{self.session_id}_{self.tool_call_counter}"

    def next_message_id(self):
        self.message_counter += 1
        return f"msg_{self.session_id}_{self.message_counter}"


class MockDshServer:

    @staticmethod
    def _make_default_asr_engine():
        """根据环境变量自动选 backend

        ALIYUN_ACCESS_KEY_ID / ALIYUN_ACCESS_KEY_SECRET / ALIYUN_NLS_APPKEY
        三个都设了 → 阿里云 NLS ISI (REST, 1:1 OMT port)
        否则 → Mock
        """
        ak_id = os.environ.get("ALIYUN_ACCESS_KEY_ID")
        ak_secret = os.environ.get("ALIYUN_ACCESS_KEY_SECRET")
        appkey = os.environ.get("ALIYUN_NLS_APPKEY")
        if ak_id and ak_secret and appkey:
            logger.info(f"🔑 检测到 ALIYUN_NLS 凭证 → 用 NLSASREngine (REST, appkey={appkey[:6]}...)")
            return NLSASREngine(ak_id, ak_secret, appkey)
        logger.info("🎭 无 ALIYUN_NLS 凭证 → 用 MockASREngine")
        return MockASREngine()

    """Mock DSH WebSocket 服务端"""

    def __init__(self, host='0.0.0.0', port=8765,
                 asr_engine: Optional[ASREngine] = None,
                 use_opus_decoder: bool = True):
        self.host = host
        self.port = port
        self.sessions = {}  # ws → MockSession
        self.auto_reply = True  # 是否自动回复 user_input
        self.simulate_disconnect = False  # 是否模拟断连
        self.simulate_large_frame = False  # 是否发送大帧（测试多包拼接）
        self.disconnect_after_n_heartbeats = 0  # N 次心跳后断开（0=不断）
        self.heartbeat_count = {}  # ws → count
        self.broadcast_text = False  # W3: 是否把 assistant_text 广播给所有客户端 (供 TTS Adapter 订阅)

        # ── W4: ASR Backend ──
        # 默认 backend 由环境变量选择:
        #   ALIYUN_ACCESS_KEY_ID/SECRET + ALIYUN_NLS_APPKEY → NLSASREngine (真阿里云)
        #   否则 → MockASREngine (内嵌 fallback)
        # 也可显式传入 asr_engine 覆盖
        if asr_engine is None and HAS_ASR_ADAPTER:
            asr_engine = self._make_default_asr_engine()
        self.asr_engine = asr_engine
        self.use_opus_decoder = use_opus_decoder and HAS_ASR_ADAPTER

        # OPUS 解码器 (用于 ASR backend)
        if self.use_opus_decoder:
            try:
                self.opus_decoder = OpusDecoder(SAMPLE_RATE, CHANNELS)
                logger.info("🎧 OPUS 解码器: libopus (opuslib)")
            except Exception:
                self.opus_decoder = MockOpusDecoder()
                logger.info("🎧 OPUS 解码器: Mock (opuslib 未装)")
        else:
            self.opus_decoder = None

        if self.asr_engine:
            logger.info(f"🎯 ASR backend: {type(self.asr_engine).__name__}")
        else:
            logger.info("🎯 ASR backend: 内嵌 Mock (没装 adapter)")

    async def handle_client(self, ws):
        """处理单个客户端连接"""
        remote = ws.remote_address
        logger.info(f"✅ 新连接: {remote}")
        stats['connections'] += 1

        session = MockSession()
        self.sessions[ws] = session
        self.heartbeat_count[ws] = 0

        try:
            async for message in ws:
                stats['frames_rx'] += 1
                stats['bytes_rx'] += len(message)

                # ★ W3: 区分文本帧 (JSON 控制) 和二进制帧 (OPUS 音频)
                # websockets 库: type==BINARY 是 OPUS 数据, type==TEXT 是 JSON
                if isinstance(message, bytes):
                    await self._handle_audio_binary(ws, session, message)
                    continue

                try:
                    data = json.loads(message)
                except json.JSONDecodeError:
                    logger.warning(f"❌ JSON 解析失败: {message[:100]}")
                    stats['errors'] += 1
                    await ws.send(json.dumps(make_frame("system_event",
                        event="error", code="INVALID_JSON",
                        message="JSON parse failed")))
                    continue

                frame_type = data.get('type', 'unknown')
                logger.info(f"📥 收到: {frame_type} {self._truncate(data)}")

                await self._dispatch(ws, session, frame_type, data)

        except websockets.ConnectionClosed as e:
            logger.info(f"🔌 连接关闭: {remote} (code={e.code}, reason={e.reason})")
        except Exception as e:
            logger.error(f"💥 异常: {remote}: {e}")
            stats['errors'] += 1
        finally:
            self.sessions.pop(ws, None)
            self.heartbeat_count.pop(ws, None)
            logger.info(f"🧹 清理完成: {remote}")

    async def _dispatch(self, ws, session, frame_type, data):
        """分发上行帧"""

        if frame_type == "client/hello":
            await self._handle_hello(ws, session, data)

        elif frame_type == "client/heartbeat":
            await self._handle_heartbeat(ws, session, data)

        elif frame_type == "client/user_input":
            await self._handle_user_input(ws, session, data)

        elif frame_type == "client/tool_result":
            await self._handle_tool_result(ws, session, data)

        else:
            logger.warning(f"⚠️  未知帧类型: {frame_type}")
            await self._send(ws, make_frame("system_event",
                event="unknown_frame", received_type=frame_type))

    async def _handle_hello(self, ws, session, data):
        """处理 client/hello → 返回 session_state"""
        device_id = data.get('device_id', 'unknown')
        version = data.get('version', '?')
        capabilities = data.get('capabilities', [])
        auth_token = data.get('auth_token')

        logger.info(f"  📋 Hello from {device_id} v{version}, "
                    f"caps={capabilities}, auth={'yes' if auth_token else 'no'}")

        # 认证检查（如果设置了 token）
        if auth_token and auth_token != "test-token":
            await self._send(ws, make_frame("system_event",
                event="error", code="AUTH_FAIL",
                message="Invalid auth token"))
            return

        # 发送 session_state: idle → running
        await self._send(ws, make_frame("session_state",
            session_id=session.session_id,
            state="idle",
            model="mock-claude",
            agent_name="mock-dsh"))

        session.state = "running"

        await self._send(ws, make_frame("session_state",
            session_id=session.session_id,
            state="running"))

    async def _handle_heartbeat(self, ws, session, data):
        """处理 client/heartbeat"""
        battery = data.get('battery', -1)
        rssi = data.get('rssi', 0)
        ts = data.get('timestamp', 0)

        self.heartbeat_count[ws] = self.heartbeat_count.get(ws, 0) + 1
        count = self.heartbeat_count[ws]

        logger.info(f"  💓 Heartbeat #{count}: battery={battery}%, rssi={rssi}dBm")

        # 模拟断连测试
        if (self.disconnect_after_n_heartbeats > 0 and
                count >= self.disconnect_after_n_heartbeats):
            logger.info(f"  🔌 模拟断连（第 {count} 次心跳后）")
            await ws.close(1000, "mock disconnect test")
            return

    async def _handle_user_input(self, ws, session, data):
        """处理 client/user_input → 模拟完整对话流"""
        text = data.get('text', '')
        logger.info(f"  💬 User input: {text[:80]}")

        if not self.auto_reply:
            return

        # 模拟完整对话流: thinking → assistant_text → assistant_done
        msg_id = session.next_message_id()

        # 1. thinking
        await self._send(ws, make_frame("thinking",
            content=f"让我分析一下: \"{text[:30]}...\"",
            tokens_estimate=len(text) * 2))
        await asyncio.sleep(0.3)

        # 2. assistant_text (流式)
        reply = f"收到您的消息: \"{text}\"。这是 Mock DSH 的自动回复。"
        # 分 3 段发送，模拟流式
        chunks = [reply[:len(reply)//3],
                  reply[len(reply)//3:2*len(reply)//3],
                  reply[2*len(reply)//3:]]
        for chunk in chunks:
            text_frame = make_frame("assistant_text",
                content=chunk,
                message_id=msg_id)
            await self._send(ws, text_frame)
            # ★ W3: 广播模式 - 同步发给所有客户端 (TTS Adapter 可订阅)
            if self.broadcast_text:
                await self._broadcast_frame(text_frame, exclude=ws)
            await asyncio.sleep(0.2)

        # 3. assistant_done
        await self._send(ws, make_frame("assistant_done",
            message_id=msg_id,
            stop_reason="end_turn",
            usage={"input_tokens": len(text), "output_tokens": len(reply)},
            duration_ms=700))

        # 4. 模拟 tool_call（每 3 次用户输入触发一次）
        if session.message_counter % 3 == 0:
            await self._simulate_tool_call(ws, session)
            await asyncio.sleep(0.3)
            # W2: 完整发送剩余 7 类下行帧
            await self._send_remaining_frames(ws, session)

        # 大帧测试
        if self.simulate_large_frame:
            await self._send_large_frame(ws, session)

    async def _handle_tool_result(self, ws, session, data):
        """处理 client/tool_result"""
        tool_id = data.get('id', '?')
        status = data.get('status', '?')
        result = data.get('result', '')

        logger.info(f"  🔧 Tool result: id={tool_id}, status={status}")
        if result:
            logger.info(f"     result: {result[:80]}")

        # 发送 tool_progress 确认
        await self._send(ws, make_frame("tool_progress",
            id=tool_id,
            elapsed_seconds=0.1,
            heartbeat=False))

    async def _simulate_tool_call(self, ws, session):
        """模拟一个 tool_call 帧"""
        tool_id = session.next_tool_call_id()

        await self._send(ws, make_frame("tool_call",
            id=tool_id,
            tool_name="get_device_status",
            arguments=json.dumps({"query": "battery"}),
            display={"title": "查询设备状态"}))

        logger.info(f"  🔧 发送 tool_call: {tool_id}")

    async def _send_remaining_frames(self, ws, session):
        """W2: 发送剩余的 6 类下行帧 (验证 14 类完整)"""
        # 1. tool_result (server 中继)
        await self._send(ws, make_frame("tool_result",
            id="tool_mid_001",
            name="web_search",
            status="ok",
            output="搜索结果: ESP32-P4 已发布 (Espressif 2024)"))
        await asyncio.sleep(0.2)

        # 2. file_change
        await self._send(ws, make_frame("file_change",
            path="/tmp/p4c5_data.bin",
            change_type="modified",
            size_bytes=4096))
        await asyncio.sleep(0.2)

        # 3. authorization/requested
        auth_id = f"auth_{session.session_id}_{session.message_counter}"
        await self._send(ws, make_frame("authorization/requested",
            id=auth_id,
            tool_name="git_push",
            description="推送代码到远程仓库",
            requires_user_approval=True))
        await asyncio.sleep(0.2)

        # 4. authorization/resolved (直接 resolve 上一个)
        await self._send(ws, make_frame("authorization/resolved",
            id=auth_id,
            decision="approved",
            reason="用户授权"))
        await asyncio.sleep(0.2)

        # 5. question/requested
        q_id = f"q_{session.session_id}_{session.message_counter}"
        await self._send(ws, make_frame("question/requested",
            id=q_id,
            question="请选择部署目标",
            options=["生产环境", "测试环境", "取消"],
            multi_select=False))
        await asyncio.sleep(0.2)

        # 6. question/resolved
        await self._send(ws, make_frame("question/resolved",
            id=q_id,
            selected=["测试环境"]))
        await asyncio.sleep(0.2)

        # 7. file_upload (server 让 client 上传)
        await self._send(ws, make_frame("file_upload",
            upload_id=f"upload_{session.session_id}",
            target_url="https://api.example.com/upload",
            content_type="application/octet-stream",
            size_bytes=2048,
            checksum="sha256:abc123..."))
        await asyncio.sleep(0.2)

        logger.info(f"  📋 [W2] 发送剩余 7 类下行帧完成")

    async def _send_large_frame(self, ws, session):
        """发送大帧（>4096 字节），测试客户端多包拼接"""
        large_content = "X" * 8000  # 8KB 内容
        await self._send(ws, make_frame("assistant_text",
            content=large_content,
            message_id=session.next_message_id(),
            note="large_frame_test"))
        logger.info(f"  📦 发送大帧: {8000 + 200} bytes")

    async def _send(self, ws, frame):
        """发送下行帧"""
        data = json.dumps(frame, ensure_ascii=False)
        stats['frames_tx'] += 1
        stats['bytes_tx'] += len(data)
        await ws.send(data)
        logger.info(f"📤 发送: {frame.get('type', '?')} ({len(data)} bytes)")

    async def _broadcast_frame(self, frame, exclude=None):
        """W3: 广播 JSON 帧给所有客户端 (TTS Adapter 订阅模式)

        用于在 broadcast_text=True 时, 把 assistant_text 也发给其他客户端
        (不只是触发 user_input 的那个)
        """
        data = json.dumps(frame, ensure_ascii=False)
        broadcast_count = 0
        for other_ws in list(self.sessions.keys()):
            if other_ws is exclude:
                continue
            # 检查连接状态 (websockets 库: state 属性)
            try:
                if other_ws.state.name in ("CLOSED", "CLOSING"):
                    continue
            except AttributeError:
                # 旧版 websockets 没有 state.name
                pass
            try:
                await other_ws.send(data)
                broadcast_count += 1
            except Exception as e:
                logger.warning(f"广播失败: {e}")
        if broadcast_count > 0:
            logger.info(f"📡 广播 {frame.get('type', '?')} 到 {broadcast_count} 个客户端")

    async def _send_binary(self, ws, data: bytes):
        """发送 WS Binary 帧 (W3: ASR 结果回放/下行 TTS 占位)"""
        stats['frames_tx'] += 1
        stats['bytes_tx'] += len(data)
        await ws.send(data)
        logger.info(f"📦 发送 Binary: {len(data)} bytes")

    async def _handle_audio_binary(self, ws, session, opus_frame: bytes):
        """W4: 处理 P4C5 上行的 OPUS 音频帧 (WS Binary)

        完整链路 (vs W3 POC):
          1. 解码 OPUS → PCM Int16 (libopus / MockOpusDecoder)
          2. VAD 检测 (RMS 80/50, 900ms 静音, 20s 硬截止)
          3. 触发 ASR (MockASREngine / NLSASREngine 阿里云 NLS ISI)
          4. 用识别文字作为 user_input 上行, 触发 mock LLM 对话流

        VAD 状态机复刻 OMT asr.js (1:1):
          - rmsOfInt16() 整数平方根
          - silence_started_at 时间戳 (不是 frame count)
          - 同一会话共享 PCM chunks, finalize 后 reset
        """
        # 懒初始化 session 字段
        if not hasattr(session, 'vad_session'):
            session.vad_session = VADSession(
                asr=self.asr_engine,
                on_text=lambda text: self._on_asr_text(ws, session, text),
            ) if self.asr_engine else None
            session.audio_frames = 0
            session.audio_bytes = 0

        session.audio_frames += 1
        session.audio_bytes += len(opus_frame)

        if not self.use_opus_decoder or not session.vad_session:
            # W3 fallback: 累计 100 帧就触发 (无 opus decoder / 无 backend)
            if session.audio_frames % 50 == 1:
                logger.info(f"🎤 [audio-frames-only] frame #{session.audio_frames} "
                            f"len={len(opus_frame)}, total={session.audio_bytes}")
            if session.audio_frames >= 100:
                await self._mock_asr_finalize(ws, session)
            return

        # 1. OPUS → PCM
        try:
            pcm_int16 = self.opus_decoder.decode(opus_frame)
        except Exception as e:
            logger.warning(f"opus decode 失败: {e}")
            return

        # 2. VAD 累积 + 触发 ASR
        await session.vad_session.feed(pcm_int16, time.time())

        # 日志
        if session.vad_session.frames % 50 == 1:
            elapsed = (time.time() - (session.vad_session.session_start_at or time.time()))
            logger.info(f"🎤 [vad] frames={session.vad_session.frames} "
                        f"samples={session.vad_session.samples} "
                        f"speech_started={session.vad_session.speech_started} "
                        f"elapsed={elapsed:.1f}s")

    async def _on_asr_text(self, ws, session, recognized_text: str):
        """ASR 完成后被 VADSession 回调"""
        source = type(self.asr_engine).__name__  # MockASREngine / NLSASREngine
        duration_ms = int((session.vad_session.samples / 16000) * 1000) if session.vad_session else 0
        logger.info(f"🎯 [{source}] 识别完成: \"{recognized_text}\" ({duration_ms}ms)")
        logger.info(f"   ↑ 上行 user_input → 触发 mock LLM 完整对话流")

        # ★ 用识别文字作为 user_input 上行
        await ws.send(json.dumps(make_frame(
            "client/user_input",
            session_id=session.session_id,
            text=recognized_text,
            source=source.lower(),
            duration_ms=duration_ms,
        )))

        # 等待 mock LLM 处理
        await asyncio.sleep(0.5)

        # ★ 触发 mock LLM 完整对话流
        await self._handle_user_input(ws, session, {
            'text': recognized_text,
            'source': source.lower(),
        })

        # 重置 VAD session 准备下一段语音
        session.vad_session.reset()

    async def _mock_asr_finalize(self, ws, session):
        """W3 fallback: 无 OPUS decoder / 无 backend 时, 累计帧数触发"""
        duration = session.audio_frames * 0.020  # 20ms 帧
        opus_kbps_est = (session.audio_bytes * 8 / 1024) / duration if duration > 0 else 0

        recognized_text = (
            f"[W3-Mock-ASR] 收到 {session.audio_bytes} bytes OPUS, "
            f"{session.audio_frames} 帧, {duration:.1f}s, "
            f"约 {opus_kbps_est:.1f} kbps"
        )

        logger.info(f"🎯 [w3-mock] 识别完成: \"{recognized_text}\"")

        session.audio_frames = 0
        session.audio_bytes = 0

        await ws.send(json.dumps(make_frame(
            "client/user_input",
            session_id=session.session_id,
            text=recognized_text,
            source="w3_mock_asr",
            duration_ms=int(duration * 1000),
        )))

        await asyncio.sleep(0.5)
        await self._handle_user_input(ws, session, {
            'text': recognized_text,
            'source': 'w3_mock_asr',
        })

    def _truncate(self, data, max_len=80):
        """截断 JSON 用于日志显示"""
        s = json.dumps(data, ensure_ascii=False)
        if len(s) > max_len:
            return s[:max_len] + "..."
        return s

    def print_stats(self):
        """打印统计信息"""
        uptime = time.time() - stats['start_time'] if stats['start_time'] else 0
        logger.info("=" * 50)
        logger.info("📊 统计信息:")
        logger.info(f"  运行时间: {uptime:.0f}s")
        logger.info(f"  连接次数: {stats['connections']}")
        logger.info(f"  接收帧数: {stats['frames_rx']} ({stats['bytes_rx']} bytes)")
        logger.info(f"  发送帧数: {stats['frames_tx']} ({stats['bytes_tx']} bytes)")
        logger.info(f"  错误次数: {stats['errors']}")
        logger.info("=" * 50)

    async def stats_printer(self):
        """每 60 秒打印统计"""
        while True:
            await asyncio.sleep(60)
            self.print_stats()

    async def run(self):
        """启动服务端"""
        stats['start_time'] = time.time()

        logger.info("=" * 50)
        logger.info("🚀 Mock DSH WebSocket Server")
        logger.info(f"   地址: ws://{self.host}:{self.port}/ws")
        logger.info(f"   协议: P4C5 dsh_client v{DSH_CLIENT_VERSION}")
        logger.info(f"   下行帧: 14 类")
        logger.info(f"   上行帧: 4 类")
        logger.info("=" * 50)
        logger.info("")
        logger.info("命令提示:")
        logger.info("  客户端连接后自动交互")
        logger.info("  Ctrl+C 停止服务器")
        logger.info("")

        # 路径检查：只接受 /ws（兼容 ESP32-P4 dsh_client）
        async def process_request(conn, request):
            req_path = getattr(request, 'path', None)
            if req_path is None and hasattr(request, 'headers'):
                # legacy API: 从 headers 解析
                req_path = request.headers.get('Path', '/')
            if req_path is None:
                req_path = '/'
            if req_path != '/ws':
                logger.warning(f"拒绝路径: {req_path} (期望 /ws)")
                if Response is not None:
                    return Response(404, "Not Found: please use /ws path\n")
            return None  # 继续处理

        serve_kwargs = dict(
            ping_interval=30,
            ping_timeout=60,
            max_size=65536,
        )
        if Response is not None:
            serve_kwargs['process_request'] = process_request

        async with serve(
            self.handle_client,
            self.host,
            self.port,
            **serve_kwargs,
        ):
            # 启动统计打印任务
            asyncio.create_task(self.stats_printer())
            await asyncio.Future()  # run forever


DSH_CLIENT_VERSION = "0.1.0"


def main():
    parser = argparse.ArgumentParser(
        description='Mock DSH WebSocket Server for P4C5 integration testing')
    parser.add_argument('--host', default='0.0.0.0',
                        help='监听地址 (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=8765,
                        help='监听端口 (default: 8765)')
    parser.add_argument('--no-auto-reply', action='store_true',
                        help='禁用自动回复 user_input')
    parser.add_argument('--large-frame', action='store_true',
                        help='每次 user_input 后发送 >4096B 大帧')
    parser.add_argument('--disconnect-after', type=int, default=0,
                        help='N 次心跳后主动断开（测试重连）')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='详细日志')
    parser.add_argument('--broadcast-text', action='store_true',
                        help='广播 assistant_text 给所有客户端 (TTS Adapter 订阅模式)')

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger('mock-dsh').setLevel(logging.DEBUG)

    server = MockDshServer(host=args.host, port=args.port)
    server.auto_reply = not args.no_auto_reply
    server.simulate_large_frame = args.large_frame
    server.disconnect_after_n_heartbeats = args.disconnect_after
    server.broadcast_text = args.broadcast_text

    # W4: 显示 backend 选择结果
    logger.info(f"🎯 ASR backend: {type(server.asr_engine).__name__}")
    logger.info(f"🎧 OPUS decoder: {type(server.opus_decoder).__name__ if server.opus_decoder else 'None'}")

    try:
        asyncio.run(server.run())
    except KeyboardInterrupt:
        logger.info("\n🛑 服务器已停止")
        server.print_stats()


if __name__ == '__main__':
    main()
