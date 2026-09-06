# M10 端到端验证报告

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **测试执行**：CCA T10 + CCB 文档整理
> **测试日期**：2026-09-06
> **固件 commit**：`0a69e41`（M9 集成）
> **配套文档**：`dsh-client-hands-on.md` + `M9-integration-test-spec.md` + `dsh-client-frame-protocol.md`

---

## 1. 测试目标

M10 里程碑验证 P4C5 终端从冷启动到 DSH 通信的**完整端到端链路**：

1. 6 步初始化序列（Board → PMIC → Display → Audio → 4G → dsh_client）正确执行
2. ML307C 4G 模组拨号 + 网络注册
3. WebSocket 连接到 Mock DSH 服务端
4. JSON 协议帧交互（hello → heartbeat → user_input → assistant_text → done）
5. 异常场景恢复（断连重连、心跳超时）

---

## 2. 测试环境

### 2.1 硬件配置

| 项 | 规格 | 状态 |
|---|---|---|
| 开发板 | ESP32-P4C5 酷世 DIY | ✅ 芯片 v1.3 |
| 供电 | USB-C 5V/2A | ✅ 必须 2A |
| SIM 卡 | ML307C Cat.1 | ⚠️ 需确认流量 |
| PC | macOS，运行 Mock 服务端 | ✅ |
| USB 串口 | ESP32-P4 USB | ✅ |

### 2.2 软件版本

| 项 | 版本 | commit |
|---|---|---|
| 固件 | p4c5-agent-terminal v0.1.0 | `0a69e41` |
| IDF | esp-idf v5.5.5 | |
| Mock 服务端 | mock_dsh_server.py v1.0 | `6b54b66` |
| websockets | ≥ 12.0 | |

### 2.3 网络拓扑

```
┌──────────────────┐     4G (Cat.1)     ┌──────────────────┐
│  P4C5 开发板      │ ◄═══════════════► │  运营商基站        │
│  - ML307C @ 921600│                    │                    │
│  - dsh_client    │                    └────────┬───────────┘
│  - ESP32-P4 v1.3 │                             │ Internet
└──────────────────┘                             │
                                                 │
                                        ┌────────▼───────────┐
                                        │  PC (Mock DSH)      │
                                        │  ws://0.0.0.0:8765  │
                                        │  mock_dsh_server.py │
                                        └─────────────────────┘
```

> **注**：如果 PC 无公网 IP，需内网穿透（frp/ngrok）或先用 WiFi 测试协议层。

---

## 3. 启动序列验证（6 步）

### 3.1 期望日志（基于 app_main.c 分析）

以下是基于源码分析的**期望完整启动日志**，CCA T10 实测后填入实际值。

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  期望启动日志（6 步初始化）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

