# dsh_client 协议实操指南

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA / DSH / 任何开发者（10 分钟跑通联调）
> **配套文档**：`dsh-client-frame-protocol.md` + `M9-integration-test-spec.md`
> **Mock 服务端**：`tools/mock_dsh_server.py`

---

## 1. 目标

本文档提供 **从零到联调** 的完整步骤，让开发者 10 分钟内跑通：

1. PC 端启动 Mock DSH 服务端
2. P4C5 固件编译 + 烧录
3. 通过 4G/WiFi 连接到 Mock 服务端
4. 验证完整对话流（hello → heartbeat → user_input → assistant_text → done）

同时提供 5 个真实场景示例 + Python 客户端脚本 + 故障排查表。

---

## 2. 5 分钟上手

### 2.1 安装依赖

```bash
# PC 端（运行 Mock 服务端）
pip3 install websockets

# 验证安装
python3 -c "import websockets; print(websockets.__version__)"
```

### 2.2 启动 Mock 服务端

```bash
# 终端 1：启动 Mock
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5
python3 tools/mock_dsh_server.py --port 8765

# 预期输出：
# ==================================================
# 🚀 Mock DSH WebSocket Server
#    地址: ws://0.0.0.0:8765/ws
#    协议: P4C5 dsh_client v0.1.0
#    下行帧: 14 类
#    上行帧: 4 类
# ==================================================
```

### 2.3 查询 PC IP 地址

```bash
# macOS
ifconfig en0 | grep "inet " | awk '{print $2}'
# 例：192.168.1.100

# Linux
ip addr show | grep "inet " | grep -v 127.0.0.1
```

### 2.4 修改固件中的 Mock URL

在 `app_main.c` 中修改 dsh_client URL：

```c
// 修改前（占位）：
.url = "ws://dsh.example.com/ws",

// 修改后（替换为 PC IP）：
.url = "ws://192.168.1.100:8765/ws",
```

### 2.5 编译 + 烧录

```bash
# 终端 2：编译固件
. /Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5/export.sh
cd hardware/p4c5-agent-terminal
idf.py build

# 烧录（ESP32-P4 v1.3 需要 --force）
idf.py -p /dev/cu.usbmodem* flash --force

# 监控串口
idf.py -p /dev/cu.usbmodem* monitor
```

### 2.6 观察联调结果

**PC 端（Mock 服务端）预期输出**：

```
00:01:23 [INFO] ✅ 新连接: ('10.99.0.5', 45678)
00:01:23 [INFO] 📥 收到: client/hello {'type': 'client/hello', 'device_id': 'p4c5-001', ...}
00:01:23 [INFO]   📋 Hello from p4c5-001 v0.1.0, caps=['audio', 'display', 'touch', '4g']
00:01:23 [INFO] 📤 发送: session_state (85 bytes)
00:01:23 [INFO] 📤 发送: session_state (87 bytes)
00:01:53 [INFO] 📥 收到: client/heartbeat {'type': 'client/heartbeat', 'battery': 87, 'rssi': -75}
00:01:53 [INFO]   💓 Heartbeat #1: battery=87%, rssi=-75dBm
```

**P4C5 端（串口）预期输出**：

```
I (12345) app_main: 🟢 DSH connected
I (12345) app_main: 📡 DSH session: idle
I (12345) app_main: 📡 DSH session: running
I (42345) app_main: 💓 bat=87% csq=18 rssi=-75dBm dsh=✅ 4g=✅
```

---

## 3. 五个真实场景

### 场景 1：基本握手

验证设备注册 + 会话建立。

```
P4C5 串口                    Mock 服务端
─────────                    ──────────
[1/5] Board init...
[2/5] PMIC init...
[3/5] Display init...
[4/5] 4G init...
[5/5] DSH client init...
                            ✅ 新连接: (10.99.0.5, 45678)
── client/hello ──────────→ 📥 client/hello {device_id:"p4c5-001"}
                               📋 Hello from p4c5-001 v0.1.0
←── session_state(idle) ──── 📤 session_state
📡 DSH session: idle
←── session_state(running) ─ 📤 session_state
📡 DSH session: running
🟢 DSH connected
```

**验证点**：
- [ ] P4C5 日志出现 `🟢 DSH connected`
- [ ] Mock 日志出现 `📋 Hello from p4c5-001`
- [ ] 两个 `session_state` 帧按序收到

### 场景 2：文本对话

验证用户输入 → AI 回复流式输出。

