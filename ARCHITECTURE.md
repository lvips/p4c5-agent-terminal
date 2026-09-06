# p4c5-agent-terminal — 系统架构

> **版本**：v0.1.0（2026-09-06）
> **项目代号**：p4c5-agent-terminal
> **硬件平台**：ESP32-P4C5 酷世DIY 开发板
> **参考项目**：OMT（`/Volumes/ZT-1T/项目开发/OMT/`）

---

## 一、产品定位

**便携式 AI Agent 远程终端**——一个可揣进口袋的硬件遥控器，让个人 PC 用户能随时通过它调度本地 DSH（DeepSeek Harness）实例。
- 形态：4.3 寸触屏 + 4G 全网通 + 电池供电 + 4 麦 + 扬声器
- 用法：语音 / 文字 / 触摸三模输入，把请求转发到 DSH WebSocket
- **不变性**：产品定位 100% 与 OMT 相同，仅换了硬件平台

---

## 二、硬件架构

### 2.1 系统框图

```
┌────────────────────────────────────────────────────────────┐
│                    ESP32-P4 (主控)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  HP RISC-V   │  │  LP RISC-V   │  │  MIPI-DSI    │    │
│  │  400MHz × 2  │  │   40MHz × 1  │  │  ST7102 LCD  │    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  MIPI-CSI    │  │  I2S × 2     │  │  SDMMC 4-bit │    │
│  │  (SC2336)    │  │  ES8311/7210 │  │  MicroSD     │    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  I2C × 2     │  │  SPI × 3     │  │  UART × 4    │    │
│  │  触摸/IMU/PM │  │  Flash       │  │  4G/USB/调试 │    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
└────────────────────────┬───────────────────────────────────┘
                         │ esp_hosted (SDIO)
┌────────────────────────┴───────────────────────────────────┐
│                    ESP32-C5 (协处理器)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  WiFi 6      │  │  BLE 5.0     │  │  802.15.4    │    │
│  │  2.4+5 GHz   │  │              │  │  Zigbee/Thrd │    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
└────────────────────────────────────────────────────────────┘

┌─────────────────────┐  ┌────────────────────┐
│  AXP2101 (PMIC)     │  │  ML307C (4G 模组)  │
│  I2C 0x34           │  │  UART to P4         │
│  DCDC1 = 3.3V       │  │  TX/RX/PWR/DTR      │
│  ALDO1-4 可控       │  │  Cat.1 + GNSS       │
└─────────────────────┘  └────────────────────┘
```

### 2.2 关键 IC 清单

| IC | 型号 | I2C 地址 | 关键功能 |
|---|---|---|---|
| 主控 | ESP32-P4 (NRW32) | - | 400MHz 双核 RISC-V + 32MB PSRAM |
| 协处理器 | ESP32-C5 | - | WiFi 6 + BLE 5 + 802.15.4 |
| 屏幕 | ST7102 | - | 4.3寸 480×800 MIPI-DSI 2 lane |
| 触摸 | ST7123 | 0x5A | I2C 触摸 |
| DAC | ES8311 | 0x18 | 音频 DAC + 耳机放大 |
| ADC | ES7210 | 0x40 | 4 通道麦克风 ADC |
| PA | NS4150B | - | 3W D 类功放（EN=GPIO3）|
| 麦克风 | MSM381A3729H9CP ×4 | - | MEMS 差分模拟 |
| PMIC | AXP2101 | 0x34 | 11 路输出 + 库仑计 |
| 4G | ML307C-DC-CN | - | Cat.1 + GNSS（UART）|
| USB-UART | CH343P | - | USB CDC-ACM（OS 免驱）|
| IMU | LSM6DS3TR-C | 0x6A | 6 轴姿态（需实机确认型号）|
| DAC2 | MCP4725 | 0x60 | 12-bit DAC（预留）|
| LED | WS2812 ×4 | - | RGB LED（GPIO21）|

### 2.3 电压域

| 通道 | 电压 | 用途 | 控制 |
|---|---|---|---|
| DCDC1 | 3.3V | P4/C5/外设主电源 | AXP2101 持续输出 |
| ALDO1 | 1.8V | 辅助 | AXP2101 持续输出 |
| ALDO3 | 3.3V | 音频 codec | AXP2101 持续输出 |
| ALDO4 | 2.9V | 4G 模组 VBAT | AXP2101 可控 |
| VBUS | 5V | USB-C 输入 | 插入即有 |

---

## 三、软件架构

### 3.1 软件分层

