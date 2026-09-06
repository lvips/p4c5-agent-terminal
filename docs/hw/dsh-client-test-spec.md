# dsh_client 集成测试规范

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA T8（P4C5 dsh_client 实现验证）
> **配套文档**：`dsh-client-frame-protocol.md` + `M5-verification-report.md`
> **参考**：OMT `test_dsh_client.cpp` + `mock_dsh` 模拟器

---

## 1. 测试目标

验证 P4C5 终端 dsh_client 协议层的完整功能，确保与 DSH Adapter（Mac/PC 端）100% 兼容。

**测试范围**：
1. WebSocket 连接生命周期（连接/断开/重连/错误恢复）
2. 14 类下行帧正确解析 + 分发到 event_bus
3. 4 类上行帧正确编码 + 发送
4. 大帧多包拼接（>4096 字节 JSON）
5. 心跳机制（30s ping / 60s pong 超时）
6. WiFi↔4G 网络切换稳定性
7. 24h 长稳（心跳准确，0 丢帧）

---

## 2. 测试环境

### 2.1 硬件

| 项 | 规格 | 备注 |
|---|---|---|
| 开发板 | ESP32-P4C5 酷世 DIY | v1.3 芯片 |
| 供电 | USB-C 5V/2A | |
| 网络 | WiFi（2.4GHz）+ 4G（ML307C）| 双网络测试 |
| 串口 | USB-CDC | 115200bps，日志记录 |

### 2.2 软件

| 项 | 版本 | 说明 |
|---|---|---|
| 固件 | p4c5-agent-terminal | CCA T8 构建 |
| IDF | ≥ 5.5.5 | 含 esp_websocket_client |
| Mock 服务 | Python WebSocket 服务器 | 模拟 DSH Adapter |
| 串口终端 | `idf.py monitor` | 全程日志 |
| 网络抓包 | Wireshark / tcpdump | 可选，验证帧格式 |

### 2.3 Mock 服务器

建议用 Python `websockets` 库搭建本地 mock：

```python
import asyncio, json, websockets

async def handler(ws):
    print("Client connected")
    while True:
        msg = await ws.recv()
        frame = json.loads(msg)
        print(f"Received: {frame['type']}")
        
        # 模拟回复
        if frame['type'] == 'text':
            await ws.send(json.dumps({
                'type': 'session_state', 'state': 'running'
            }))
            await ws.send(json.dumps({
                'type': 'assistant_text', 'content': 'Hello from mock!'
            }))
            await ws.send(json.dumps({
                'type': 'assistant_done', 'stop_reason': 'end_turn',
                'usage': {'input_tokens': 10, 'output_tokens': 5},
                'duration_ms': 100
            }))

async def main():
    async with websockets.serve(handler, '0.0.0.0', 8080):
        await asyncio.Future()

asyncio.run(main())
```

> Mock 服务器需支持：发送全部 14 类下行帧、接收 4 类上行帧、模拟大帧（>4096 字节）。

---

## 3. 测试用例

### 3.1 连接层（TC-DSH-01 ~ TC-DSH-05）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-01 | WS 连接成功 | 1. 启动 mock 服务器 2. `dsh_client_set_uri("ws://<ip>:8080")` 3. `dsh_client_connect()` | 状态变为 CONNECTED，日志输出 "WebSocket 已连接" | | |
| TC-DSH-02 | 错误 URL | 1. `set_uri("ws://192.0.2.1:9999")`（不可达地址）2. `connect()` | 状态 CONNECTING → RECONNECTING，每 10s 重试 | | |
| TC-DSH-03 | 30 次重连上限 | 1. 保持错误 URL 2. 等待 30 次重连 | 30 次后状态变为 ERROR，停止重连，日志 "重连已达上限" | | |
| TC-DSH-04 | 手动恢复 | 1. 状态为 ERROR 2. 调用 `dsh_client_start_conversation()` | 重置计数器，重新连接，状态恢复 CONNECTED | | |
| TC-DSH-05 | 主动断开 | 1. CONNECTED 状态 2. `dsh_client_stop_conversation()` | 发送 stop 帧 → 禁用重连 → 状态 DISCONNECTED，日志 "对话已结束" | | |