I (100)  app_main: === p4c5-agent-terminal v0.1.0 ===
I (101)  app_main: [0/5] Board init (I2C)...
I (200)  p4c5_board: I2C0 init OK (SDA=7, SCL=8, 400kHz)
I (250)  app_main: [1/5] PMIC init (AXP2101)...
I (300)  p4c5_pmic: AXP2101 found at 0x34, chip_id=0x4A
I (350)  p4c5_pmic: 14 registers configured
I (351)  p4c5_pmic:   DCDC1=3300mV ALDO1=1800mV ALDO3=3300mV ALDO4=2900mV
I (400)  app_main: [2/5] Display init (ST7102 480x800)...
I (500)  p4c5_display: MIPI-DSI 2-lane init OK, backlight PWM on GPIO6
I (550)  app_main: [3/5] Audio init (ES8311+ES7210)...
I (600)  p4c5_audio: I2S0 init (MCLK=13, BCLK=12, WS=10, DOUT=9, DIN=11)
I (650)  p4c5_audio: ES8311 DAC @ 0x18 → OK
I (700)  p4c5_audio: ES7210 ADC @ 0x40 → OK
W (701)  p4c5_audio: Audio init failed (non-fatal): ESP_ERR_NO_MEM
I (750)  app_main: [4/5] 4G init (ML307C, baud=921600)...
I (800)  p4c5_4g: ML307C UART1 init (TX=53, RX=52, 921600)
I (850)  p4c5_4g: POWER_EN=HIGH (GPIO4)
I (900)  p4c5_4g: 📱 Modem detecting...
I (1200) p4c5_4g: 📱 Modem found: ML307C-CAF
I (1500) p4c5_4g: 📱 Registering network...
I (5000) p4c5_4g: 📱 Network ready! Carrier=CMCC, IMEI=86XXXXXXXXXXXXX
I (5001) app_main: [5/5] DSH client init...
I (5100) dsh_client: init OK (device_id=p4c5-001)
I (5101) app_main: Using ML307 4G transport
I (5200) dsh_client: connecting to ws://dsh.example.com/ws
W (5300) dsh_client: DSH connect failed (will retry): ESP_ERR_NOT_FOUND
I (5301) app_main: =========================================
I (5302) app_main:   All subsystems initialized
I (5303) app_main:   v0.1.0 ready
I (5304) app_main: =========================================
```

### 3.2 6 步初始化检查表

| 步骤 | 模块 | 期望结果 | 实测 | 状态 |
|---|---|---|---|---|
| [0/5] | Board (I2C) | I2C0 init OK, SDA=7, SCL=8 | 待填 | |
| [1/5] | PMIC (AXP2101) | 14 寄存器配置完成 | 待填 | |
| [2/5] | Display (ST7102) | MIPI-DSI 2-lane, 背光 PWM | 待填 | |
| [3/5] | Audio (ES8311+ES7210) | DAC/ADC 初始化（可能 non-fatal 失败）| 待填 | |
| [4/5] | 4G (ML307C) | 模组检测→网络注册→就绪 | 待填 | |
| [5/5] | dsh_client | init OK, connect（可能失败）| 待填 | |

### 3.3 关键时序

| 阶段 | 期望耗时 | 实测耗时 | 备注 |
|---|---|---|---|
| Board + PMIC | < 500ms | | I2C 枚举 |
| Display | < 300ms | | MIPI-DSI 配置 |
| Audio | < 300ms | | ES8311+ES7210 I2C |
| 4G 模组检测 | 2-5s | | AT→OK 响应 |
| 4G 网络注册 | 3-15s | | 取决于信号 |
| dsh_client 连接 | 1-3s | | WS handshake |
| **总计** | **< 20s** | | 冷启动到就绪 |

---

## 4. dsh_client 连接测试

### 4.1 Mock 服务端启动

```bash
python3 tools/mock_dsh_server.py --port 8765
```

**Mock 服务端预期输出**（连接成功后）：

```
==================================================
🚀 Mock DSH WebSocket Server
   地址: ws://0.0.0.0:8765/ws
   协议: P4C5 dsh_client v0.1.0
   下行帧: 14 类
   上行帧: 4 类
==================================================

