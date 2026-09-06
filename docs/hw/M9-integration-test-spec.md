# M9 集成测试规范

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA T9（4G + dsh_client 端到端验证）
> **配套文档**：`dsh-client-frame-protocol.md` + `dsh-client-test-spec.md` + `4g-flow-guide.md`
> **Mock 服务端**：`tools/mock_dsh_server.py`

---

## 1. 测试目标

验证 P4C5 终端 **4G 模组 (ML307C) + dsh_client 协议层** 的端到端集成：

1. **4G 拨号 → WebSocket 连接 → 协议帧交互** 完整链路
2. **Mock 服务端** 替代真实 DSH Adapter，覆盖正常 + 异常场景
3. **4G 流量消耗** 实测统计（参考 CCB T4 流量指南）
4. **WiFi↔4G 切换** 下 dsh_client 重连稳定性

**前置条件**：
- M8 dsh_client 协议层已就绪（commit `b39f4d5`）
- M9 CCA T9 4G 模组实现完成
- SIM 卡已开通数据流量

---

## 2. 测试环境

### 2.1 硬件

| 项 | 规格 | 备注 |
|---|---|---|
| 开发板 | ESP32-P4C5 酷世 DIY | v1.3 芯片 |
| 供电 | USB-C 5V/2A | **必须 2A**（4G 发射峰值） |
| SIM 卡 | ML307C Cat.1 | 已开通定向流量包 |
| PC | 运行 Mock 服务端 | 需与 4G 网络可达 |

### 2.2 软件

| 项 | 版本 | 说明 |
|---|---|---|
| 固件 | p4c5-agent-terminal (M9) | CCA T9 构建 |
| IDF | ≥ 5.5.5 | |
| Mock 服务 | `tools/mock_dsh_server.py` | Python websockets |
| 依赖 | `pip install websockets` | |

### 2.3 网络拓扑

```
┌──────────────────┐     4G (Cat.1)     ┌──────────────────┐
│  P4C5 开发板      │ ◄═══════════════► │  运营商基站        │
│  - ML307C 模组    │                    │                    │
│  - dsh_client    │                    └────────┬───────────┘
│  - ESP32-P4 v1.3 │                             │ Internet
└──────────────────┘                             │
                                                 │
                                        ┌────────▼───────────┐
                                        │  PC (Mock 服务端)   │
                                        │  :8765/ws           │
                                        │  mock_dsh_server.py │
                                        └─────────────────────┘
```

> **注意**：4G 网络 NAT 穿透问题。如果 PC 不可直接访问，需要：
> 1. 使用公网 IP / 端口转发
> 2. 或使用内网穿透工具（frp / ngrok）
> 3. 或先用 WiFi 测试（PC 和 P4C5 在同一局域网），再用 4G 测试拨号

---

## 3. Mock 服务端使用

### 3.1 启动

```bash
# 安装依赖
pip install websockets

# 基本启动
python3 tools/mock_dsh_server.py --port 8765

# 详细日志
python3 tools/mock_dsh_server.py --port 8765 --verbose

# 测试大帧（多包拼接）
python3 tools/mock_dsh_server.py --port 8765 --large-frame

# 测试重连（3 次心跳后断连）
python3 tools/mock_dsh_server.py --port 8765 --disconnect-after 3
```

### 3.2 交互流程

```
P4C5 固件                          Mock 服务端
    │                                   │
    │── WS Connect ────────────────────→│
    │                                   │  ✅ 新连接
    │                                   │
    │── client/hello ──────────────────→│  📋 Hello from p4c5-001
    │                                   │
    │←── session_state (idle) ─────────│
    │←── session_state (running) ──────│
    │                                   │
    │── client/heartbeat ──────────────→│  💓 battery=87%, rssi=-75dBm
    │── client/heartbeat ──────────────→│  💓 (30s later)
    │                                   │
    │── client/user_input ─────────────→│  💬 "你好"
    │                                   │
    │←── thinking ─────────────────────│  🤔 分析中...
    │←── assistant_text (chunk 1) ─────│
    │←── assistant_text (chunk 2) ─────│
    │←── assistant_text (chunk 3) ─────│
    │←── assistant_done ───────────────│
    │                                   │
    │←── tool_call ────────────────────│  🔧 (每 3 次触发)
    │── client/tool_result ────────────→│  🔧 执行完成
    │                                   │
```

---

## 4. 集成测试用例

### 4.1 4G 拨号序列（TC-M9-01 ~ TC-M9-06）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-M9-01 | ML307C 上电 | 1. `p4c5_4g_init()` 2. 观察串口 | POWER_EN=HIGH, UART 初始化, AT→OK | | |
| TC-M9-02 | SIM 卡就绪 | AT+CPIN? | `+CPIN: READY` | | |
| TC-M9-03 | 信号强度 | AT+CSQ | CSQ ≥ 10（≥-81dBm）| | |
| TC-M9-04 | 网络注册 | AT+CEREG? | `+CEREG: 2,1` 或 `+CEREG: 2,5` | | |
| TC-M9-05 | PDP 激活 | AT+CGACT=1,1 | `OK` | | |
| TC-M9-06 | 获取 IP | AT+CGPADDR | IP 非 0.0.0.0 | | |