```
P4C5 串口                    Mock 服务端
─────────                    ──────────
（手动触发 user_input）
── client/user_input ──────→ 📥 client/user_input {text:"现在几点了?"}
                               💬 User input: 现在几点了?
                               📤 thinking {content:"让我分析一下..."}
←── thinking ───────────────
🤔 Thinking...
←── assistant_text(chunk1) ── 📤 assistant_text
收到您的消息:
←── assistant_text(chunk2) ── 📤 assistant_text
"现在几点了?"。
←── assistant_text(chunk3) ── 📤 assistant_text
这是 Mock DSH 的自动回复。
←── assistant_done ────────── 📤 assistant_done
📝 Assistant done
```

**验证点**：
- [ ] `🤔 Thinking...` 出现
- [ ] 3 段 `assistant_text` 按序打印
- [ ] `📝 Assistant done` 出现
- [ ] 完整文本可拼接

### 场景 3：工具调用

验证 Agent 请求工具 → 设备执行 → 返回结果。

```
P4C5 串口                    Mock 服务端
─────────                    ──────────
（每 3 次 user_input 自动触发）
←── tool_call ────────────── 📤 tool_call {name:"get_device_status"}
🔧 Tool call: get_device_status (id=tool_xxx_1)
（设备执行工具...）
── client/tool_result ─────→ 📥 client/tool_result {id:"tool_xxx_1", status:"success"}
                               🔧 Tool result: id=tool_xxx_1, status=success
←── tool_progress ────────── 📤 tool_progress
```

**验证点**：
- [ ] `🔧 Tool call: get_device_status` 出现
- [ ] `client/tool_result` 包含正确 tool_id
- [ ] Mock 日志确认收到 result

### 场景 4：心跳保活

验证 30s 间隔心跳 + 电池/信号数据。

```
P4C5 串口                    Mock 服务端
─────────                    ──────────
（每 30s 自动发送）
── client/heartbeat ───────→ 📥 client/heartbeat
💓 bat=87% csq=18 ...          💓 Heartbeat #1: battery=87%, rssi=-75dBm

（30s 后）
── client/heartbeat ───────→ 📥 client/heartbeat
💓 bat=87% csq=17 ...          💓 Heartbeat #2: battery=87%, rssi=-75dBm

（30s 后）
── client/heartbeat ───────→ 📥 client/heartbeat
💓 bat=86% csq=18 ...          💓 Heartbeat #3: battery=86%, rssi=-75dBm
```

**验证点**：
- [ ] 心跳间隔 ≈ 30s ± 2s
- [ ] battery 值来自 PMIC（`p4c5_pmic_get_battery_level()`）
- [ ] rssi 值来自 4G（`p4c5_4g_get_rssi_dbm()`）
- [ ] 连续 10 次心跳无丢失

### 场景 5：断连重连

验证 Mock 主动断开后 P4C5 自动重连。

```bash
# 启动 Mock（3 次心跳后断开）
python3 tools/mock_dsh_server.py --port 8765 --disconnect-after 3
```

```
P4C5 串口                    Mock 服务端
─────────                    ──────────
── heartbeat #1 ───────────→ 💓 Heartbeat #1
── heartbeat #2 ───────────→ 💓 Heartbeat #2
── heartbeat #3 ───────────→ 💓 Heartbeat #3
                               🔌 模拟断连（第 3 次心跳后）
🔴 DSH disconnected          🔌 连接关闭
🔄 DSH reconnecting...
（10s 后重连）
                            ✅ 新连接: (10.99.0.5, 45679)
── client/hello ──────────→ 📥 client/hello
🟢 DSH connected
```

**验证点**：
- [ ] 3 次心跳后 Mock 主动关闭
- [ ] P4C5 检测到断连 → `🔴 DSH disconnected`
- [ ] 自动重连 → `🔄 DSH reconnecting...`
- [ ] 重新 hello → `🟢 DSH connected`

---

## 4. Python 客户端脚本

用于独立测试（不依赖 P4C5 硬件）：

