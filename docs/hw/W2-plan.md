# W2 计划 — DSH 验证 (WiFi 路径)

> **目标**: 验证 dsh_client 14 类下行帧 + 4 类上行帧
> **前置**: W1 WiFi 链路完成 ✅
> **状态**: 🚧 进行中

---

## 🎯 W2 验收标准

| 帧类型 | 方向 | 验证方法 |
|---|---|---|
| `session_state` | ↓ | Server 发送 → ESP32 收到, callback 触发 |
| `thinking` | ↓ | Server 发送 → ESP32 收到 |
| `assistant_text` | ↓ | Server 发送 → ESP32 收到 |
| `assistant_done` | ↓ | Server 发送 → ESP32 收到 |
| `tool_call` | ↓ | Server 发送 → ESP32 收到 |
| `tool_result` | ↓ | Server 发送 → ESP32 收到 |
| `tool_progress` | ↓ | Server 发送 → ESP32 收到 |
| `file_change` | ↓ | Server 发送 → ESP32 收到 |
| `authorization/requested` | ↓ | Server 发送 → ESP32 收到 |
| `authorization/resolved` | ↓ | Server 发送 → ESP32 收到 |
| `question/requested` | ↓ | Server 发送 → ESP32 收到 |
| `question/resolved` | ↓ | Server 发送 → ESP32 收到 |
| `file_upload` | ↓ | Server 发送 → ESP32 收到 |
| `system_event` | ↓ | Server 发送 → ESP32 收到 |
| `client/hello` | ↑ | ESP32 → Server, 启动后自动发 |
| `client/heartbeat` | ↑ | ESP32 → Server, 每 30s 自动发 |
| `client/tool_result` | ↑ | 收到 tool_call 后 ESP32 回 |
| `client/user_input` | ↑ | 用户输入触发 |

---

## 📋 实施步骤

### Step 1: 启动 Mock DSH Server
```bash
# 在 Mac 上 (192.168.1.55) 启动
cd /Volumes/ZT-1T/项目开发/ESP32-P4C5
pip3 install websockets  # 一次
python3 tools/mock_dsh_server.py --port 8765 --host 0.0.0.0
```

### Step 2: 修改 ESP32 DSH URL
```c
// sdkconfig.defaults 改:
CONFIG_P4C5_DSH_WEBSOCKET_URL="ws://192.168.1.55:8765/ws"
```

### Step 3: 烧录 + 跟踪 log
- 烧录后看 dsh_client 是否 connect ✅
- heartbeat 是否收到 `💓 wifi=✅ connected dsh=✅`
- mock server 端看 ESP32 是否发 `client/hello`, `client/heartbeat`

### Step 4: 触发各类型帧
- mock server 端手动发 14 类下行帧, 验证 ESP32 callback
- ESP32 端尝试发 `client/user_input`, 验证 mock server 收到

### Step 5: 记录 + commit

---

## 🛡️ 风险

| 风险 | 缓解 |
|---|---|
| Mac IP 变化 | 用 hostname `mac-mini.local` 或固件每次启动 log IP |
| mock server 没实现某些帧 | 看 mock_dsh_server.py 源码, 补全 |
| ESP32 lwIP DNS 失败 | ws://IP 不需要 DNS |
| 端口被占用 | 用其他端口如 8766, 同步改 URL |

---

## 📚 参考

- [W1-completion-summary.md](W1-completion-summary.md)
- [dsh-client-frame-protocol.md](dsh-client-frame-protocol.md)
- [tools/mock_dsh_server.py](../../tools/mock_dsh_server.py)