00:01:23 [INFO] ✅ 新连接: ('10.99.0.5', 45678)
00:01:23 [INFO] 📥 收到: client/hello {'type': 'client/hello', 'device_id': 'p4c5-001'}
00:01:23 [INFO]   📋 Hello from p4c5-001 v0.1.0, caps=['audio', 'display', 'touch', '4g']
00:01:23 [INFO] 📤 发送: session_state (85 bytes)
00:01:23 [INFO] 📤 发送: session_state (87 bytes)
00:01:53 [INFO] 📥 收到: client/heartbeat {'type': 'client/heartbeat', 'battery': 87, 'rssi': -75}
00:01:53 [INFO]   💓 Heartbeat #1: battery=87%, rssi=-75dBm
```

### 4.2 Hello 握手帧验证

**上行（P4C5 → Mock）**：
```json
{
  "type": "client/hello",
  "device_id": "p4c5-001",
  "version": "0.1.0",
  "capabilities": ["audio", "display", "touch", "4g"]
}
```

**下行（Mock → P4C5）**：
```json
{"type": "session_state", "session_id": "xxxxxxxx", "state": "idle",
 "model": "mock-claude", "agent_name": "mock-dsh"}
{"type": "session_state", "session_id": "xxxxxxxx", "state": "running"}
```

| 验证项 | 期望 | 实测 | 状态 |
|---|---|---|---|
| client/hello 发送 | 连接成功后自动 | 待填 | |
| device_id 正确 | "p4c5-001" | 待填 | |
| session_state(idle) | 第一帧 | 待填 | |
| session_state(running) | 第二帧 | 待填 | |
| 串口日志 "🟢 DSH connected" | 出现 | 待填 | |

### 4.3 心跳帧验证

**上行（P4C5 → Mock，每 30s）**：
```json
{
  "type": "client/heartbeat",
  "timestamp": 1234567890,
  "battery": 87,
  "rssi": -75
}
```

| 验证项 | 期望 | 实测 | 状态 |
|---|---|---|---|
| 心跳间隔 | 30s ± 2s | 待填 | |
| battery 来源 | p4c5_pmic_get_battery_level() | 待填 | |
| rssi 来源 | p4c5_4g_get_rssi_dbm() | 待填 | |
| 连续 10 次不丢失 | 无断帧 | 待填 | |

### 4.4 对话流验证

**触发**：`dsh_client_send_user_input("你好")`

| 序号 | 帧类型 | 方向 | 期望内容 | 实测 |
|---|---|---|---|---|
| 1 | thinking | ↓ | content 包含分析文本 | 待填 |
| 2 | assistant_text | ↓ | chunk 1 | 待填 |
| 3 | assistant_text | ↓ | chunk 2 | 待填 |
| 4 | assistant_text | ↓ | chunk 3 | 待填 |
| 5 | assistant_done | ↓ | stop_reason="end_turn" | 待填 |

**P4C5 串口期望**：
```
I (...) app_main: 🤔 Thinking...
收到您的消息: "你好"。这是 Mock DSH 的自动回复。
I (...) app_main: 📝 Assistant done
```

---

## 5. 4G 状态

### 5.1 ML307C 模组状态

| 项 | AT 指令 | 期望值 | 实测 | 状态 |
|---|---|---|---|---|
| 模组型号 | AT+CGMM | ML307C | 待填 | |
| 固件版本 | AT+CGMR | ≥ v3.6.4 | 待填 | |
| SIM 就绪 | AT+CPIN? | +CPIN: READY | 待填 | |
| 信号质量 | AT+CSQ | CSQ ≥ 10 | 待填 | |
| 网络注册 | AT+CEREG? | +CEREG: 2,1 或 2,5 | 待填 | |
| PDP 激活 | AT+CGACT? | +CGACT: 1,1 | 待填 | |
| IP 地址 | AT+CGPADDR | 非 0.0.0.0 | 待填 | |
| 运营商 | AT+COPS? | CMCC/CUCC/CTCC | 待填 | |

### 5.2 信号质量对照

实测 CSQ：`_______`

| CSQ 范围 | dBm | 信号等级 | 拨号可用性 |
|---|---|---|---|
| 0-9 | < -109 | 极差 | ❌ 不可用 |
| 10-14 | -109~-95 | 差 | ⚠️ 勉强 |
| 15-19 | -95~-81 | 一般 | ✅ 可用 |
| 20-31 | > -81 | 良好/优 | ✅ 优秀 |

### 5.3 ALDO4 电压验证（R4 风险）

| 测量点 | 期望值 | 实测值 | 状态 |
|---|---|---|---|
| ALDO4 输出（万用表） | 2.9V | 待填 | |
| ML307C VBAT 输入 | ≥ 3.4V | 待填 | ⚠️ R4 |

> **R4 风险**：AXP2101 ALDO4 配置为 2900mV，但 ML307C 要求 VBAT ≥ 3.4V。
> 如果万用表测得 ALDO4 < 3.3V，需修改 AXP2101 寄存器 0x95 将 ALDO4 提升到 3.4V+。

---

## 6. 风险更新

### 6.1 已有风险状态（R1-R8，来自 M5 报告）

| 风险 | 描述 | M5 状态 | M10 更新 |
|---|---|---|---|
| R1 | I2C 总线冲突 | ✅ 排除 | 无变化 |
| R2 | AXP2101 寄存器配置 | ✅ 已验证 | 14 寄存器全部正确 |
| R3 | ES8311/ES7210 I2C 地址 | ✅ 已修正 | 预移位地址 0x30/0x80 |
| R4 | ALDO4=2.9V vs ML307C 3.4V | ⚠️ 待验证 | **需万用表实测** |
| R5 | 背光 PWM 频率 | ✅ 已配置 | GPIO6 PWM |
| R6 | 触摸 INT 引脚 | ✅ 已确认 | GPIO23 |
| R7 | IMU 中断未连接 | ✅ 已确认 | 轮询模式 |
| R8 | 芯片版本兼容性 | ✅ 已消除 | v1.3 CONFIG 正确 |

### 6.2 新增风险

#### R9：Transport 设置时序错误

**严重度**：🔴 高
**描述**：`app_main.c` 第 244-248 行注释明确指出——`set_transport` 必须在 `dsh_client_init()` 之前调用，但当前代码先调用 `dsh_client_init()` 再检测 transport，导致 ML307 4G transport 无法生效。

**代码位置**：`hardware/p4c5-agent-terminal/main/app_main.c`
```c
dsh_client_init(&dsh_cfg);          // ← 先 init 了
...
if (p4c5_4g_is_modem_detected()) {
    const dsh_transport_t *transport = p4c5_4g_get_transport();
    if (transport) {
        /* 注意：set_transport 必须在 init 之前调用。
         * 但这里已经 init 了。需要在下次重构时修正。
         * 当前先用默认 WS transport，connect 时会自动失败。 */
    }
}
err = dsh_client_connect();          // ← 用默认 WS transport，4G 下必失败
```

**影响**：4G 网络下 dsh_client_connect() 使用默认 WebSocket transport 而非 ML307 transport，连接必定失败。

**缓解**：重构 app_main.c，将 `p4c5_4g_get_transport()` 调用移到 `dsh_client_init()` 之前：
```c
// 正确顺序：
// 1. 检测 4G 模组
// 2. 获取 transport
// 3. dsh_client_init(&cfg)
// 4. dsh_client_set_transport(transport)  ← 必须在 connect 前
// 5. dsh_client_connect()
```

#### R10：DSH URL 硬编码

**严重度**：🟡 中
**描述**：`app_main.c` 第 230 行 dsh_client URL 硬编码为 `"ws://dsh.example.com/ws"`，未从 `menuconfig` 读取。

**代码位置**：`hardware/p4c5-agent-terminal/main/app_main.c`
```c
dsh_client_config_t dsh_cfg = {
    .url = "ws://dsh.example.com/ws",   /* TODO: 从 menuconfig 读取 */
    .device_id = "p4c5-001",
    .auth_token = NULL,
};
```

**影响**：每次联调需手动修改代码中的 URL，不利于多环境切换（开发/测试/生产）。

**缓解**：
1. **短期**：联调时手动修改为 Mock 服务端 IP
2. **中期**：实现 `idf.py menuconfig` → `P4C5_DSH_WS_URL` 配置项
3. **长期**：支持从 4G 模组短信 / BLE 配网获取 URL

---

## 7. 实测日志摘录

> **注**：以下为 CCA T10 实测时应填入的真实串口日志摘录。
> CCB 基于源码分析提供了期望模式，实际测试后替换。

### 摘录 1：冷启动（Board + PMIC）

```
[待 CCA T10 填入真实日志]