```python
#!/usr/bin/env python3
"""test_p4c5_protocol.py — 独立测试 P4C5 dsh_client 协议"""

import asyncio
import json
import websockets

MOCK_URL = "ws://127.0.0.1:8765/ws"

async def test_handshake():
    """测试 1：握手流程"""
    print("=== 测试 1：握手 ===")
    async with websockets.connect(MOCK_URL) as ws:
        # 发送 client/hello
        hello = {
            "type": "client/hello",
            "device_id": "test-client",
            "version": "0.1.0",
            "capabilities": ["audio", "display"]
        }
        await ws.send(json.dumps(hello))

        # 接收 session_state
        msg1 = json.loads(await ws.recv())
        assert msg1["type"] == "session_state"
        assert msg1["state"] == "idle"
        print(f"  ✅ 收到 session_state(idle)")

        msg2 = json.loads(await ws.recv())
        assert msg2["type"] == "session_state"
        assert msg2["state"] == "running"
        print(f"  ✅ 收到 session_state(running)")

async def test_conversation():
    """测试 2：对话流"""
    print("=== 测试 2：对话流 ===")
    async with websockets.connect(MOCK_URL) as ws:
        # 先 hello
        await ws.send(json.dumps({
            "type": "client/hello",
            "device_id": "test-client"
        }))
        await ws.recv()  # idle
        await ws.recv()  # running

        # 发送 user_input
        await ws.send(json.dumps({
            "type": "client/user_input",
            "text": "测试消息"
        }))

        # 接收 thinking + 3×assistant_text + assistant_done
        frames = []
        for _ in range(5):
            msg = json.loads(await asyncio.wait_for(ws.recv(), timeout=5))
            frames.append(msg["type"])

        assert frames == ["thinking", "assistant_text", "assistant_text",
                          "assistant_text", "assistant_done"]
        print(f"  ✅ 帧序列正确: {' → '.join(frames)}")

async def test_heartbeat():
    """测试 3：心跳"""
    print("=== 测试 3：心跳 ===")
    async with websockets.connect(MOCK_URL) as ws:
        await ws.send(json.dumps({
            "type": "client/hello",
            "device_id": "test-client"
        }))
        await ws.recv()
        await ws.recv()

        # 发送 3 次心跳
        for i in range(3):
            await ws.send(json.dumps({
                "type": "client/heartbeat",
                "timestamp": 1234567890 + i * 30,
                "battery": 87 - i,
                "rssi": -75
            }))
            await asyncio.sleep(0.5)
        print(f"  ✅ 3 次心跳发送成功")

async def main():
    await test_handshake()
    await test_conversation()
    await test_heartbeat()
    print("\n✅ 全部测试通过！")

if __name__ == "__main__":
    asyncio.run(main())
```

运行：
```bash
# 终端 1：Mock 服务端
python3 tools/mock_dsh_server.py --port 8765

# 终端 2：测试脚本
python3 test_p4c5_protocol.py
```

---

## 5. 故障排查

| 现象 | 可能原因 | 解决 |
|---|---|---|
| `Connection refused` | Mock 未启动 / 端口错 | `lsof -i :8765` 检查 |
| `AUTH_FAIL` | auth_token 不匹配 | Mock 默认接受 `test-token` 或空 |
| 帧无响应 | frame_type 拼写错 | 核对 `dsh_client_frames.h` 常量 |
| 心跳超时 | Mock `--disconnect-after` | 去掉该参数 |
| `404 Not Found` | URL 路径缺 `/ws` | URL 必须 `ws://host:port/ws` |
| `JSON parse failed` | 帧格式错 | 确保 `{"type":"xxx",...}` |
| `Unknown frame` | 帧类型不在 18 类中 | 检查拼写 |
| 大帧截断 | buffer_size=4096 限制 | 确认客户端多包拼接逻辑 |
| 重连循环 | Mock 频繁断连 | 检查网络 / Mock 日志 |
| 无 heartbeat | 心跳任务未启动 | 确认 `dsh_client_start_heartbeat()` |
| 4G 无网络 | SIM / 信号 / APN | 参考 M9 TC-M9-01~06 |
| ALDO4 电压不足 | R4 风险 | 万用表测，修改 0x95 寄存器 |

### 5.1 快速诊断命令

```bash
# 检查 Mock 服务端运行
lsof -i :8765

# 检查 P4C5 USB 连接
ls /dev/cu.usbmodem*

# 检查 IDF 环境
idf.py --version

# 串口抓帧（过滤 dsh_client 日志）
idf.py monitor | grep -E "dsh_client|DSH|📡|🟢|🔴"

# Mock 详细模式
python3 tools/mock_dsh_server.py --port 8765 --verbose
```

---

## 6. 协议速查

### 6.1 上行帧（P4C5 → Mock）

| 帧类型 | 关键字段 | 触发时机 |
|---|---|---|
| `client/hello` | device_id, version, capabilities[] | 连接成功后自动 |
| `client/heartbeat` | timestamp, battery, rssi | 每 30s 自动 |
| `client/user_input` | text | 用户输入 |
| `client/tool_result` | id, status, result | 工具执行完 |

### 6.2 下行帧（Mock → P4C5）

| 帧类型 | 关键字段 | 触发时机 |
|---|---|---|
| `session_state` | state, model, agent_name | hello 后 |
| `thinking` | content, tokens_estimate | 处理用户输入 |
| `assistant_text` | content, message_id | 流式回复 |
| `assistant_done` | stop_reason, usage, duration_ms | 回复结束 |
| `tool_call` | id, tool_name, arguments | Agent 调用工具 |
| `system_event` | event, code, message | 系统通知/错误 |

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：M10 真机联调后补充实测截图