**TC-DSH-01 详细检查**：
- [ ] 日志包含 "WebSocket 已连接"
- [ ] `dsh_client_get_state() == DshState::CONNECTED`
- [ ] `dsh_client_get_uptime_sec() > 0`
- [ ] free_heap 无明显下降（<1KB 变化）

**TC-DSH-05 详细检查**：
- [ ] stop 帧已发送（`esp_websocket_client_send_text` 返回 > 0）
- [ ] 自动重连已禁用
- [ ] `s_manual_disconnect == true`
- [ ] 重连定时器已停止

### 3.2 帧编解码（TC-DSH-06 ~ TC-DSH-10）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-06 | text 上行帧 | 1. `dsh_client_send_text("hello")` | Mock 收到 `{"type":"text","content":"hello"}` | | |
| TC-DSH-07 | session_state 下行 | Mock 发送 `{"type":"session_state","state":"running"}` | event_bus 收到 SESSION_STATE 事件，content="running" | | |
| TC-DSH-08 | assistant_text 流式 | Mock 连续发送 10 个 assistant_text 帧 | event_bus 按序收到 10 个 ASSISTANT_TEXT 事件 | | |
| TC-DSH-09 | 大帧多包拼接 | Mock 发送 >4096 字节 assistant_text | 客户端正确拼接，JSON 解析成功，content 完整 | | |
| TC-DSH-10 | 畸形 JSON | Mock 发送 `{invalid json` | 客户端日志 "JSON 解析失败"，不崩溃，继续接收 | | |

**TC-DSH-09 详细检查**（关键！OMT 实测 bug）：
- [ ] 发送 8192 字节 JSON（`assistant_text` + 长 content）
- [ ] 客户端 `s_ws_rx_buffer` 动态扩容到 16KB+
- [ ] `payload_offset` 连续性检查通过
- [ ] 最终 `parse_and_dispatch_event` 收到完整 JSON
- [ ] 拼接完成后 `s_ws_rx_buffer_len` 归零

**全帧类型遍历测试**：

| 下行帧 type | event_bus EventType | 关键字段验证 |
|---|---|---|
| `session_state` | SESSION_STATE | state, model, agent_name |
| `thinking` | THINKING | content, tokens_estimate |
| `assistant_text` | ASSISTANT_TEXT | content |
| `assistant_done` | ASSISTANT_DONE | stop_reason, usage, duration_ms |
| `tool_call` | TOOL_CALL | call_id, tool_name, arguments |
| `tool_result` | TOOL_RESULT | call_id, is_error, output |
| `tool_progress` | TOOL_PROGRESS | call_id, elapsed_seconds |
| `file_change` | FILE_CHANGE | action, path |
| `authorization/requested` | AUTH_REQUESTED | request_id, tool_name, risk_level |
| `authorization/resolved` | AUTH_RESOLVED | request_id |
| `question/requested` | QUESTION_REQUESTED | request_id |
| `question/resolved` | QUESTION_RESOLVED | request_id |
| `error` | ERROR | content |
| `pong` | PONG | — |

### 3.3 业务层（TC-DSH-11 ~ TC-DSH-14）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-11 | 完整对话流 | 1. 发送 text 2. Mock 回复 session_state→thinking→assistant_text→assistant_done | event_bus 按序收到全部事件 | | |
| TC-DSH-12 | 工具调用流 | Mock 发送 tool_call→tool_progress→tool_result | call_id 配对正确，UI 显示工具名和进度 | | |
| TC-DSH-13 | 授权请求流 | Mock 发送 authorization/requested → 客户端回复 auth_response | request_id 配对，decision 正确发送 | | |
| TC-DSH-14 | 会话控制 | 发送 session_ctrl "new"/"stop"/"cancel" | Mock 收到对应 action，会话状态正确变更 | | |