期望模式：
I (100)  app_main: === p4c5-agent-terminal v0.1.0 ===
I (101)  app_main: [0/5] Board init (I2C)...
I (200)  p4c5_board: I2C0 init OK (SDA=7, SCL=8, 400kHz)
I (250)  app_main: [1/5] PMIC init (AXP2101)...
I (300)  p4c5_pmic: AXP2101 found at 0x34, chip_id=0x4A
```

### 摘录 2：Display + Audio

```
[待 CCA T10 填入真实日志]

期望模式：
I (400)  app_main: [2/5] Display init (ST7102 480x800)...
I (500)  p4c5_display: MIPI-DSI 2-lane init OK
I (550)  app_main: [3/5] Audio init (ES8311+ES7210)...
W (701)  p4c5_audio: Audio init failed (non-fatal): ESP_ERR_NO_MEM
```

### 摘录 3：4G 模组注册

```
[待 CCA T10 填入真实日志]

期望模式：
I (750)  app_main: [4/5] 4G init (ML307C, baud=921600)...
I (900)  p4c5_4g: 📱 Modem detecting...
I (1200) p4c5_4g: 📱 Modem found: ML307C-CAF
I (1500) p4c5_4g: 📱 Registering network...
I (5000) p4c5_4g: 📱 Network ready! Carrier=CMCC, IMEI=86XXXXXXXXXXXXX
```

### 摘录 4：dsh_client 连接

```
[待 CCA T10 填入真实日志]