```
┌──────────────────────────────────────────────────────┐
│  应用层 (Application)                                 │
│  - LVGL UI (4.3" 触屏)                                │
│  - 语音交互 (Wakenet + 4 麦 AEC)                       │
│  - WebSocket Client (连 DSH)                          │
│  - 4G 拨号 + WiFi failover                            │
└──────────────────┬───────────────────────────────────┘
                   │
┌──────────────────┴───────────────────────────────────┐
│  中间件 (Middleware)                                  │
│  - esp-ml307 v3.6.4 (4G 拨号 / TCP / MQTT / HTTP)    │
│  - esp_hosted v2.12+ (P4 ↔ C5 通信)                  │
│  - cpp_bus_driver (IMU)                               │
│  - LVGL 8.x                                          │
│  - 协议层 (dsh_client 14 类帧格式 — 复用 OMT)         │
└──────────────────┬───────────────────────────────────┘
                   │
┌──────────────────┴───────────────────────────────────┐
│  驱动层 (Drivers)                                     │
│  - ST7102 (MIPI-DSI LCD)                              │
│  - ST7123 (I2C 触摸)                                  │
│  - ES8311 (I2C DAC)                                   │
│  - ES7210 (I2C ADC)                                   │
│  - NS4150B (GPIO 功放使能)                            │
│  - AXP2101 (I2C PMIC, 14 个特殊寄存器)               │
│  - ML307C (UART 4G 模组)                              │
│  - CH343P (USB-UART 桥接)                             │
│  - LSM6DS3 (I2C IMU)                                  │
│  - WS2812 (RMT LED)                                   │
│  - SDMMC (MicroSD)                                    │
│  - MIPI-CSI (SC2336 摄像头) — 暂不实现                │
└──────────────────┬───────────────────────────────────┘
                   │
┌──────────────────┴───────────────────────────────────┐
│  OS / BSP                                             │
│  - ESP-IDF 5.5.5+ 或 6.2 (必须升级, OMT 用 5.4.2)     │
│  - FreeRTOS                                           │
│  - WiFi / BLE 协议栈                                  │
└──────────────────────────────────────────────────────┘
```

### 3.2 协议层（**100% 复用 OMT**）

**dsh_client 14 类帧格式**——和 OMT 完全相同，PC 端 Adapter 不变。

| 帧类型 | 方向 | 说明 |
|---|---|---|
| `FRAME_HELLO` | 设备 → DSH | 设备首次连接 |
| `FRAME_AUTH` | 设备 → DSH | 设备认证（JWT / API Key）|
| `FRAME_TEXT` | 双向 | 文本消息（用户输入 / AI 回复）|
| `FRAME_VOICE` | 双向 | 语音帧（Opus 编码）|
| `FRAME_IMAGE` | 设备 → DSH | 图片（截图 / 摄像头）|
| `FRAME_FILE` | 双向 | 文件（OTA / 资源）|
| `FRAME_CONTROL` | 设备 → DSH | 控制命令（重启 / 升级 / 调参）|
| `FRAME_STATUS` | 设备 → DSH | 设备状态（电量 / 信号 / CPU）|
| `FRAME_EVENT` | 设备 → DSH | 事件（按键 / 触摸 / 传感器触发）|
| `FRAME_ACK` | 双向 | 确认帧 |
| `FRAME_ERROR` | 双向 | 错误码 |
| `FRAME_PING` | 双向 | 心跳（30s）|
| `FRAME_PONG` | 双向 | 心跳响应 |
| `FRAME_BINARY` | 双向 | 任意二进制（透传）|

PC 端 Adapter：`hardware/tab5-adapter/` —— **OS-agnostic 100% 复用**。

### 3.3 应用层（**待 M2+ 实现**）

- LVGL UI 框架（参考 xiaozhi 板级代码）
- 语音交互（Wakenet 唤醒 + 4 麦 AEC + Opus 编码）
- WebSocket Client（基于 esp-ml307 或 esp_http_client）
- 4G / WiFi failover 逻辑

---

## 四、协作方法论（DSH + 双 CC）

### 4.1 角色定义

| 角色 | 工具 | 文件边界 | 工作性质 |
|---|---|---|---|
| **DSH** | DSH 主对话 | 所有（但不直接写代码）| 指挥 + 协调 + 跨域决策 |
| **CCA** | claude -p (manager) | `hardware/` + `docs/hw/` | 主力科研（代码 / 驱动 / 集成）|
| **CCB** | claude -p (assistant) | `docs/research/` + 文档归档 + commit + benchmark | 辅助科研（调研 / 文档 / commit 撰写）|

### 4.2 协作拓扑