**TC-M9-01 详细检查**：
- [ ] ALDO4 输出电压（万用表）：`_______ V`（⚠️ R4: 期望 2.9V）
- [ ] ML307C 功耗（上电瞬间）：`_______ mA`
- [ ] UART 波特率：115200（config.h 默认）
- [ ] AT 响应时间：< 2s

**TC-M9-03 信号质量**：

| CSQ | dBm | 信号等级 | 是否可拨号 |
|---|---|---|---|
| 0-9 | < -109 | 极差 | ❌ |
| 10-14 | -109 ~ -95 | 差 | ⚠️ 勉强 |
| 15-19 | -95 ~ -81 | 一般 | ✅ |
| 20-31 | > -81 | 良好/优秀 | ✅ |

实测 CSQ：`_______`（对应 `_______ dBm`）

### 4.2 dsh_client 连接（TC-M9-07 ~ TC-M9-10）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-M9-07 | WS 连接到 Mock | `dsh_client_connect()` | 状态 CONNECTED，日志 "WebSocket 已连接" | | |
| TC-M9-08 | Hello 握手 | 自动发送 client/hello | 收到 session_state(idle→running) | | |
| TC-M9-09 | 心跳维持 | 观察 3 次心跳 | 每 30s 发送 client/heartbeat，含 battery+rssi | | |
| TC-M9-10 | Mock 断连重连 | `--disconnect-after 3` | 3 次心跳后 Mock 断开 → dsh_client 自动重连 | | |

**TC-M9-07 网络配置**：
- Mock 服务端 IP：`_______________`
- Mock 服务端端口：`_______________`（默认 8765）
- P4C5 URL：`ws://_______________:8765/ws`
- dsh_client_init 返回值：`_______________`

**TC-M9-08 Hello 帧验证**：
```
期望上行:
{"type":"client/hello",
 "device_id":"p4c5-001",
 "version":"0.1.0",
 "capabilities":["audio","display","touch","4g"]}

期望下行:
{"type":"session_state","session_id":"...","state":"idle","model":"mock-claude","agent_name":"mock-dsh"}
{"type":"session_state","session_id":"...","state":"running"}
```

### 4.3 业务帧交互（TC-M9-11 ~ TC-M9-14）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-M9-11 | 用户输入→回复 | `dsh_client_send_user_input("你好")` | 收到 thinking→assistant_text→assistant_done | | |
| TC-M9-12 | 工具调用 | Mock 发送 tool_call | 客户端回调触发，event_bus 推送 | | |
| TC-M9-13 | 工具结果回复 | `dsh_client_send_tool_result(id, ...)` | Mock 收到 client/tool_result | | |
| TC-M9-14 | 大帧多包拼接 | Mock `--large-frame` | >4096B JSON 正确拼接解析 | | |

**TC-M9-11 完整时序**：
```
T=0.0s  P4C5 → {"type":"client/user_input","text":"你好"}
T=0.3s  P4C5 ← {"type":"thinking","content":"让我分析一下..."}
T=0.5s  P4C5 ← {"type":"assistant_text","content":"收到您..."}
T=0.7s  P4C5 ← {"type":"assistant_text","content":"的消息..."}
T=0.9s  P4C5 ← {"type":"assistant_text","content":"...自动回复"}
T=1.0s  P4C5 ← {"type":"assistant_done","stop_reason":"end_turn","usage":{...}}
```

- [ ] 全部 5 个下行帧按序收到
- [ ] assistant_text 内容拼接完整
- [ ] assistant_done 包含 usage 统计

**TC-M9-14 大帧测试**：
- Mock 发送 8KB+ JSON
- 客户端多包拼接 buffer 扩容到 16KB+
- `payload_offset` 连续性正确
- JSON 解析成功，content 完整（8000+ 字符）

### 4.4 4G 流量实测（TC-M9-15）

| TC | 测试项 | 测试步骤 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|---|
| TC-M9-15 | 1h 流量统计 | 运行 1h，每 5 分钟记录 | 总流量 < 10 MB（纯文字场景）| | |

**流量记录表**：

| 时间 | 上传 bytes | 下载 bytes | CSQ | 延迟 ms | dsh 状态 |
|---|---|---|---|---|---|
| 00:00 | | | | | CONNECTED |
| 00:05 | | | | | |
| 00:10 | | | | | |
| 00:15 | | | | | |
| 00:20 | | | | | |
| 00:25 | | | | | |
| 00:30 | | | | | |
| 00:35 | | | | | |
| 00:40 | | | | | |
| 00:45 | | | | | |
| 00:50 | | | | | |
| 00:55 | | | | | |
| 01:00 | | | | | |