期望模式：
I (5001) app_main: [5/5] DSH client init...
I (5100) dsh_client: init OK (device_id=p4c5-001)
I (5200) dsh_client: connecting to ws://192.168.1.100:8765/ws
I (5300) dsh_client: 🟢 DSH connected
I (5301) dsh_client: 📡 DSH session: idle
I (5302) dsh_client: 📡 DSH session: running
```

### 摘录 5：主循环心跳

```
[待 CCA T10 填入真实日志]

期望模式：
I (15000) app_main: 💓 bat=87% csq=18 rssi=-75dBm dsh=✅ 4g=✅
I (25000) app_main: 💓 bat=87% csq=17 rssi=-76dBm dsh=✅ 4g=✅
I (35000) app_main: 💓 bat=86% csq=18 rssi=-75dBm dsh=✅ 4g=✅
```

### 摘录 6：断连重连（如有测试）

```
[待 CCA T10 填入真实日志]

期望模式：
W (95000) dsh_client: 🔴 DSH disconnected
W (95100) dsh_client: 🔄 DSH reconnecting...
I (105000) dsh_client: 🟢 DSH connected
```

---

## 8. 测试结果汇总

### 8.1 TC 通过率

| 类别 | TC 编号 | 总数 | 通过 | 失败 | 未测 |
|---|---|---|---|---|---|
| 4G 拨号 | TC-M9-01~06 | 6 | | | |
| dsh_client 连接 | TC-M9-07~10 | 4 | | | |
| 业务帧 | TC-M9-11~14 | 4 | | | |
| 流量统计 | TC-M9-15 | 1 | | | |
| **总计** | | **15** | | | |

### 8.2 验收标准

| 等级 | 要求 | 结果 |
|---|---|---|
| P0 必须 | TC-M9-01~08, TC-M9-11 | 待填 |
| P1 建议 | TC-M9-09~10, TC-M9-12~14 | 待填 |
| P2 可选 | TC-M9-15 | 待填 |

---

## 9. 结论与下一步

### 9.1 M10 结论

| 项 | 状态 | 说明 |
|---|---|---|
| 6 步初始化 | 待测 | 代码审查 OK，需实测 |
| 4G 拨号 | 待测 | R4 ALDO4 风险需万用表 |
| dsh_client 连接 | 🔴 | R9 transport 时序 bug 阻塞 |
| 协议帧交互 | 待测 | 依赖连接成功 |
| 风险状态 | R9🔴 R10🟡 | 需修复后重测 |

### 9.2 阻塞项

1. **R9（transport 时序）**：必须修复 app_main.c，否则 4G 下 dsh_client 无法连接
2. **R4（ALDO4 电压）**：万用表实测后决定是否需要修改 PMIC 寄存器

### 9.3 M11 计划

| 优先级 | 任务 | 负责人 |
|---|---|---|
| P0 | 修复 R9：重构 app_main.c transport 设置顺序 | CCA T11 |
| P0 | R4 万用表验证 + ALDO4 电压调整 | CCA T11 |
| P1 | 实现 R10：menuconfig DSH URL 配置 | CCA T11 |
| P1 | M10 全部 TC 实测 + 填入本报告 | CCA T11 |
| P2 | TC-M9-15 1h 流量统计 | CCA T11 |

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：CCA T10/T11 实测完成后，补充真实日志 + TC 结果