**TC-DSH-11 时序验证**：
```
T=0s    Client → {"type":"text","content":"你好"}
T=0.1s  Server → {"type":"session_state","state":"running"}
T=0.5s  Server → {"type":"thinking","content":"让我想想..."}
T=1.0s  Server → {"type":"assistant_text","content":"你好！"}
T=1.1s  Server → {"type":"assistant_text","content":"有什么"}
T=1.2s  Server → {"type":"assistant_text","content":"可以帮你的？"}
T=1.3s  Server → {"type":"assistant_done","stop_reason":"end_turn",...}
```

### 3.4 音频层（TC-DSH-15 ~ TC-DSH-16）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-15 | 音频上行 | 1. 录音 20ms Opus 帧 2. `dsh_client_send_audio(data, len)` | Mock 收到二进制帧（opcode 0x02）| | |
| TC-DSH-16 | 未连接发音频 | 1. DISCONNECTED 状态 2. `send_audio()` | 返回 false，日志 "未连接，无法发送音频" | | |

**TC-DSH-15 详细检查**：
- [ ] 二进制帧 opcode = 0x02
- [ ] payload 长度正确
- [ ] Mock 能解码 Opus 数据
- [ ] 发送期间 `dsh_client_get_state() == CONNECTED`

### 3.5 网络层（TC-DSH-17 ~ TC-DSH-18）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-17 | WiFi 断线重连 | 1. WiFi 连接中 2. 关闭 WiFi 路由器 3. 重新开启 | 检测到断开 → 10s 后重连 → 恢复 CONNECTED | | |
| TC-DSH-18 | WiFi↔4G 切换 | 1. WiFi 连接中 2. 断开 WiFi 3. 4G 自动接管 | 30s 内完成切换，WebSocket 重连成功 | | |

**TC-DSH-18 详细检查**（P4C5 特有）：
- [ ] WiFi 断开后 4G 拨号成功
- [ ] dsh_client URI 无需变更（通过 WireGuard 隧道）
- [ ] 切换期间丢失的心跳 ≤ 3 个（30s × 3 = 90s 内恢复）
- [ ] event_bus 收到 DISCONNECTED 事件后恢复

### 3.6 长稳测试（TC-DSH-19 ~ TC-DSH-20）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-DSH-19 | 24h 心跳 | 持续运行 24h，统计 ping/pong | 心跳准确（30s ±1s），0 次超时 | | |
| TC-DSH-20 | 24h 收发 | 每 5 分钟发送 1 次 text + 接收回复 | 1000+ 帧不丢失，JSON 解析 100% 成功 | | |

**TC-DSH-19 详细检查**：
- [ ] 24h 内 ping 发送次数 ≈ 2880（24×60×2/30）
- [ ] pong 接收次数 = ping 发送次数（0 丢失）
- [ ] 连接时长 `dsh_client_get_uptime_sec()` ≈ 86400
- [ ] free_heap 波动 < 5%
- [ ] 无 panic / Guru Meditation

**TC-DSH-20 统计指标**：
- [ ] 发送帧数 ≥ 288（每 5 分钟 1 帧 × 24h）
- [ ] 接收帧数 ≥ 288
- [ ] JSON 解析成功率 = 100%
- [ ] event_bus 推送成功率 = 100%
- [ ] 内存泄漏 < 5%

---

## 4. Mock 服务器测试矩阵

### 4.1 异常场景