**流量汇总**：
- 总上传：`_______ bytes`（`_______ KB`）
- 总下载：`_______ bytes`（`_______ KB`）
- 总流量：`_______ KB`
- 平均 CSQ：`_______`
- 最小 CSQ：`_______`
- 重连次数：`_______`
- 心跳发送次数：`_______`（期望 120 = 60min / 30s × 2 方向）

**流量预估对比**（参考 CCB T4）：

| 场景 | CCB T4 预估 | M9 实测 | 偏差 |
|---|---|---|---|
| 心跳 (30s) | ~144 B/h | _______ B/h | |
| 文字交互 | ~144 KB/h | _______ KB/h | |
| 总流量/h | ~5-10 MB/h | _______ MB/h | |

---

## 5. 异常场景测试

### 5.1 网络异常

| 场景 | Mock 操作 | 期望 P4C5 行为 |
|---|---|---|
| 服务端主动关闭 | `--disconnect-after 3` | 检测到断连 → 自动重连 → 重新 hello |
| 心跳超时 | Mock 不回复（模拟） | WS pong 超时 → 断开 → 重连 |
| DNS 解析失败 | URL 指向不存在域名 | 连接失败 → 重连（指数退避）|
| 网络不可达 | 拔出 SIM 卡 | AT 指令失败 → 状态 ERROR |

### 5.2 协议异常

| 场景 | Mock 操作 | 期望 P4C5 行为 |
|---|---|---|
| 畸形 JSON | 发送 `{invalid` | "JSON 解析失败"，不崩溃 |
| 未知帧类型 | 发送 `{"type":"unknown"}` | "未知帧类型"，推送 system_event |
| 超大帧 | `--large-frame` | 多包拼接成功 |
| 空帧 | 发送 `{}` | 正常处理（无 type 字段）|

### 5.3 4G 异常

| 场景 | 操作 | 期望 P4C5 行为 |
|---|---|---|
| SIM 卡未插入 | 不插 SIM | AT+CPIN? → ERROR → 日志告警 |
| 信号丢失 | 屏蔽天线 | CSQ=0 → 重连失败 → ERROR |
| 流量耗尽 | 欠费停机 | AT+CGACT 失败 → 状态 ERROR |
| ALDO4 电压不足 | R4 风险触发 | ML307C 重启/不响应 → 上电序列重试 |

---

## 6. 验收标准

### 6.1 必须通过（P0）

| TC | 要求 |
|---|---|
| TC-M9-01~06 | ✅ 4G 拨号全通过 |
| TC-M9-07 | ✅ WS 连接到 Mock |
| TC-M9-08 | ✅ Hello 握手成功 |
| TC-M9-11 | ✅ 完整对话流 |

### 6.2 建议通过（P1）

| TC | 要求 |
|---|---|
| TC-M9-09~10 | ✅ 心跳 + 重连 |
| TC-M9-12~13 | ✅ 工具调用流 |
| TC-M9-14 | ✅ 大帧拼接 |

### 6.3 可选通过（P2）

| TC | 要求 |
|---|---|
| TC-M9-15 | ✅ 1h 流量统计（可延后到 M10）|

---

## 7. 风险标注

| 风险 | 描述 | 关联 TC | 缓解 |
|---|---|---|---|
| R4 | ALDO4=2.9V vs ML307C 3.4V min | TC-M9-01 | 万用表实测，不足则修改 0x95 |
| R-4G-1 | 4G NAT 穿透（Mock 不可达）| TC-M9-07 | frp/ngrok 内网穿透 |
| R-4G-2 | 4G 流量费用超预期 | TC-M9-15 | 定向流量包 + 流量监控 |
| R-DSH-1 | 大帧截断（>4096B）| TC-M9-14 | 多包拼接（OMT v43 修复）|
| R-DSH-2 | 心跳任务栈溢出 | TC-M9-09 | 独立任务栈（P4C5 已设计）|

---

## 8. 测试报告模板

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  M9 集成测试报告
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
测试日期：____________
固件 commit：____________
Mock 版本：____________
SIM 卡号：____________

━━ 4G 拨号 ━━
  ML307C 上电：[ ] ✅ / [ ] ❌
  SIM 就绪：[ ] ✅ / [ ] ❌
  信号 CSQ：_______
  PDP 激活：[ ] ✅ / [ ] ❌
  IP 获取：_______________

━━ dsh_client 连接 ━━
  WS 连接：[ ] ✅ / [ ] ❌
  Hello 握手：[ ] ✅ / [ ] ❌
  心跳正常：[ ] ✅ / [ ] ❌
  重连测试：[ ] ✅ / [ ] ❌

━━ 业务帧 ━━
  对话流：[ ] ✅ / [ ] ❌
  工具调用：[ ] ✅ / [ ] ❌
  大帧拼接：[ ] ✅ / [ ] ❌

━━ 流量统计 (1h) ━━
  上传：_______ KB
  下载：_______ KB
  总计：_______ KB
  重连：_______ 次

━━ 结论 ━━
  _______________
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：CCA T9 实测完成后，补充数据