```
        ┌──────────────┐
        │     DSH      │
        │  (本会话)    │
        └──────┬───────┘
               │ 派任务
        ┌──────┴───────┐
        │              │
   ┌────▼────┐    ┌────▼────┐
   │   CCA   │    │   CCB   │
   │ 主管    │    │ 助理    │
   │ claude  │    │ claude  │
   │  -p     │    │  -p     │
   └────┬────┘    └────┬────┘
        │              │
        ▼              ▼
   hardware/       docs/
   docs/hw/        research/
                   cc-orchestration/
```

### 4.3 实施计划

| 阶段 | 目标 | 状态 |
|---|---|---|
| **Day 0** | 仓库 + 目录结构 + 资料整理 + 首次 commit | ✅ |
| **M0.5** | CC 编排脚本 + 提示词模板 | ✅ |
| **M0-A** | 5 份 datasheets-summary 调研 | ✅（DSH 兜底产出）|
| **M1** | Cordis 插件 + DSH 集成 | ✅ |
| **M2** | HANDOFF 机制实测 | ⏳ |
| **M3** | 试运行 + 最终文档 | ⏳ |
| **M4** | 真机集成 + PoC | ⏳ |
| **M5** | 长稳测试 + 低功耗 | ⏳ |

### 4.4 红线

- 🚫 最大 2 个 `claude -p` 并发（OMT 红线 11）
- 🚫 不修改 OMT / macs 仓库
- 🚫 不修改 DSH 全局配置
- 🚫 大文件不入仓（用 LFS）
- 🚫 敏感信息不上 commit

---

## 五、与 OMT 项目的复用度

| 维度 | 复用度 | 说明 |
|---|---|---|
| 产品定位 | 100% | 完全相同 |
| 目标用户 | 100% | 个人 PC 爱好者 |
| MVP 验收清单 | 100% | 完全相同 |
| PC 端 Adapter | 100% | `hardware/tab5-adapter/` OS-agnostic |
| 通信协议 | 100% | dsh_client 14 类帧格式 |
| 协作方法论 | 100% | DSH + 双 CC |
| 硬件代码 | 30% | 屏/PMIC/4G/触摸 全换 |
| 工具链 | 90% | 协议层 + 集成层 |
| 资料完整度 | 优于 OMT | xiaozhi 完整参考工程 |

---

## 六、目录结构

```
p4c5-agent-terminal/
├── README.md                      # 项目总览
├── LICENSE                        # Apache 2.0
├── ARCHITECTURE.md                # 本文件
├── CHANGELOG.md                   # 变更日志
├── RELEASE_NOTES.md               # 发布说明
├── run-cordis-overlay.sh          # DSH 启动 + Cordis 加载
├── docs/
│   ├── README.md
│   ├── hw/                        # 硬件资料
│   │   ├── README.md
│   │   ├── p4c5-spec.md
│   │   ├── p4c5-pins.csv
│   │   ├── 00-新硬件差异矩阵.md
│   │   ├── 可信度评估报告-v1.md
│   │   ├── datasheets/            # 29 份 IC 规格书 PDF
│   │   ├── datasheets-summary/    # 5 份 DSH 调研报告
│   │   ├── schematic/             # 原理图
│   │   └── shell/                 # 3D 外壳
│   ├── cc-orchestration/
│   │   ├── README.md
│   │   ├── RUNBOOK.md
│   │   └── SUBAGENT-BEST-PRACTICES.md
│   ├── handovers/HANDOFF.md
│   └── research/                  # (M3+ 填充)
├── hardware/                      # (M1+ 填充)
└── .dsh-orchestration/
    ├── bin/                       # 6 个 shell 脚本
    ├── templates/                 # 3 份提示词
    ├── cordis-plugin/             # DSH 集成
    ├── state/  logs/  locks/      # 运行时（gitignore）
```

---

## 七、未来路线图

| 版本 | 目标 | 时间 |
|---|---|---|
| v0.1.0 | 仓库 + 资料 + 工具链建立 | ✅ 当前 |
| v0.2.0 | M2 HANDOFF 实测 + 第一次真机验证（仅供电）| 1 周内 |
| v0.3.0 | M4 PoC：屏幕点亮 + 4G 拨号 + 1 个传感器 | 2 周内 |
| v0.4.0 | M4 集成：LVGL UI + WebSocket Client | 3 周内 |
| v0.5.0 | M4 完成：语音交互 + 协议层 | 4 周内 |
| v1.0.0 | M5：长稳测试 + 低功耗 + 量产就绪 | 6-8 周内 |

---

**作者**：DSH
**版本**：v0.1.0
**下次更新**：M2 实测后