| 场景 | Mock 行为 | 期望客户端行为 |
|---|---|---|
| 服务端主动关闭 | 发送 WS Close 帧 | 客户端检测到 → RECONNECTING |
| 服务端无响应 | 不回复 ping | 60s 后 pong 超时 → 断开 → 重连 |
| 发送畸形 JSON | `{"type": invalid}` | 客户端 "JSON 解析失败"，不崩溃 |
| 发送未知 type | `{"type":"unknown_type"}` | 客户端 "未知事件类型"，推送 UNKNOWN |
| 连续发送 100 帧 | 快速发送 100 个 assistant_text | 全部按序接收，无丢失 |
| 空 payload | WS 空文本帧 | 客户端忽略，不报错 |
| 超大帧 | 发送 100KB JSON | 客户端 buffer 扩容，正确解析 |

### 4.2 压力场景

| 场景 | Mock 行为 | 期望客户端行为 |
|---|---|---|
| 高频发送 | 每 100ms 发送 1 帧 | 客户端正常接收，event_bus 不溢出 |
| 长内容 | 单帧 50KB JSON | 多包拼接成功，解析正确 |
| 并发会话 | 多个 session_state 切换 | 客户端正确处理 session_id |

---

## 5. 验收标准

### 5.1 必须通过（P0）

| TC | 要求 |
|---|---|
| TC-DSH-01 | ✅ WS 连接成功 |
| TC-DSH-06 | ✅ text 上行帧正确 |
| TC-DSH-07 | ✅ session_state 下行解析 |
| TC-DSH-08 | ✅ assistant_text 流式接收 |
| TC-DSH-09 | ✅ 大帧多包拼接 |
| TC-DSH-11 | ✅ 完整对话流 |
| TC-DSH-15 | ✅ 音频上行 |

### 5.2 建议通过（P1）

| TC | 要求 |
|---|---|
| TC-DSH-02~05 | ✅ 重连 + 错误处理 |
| TC-DSH-10 | ✅ 畸形 JSON 不崩溃 |
| TC-DSH-12~14 | ✅ 工具/授权/会话控制 |
| TC-DSH-17~18 | ✅ 网络切换 |

### 5.3 可选通过（P2）

| TC | 要求 |
|---|---|
| TC-DSH-19~20 | ✅ 24h 长稳（可延后到 M8）|

---

## 6. 风险标注

| 风险 | 描述 | 关联 TC | 缓解 |
|---|---|---|---|
| R-DSH-1 | buffer_size=4096 导致大帧截断 | TC-DSH-09 | 多包拼接机制（已验证 OMT） |
| R-DSH-2 | reconnect_task 栈溢出 | TC-DSH-02 | 独立 12KB 栈任务（OMT P0 修复）|
| R-DSH-3 | stop_conversation 并发崩溃 | TC-DSH-05 | volatile bool 防重入 + 200ms 超时 |
| R-DSH-4 | WiFi↔4G 切换丢帧 | TC-DSH-18 | WireGuard 隧道 IP 不变 |
| R-DSH-5 | 内存泄漏（24h 运行）| TC-DSH-19 | 每次事件 free(content/call_id/extra) |

---

## 7. 测试记录模板

### 7.1 单次测试结果

```
测试日期：____________
固件 commit：____________
Mock 服务器版本：____________

TC-DSH-XX：____________
  步骤：____________
  期望：____________
  实测：____________
  状态：[ ] PASS / [ ] FAIL
  备注：____________
```

### 7.2 总结报告

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  dsh_client 集成测试报告
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
测试日期：____________
测试人员：____________

━━ 结果汇总 ━━
  总测试项：20
  通过：_______
  失败：_______
  未测：_______
  通过率：_______ %

━━ P0 必须通过项 ━━
  TC-DSH-01: [ ] PASS / [ ] FAIL
  TC-DSH-06: [ ] PASS / [ ] FAIL
  TC-DSH-07: [ ] PASS / [ ] FAIL
  TC-DSH-08: [ ] PASS / [ ] FAIL
  TC-DSH-09: [ ] PASS / [ ] FAIL
  TC-DSH-11: [ ] PASS / [ ] FAIL
  TC-DSH-15: [ ] PASS / [ ] FAIL

━━ 结论 ━━
  _______________
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：CCA T8 实现完成后，补充实测数据
