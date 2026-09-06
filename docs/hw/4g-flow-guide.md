# 4G 流量监控指南与实测报告模板

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA（M1 真机验证）+ DSH（流量费用决策）
> **配套文档**：`ml307c.md` + `可信度评估报告-v1.md` §2.2 + `pmic-registers-audit.md`

---

## 1. 4G 流量预估

DSH Agent 通信场景下，4G 流量消耗主要来自 WebSocket 长连接 + 语音/文字数据包：

| 通信类型 | 单次数据量 | 频率 | 每小时 | 24h |
|---|---|---|---|---|
| 语音 Opus 帧 | ~1 KB（20ms @ 24kbps）| 50 帧/s（双向）| ~360 MB | ~8.6 GB |
| **纯文字交互** | ~200 B（JSON）| 每 5s 一次 | ~144 KB | ~3.5 MB |
| **心跳保活** | ~50 B | 每 30s 一次 | ~6 KB | ~144 B |
| **TCP 重传/ACK** | ~40 B/packet | 随数据 | 随数据 | 随数据 |
| **实际混合场景** | — | 语音 30% + 文字 70% | **~5-10 MB** | **~120-240 MB** |

> **结论**：纯文字 Agent 场景（无语音）约 5-10 MB/h，持续 24h 约 120-240 MB。
> **建议**：申请 **≥ 1 GB/月** 定向流量包（含语音场景则 ≥ 5 GB/月）。

⚠️ **R7 风险**：不包流量会产生天价账单。必须用定向流量包（5-10 元/月）。

---

## 2. 拨号流程（5 步）

参考 ML307C 拨号上网用户手册（V1.0.1）+ TCP/IP 用户手册（V5.1.6）+ xiaozhi `Ml307Board::NetworkTask()`。

### 2.1 裸 AT 指令拨号（调试用）

```
步骤 1: AT                              → OK                  # 测试模组在线
步骤 2: AT+CPIN?                        → +CPIN: READY        # SIM 卡就绪
步骤 3: AT+CSQ                          → +CSQ: 20,0          # 信号质量（0-31，≥10 可拨号）
步骤 4: AT+CEREG?                       → +CEREG: 2,1         # EPS 网络注册（1=本机, 5=漫游）
步骤 5: AT+CGDCONT=1,"IP","CMNET"       → OK                  # APN（中国移动）
步骤 6: AT+CGACT=1,1                    → OK                  # 激活 PDP 上下文
步骤 7: AT+CGPADDR                      → +CGPADDR: 1,"10.x.x.x"  # 获取 IP 地址
```

### 2.2 esp-ml307 高层拨号（生产用）

```cpp
// xiaozhi 方式（p4c5_4g.cc 参考）
auto modem = AtModem::Detect(tx_pin, rx_pin, dtr_pin, 921600);
auto status = modem->WaitForNetworkReady(30000);  // 30s 超时
if (status == NetworkStatus::Ready) {
    // 拨号成功，可建立 TCP/WebSocket 连接
    auto tcp = modem->CreateTcp(0);
    tcp->Connect("ds-host", 8443);
}
```

> **注意**：esp-ml307 自动处理 APN/CGDCONT/MIPCALL，无需手动发 AT 指令。
> 波特率 921600（xiaozhi 默认），非 115200（CCA 当前配置需修正）。

### 2.3 CSQ 与信号强度换算

| CSQ 值 | dBm | 信号等级 | 拨号可用性 |
|---|---|---|---|
| 0 | ≤ -113 | 无信号 | ❌ 不可拨号 |
| 1-9 | -111 ~ -93 | 极弱 | ⚠️ 可能超时 |
| 10-14 | -91 ~ -83 | 弱 | ✅ 可拨号（延迟高）|
| 15-19 | -81 ~ -73 | 中等 | ✅ 正常 |
| 20-31 | -71 ~ -51 | 强 | ✅ 最佳 |

---

## 2.5 APN 运营商配置参考

| 运营商 | APN | 备注 |
|---|---|---|
| 中国移动 | `CMNET` | 默认，推荐 |
| 中国联通 | `3GNET` | 部分地区需手动设置 |
| 中国电信 | `CTNET` | 需 CTAUTH 认证（用户名/密码留空）|
| 物联网卡 | 卡商指定 | 常见：`CMIOT`、`UNIM2M.NJM2MAPN` 等 |

> **提示**：如 `CMNET` 拨号失败（`+CME ERROR`），尝试 `AT+CGDCONT=1,"IP","CMWAP"`。

---

## 3. 流量监控方案

### 3.1 应用层流量计数（推荐）

esp-ml307 **无内置流量统计 API**，需在应用层跟踪：

```c
// p4c5_4g_flow.h — 流量监控（应用层实现）
typedef struct {
    uint64_t total_sent;       // 累计发送字节
    uint64_t total_recv;       // 累计接收字节
    uint32_t tcp_send_count;   // TCP 发送次数
    uint32_t tcp_recv_count;   // TCP 接收次数
    uint32_t reconnect_count;  // 重连次数
    uint32_t last_report_ts;   // 上次报告时间戳（秒）
} p4c5_4g_flow_stats_t;

// 每次 tcp_send 后调用
void p4c5_4g_flow_add_sent(int bytes);
// 每次 tcp_recv 后调用
void p4c5_4g_flow_add_recv(int bytes);
// 每分钟写一行日志
void p4c5_4g_flow_log(void);
// 查询统计
void p4c5_4g_flow_get(p4c5_4g_flow_stats_t* stats);
// 重置统计
void p4c5_4g_flow_reset(void);
```

### 3.2 AT 指令辅助查询

| AT 指令 | 用途 | 频率 |
|---|---|---|
| `AT+CSQ` | 信号强度 | 每 60s |
| `AT+MIPSTATE?` | TCP 连接状态 | 每 60s |
| `AT+CEREG?` | 网络注册状态 | 每 300s |
| `AT+CGPADDR` | IP 地址 | 拨号后一次 |
| `AT+CGSN` | 模组 IMEI | 启动时一次 |
| `AT+CIMI` | SIM IMSI | 启动时一次 |

### 3.3 日志格式

每分钟输出一行，格式固定，便于 PC 端解析：

```
[4G_FLOW] ts=1693987200 sent=1024 recv=2048 total_sent=61440 total_recv=122880 csq=20 ip=10.168.1.1 state=CONNECTED
```

字段说明：
- `ts`：Unix 时间戳
- `sent/recv`：本周期增量（字节）
- `total_sent/total_recv`：累计（字节）
- `csq`：当前信号质量
- `ip`：拨号 IP 地址
- `state`：TCP 连接状态（CONNECTED / DISCONNECTED / CONNECTING）

---

## 4. 实测报告模板（M1 真机验证）

> **以下内容在 M1 真机验证时填写**

### 4.1 基本信息

| 项 | 值 |
|---|---|
| 测试日期 | ____-__-__ |
| 测试时间 | __:__ ~ __:__ |
| 测试环境 | □ 室内（办公室） □ 室内（地下室） □ 室外 □ 窗边 |
| SIM 卡运营商 | □ 中国移动 □ 中国联通 □ 中国电信 |
| SIM 卡类型 | □ 定向流量包 □ 通用流量包 □ 物联网卡 |
| 流量包月额度 | ____ GB/月 |
| 模组固件版本 | ________________（AT+CGMR）|
| IMEI | ________________（AT+CGSN）|
| ICCID | ________________（AT+CCID）|

### 4.2 拨号测试（10 次）

| 序号 | AT+CPIN? | AT+CSQ | AT+CEREG? | AT+CGDCONT | 拨号结果 | 获取 IP | 耗时(s) |
|---|---|---|---|---|---|---|---|
| 1 | READY | | | OK | | | |
| 2 | READY | | | OK | | | |
| 3 | READY | | | OK | | | |
| 4 | READY | | | OK | | | |
| 5 | READY | | | OK | | | |
| 6 | READY | | | OK | | | |
| 7 | READY | | | OK | | | |
| 8 | READY | | | OK | | | |
| 9 | READY | | | OK | | | |
| 10 | READY | | | OK | | | |
| **成功率** | /10 | CSQ 中位数= | /10 | /10 | /10 | /10 | **中位数=** __s |

**通过标准**：拨号成功率 ≥ 8/10，平均耗时 < 30s，CSQ ≥ 10。

### 4.3 网络延迟（ping 测试）

| 目标 | 协议 | 端口 | 发送包数 | 成功 | 失败 | 最小(ms) | 最大(ms) | 中位数(ms) |
|---|---|---|---|---|---|---|---|---|
| DSH WebSocket 服务器 | TCP | 8443 | 100 | | | | | |
| baidu.com | HTTP | 80 | 100 | | | | | |
| 8.8.8.8 | ICMP | — | 100 | | | | | |

**通过标准**：DSH WebSocket 延迟中位数 < 500ms，丢包率 < 5%。

### 4.4 流量统计（24h 长稳）

| 时间点 | 周期发送(KB) | 周期接收(KB) | 累计发送(KB) | 累计接收(KB) | 备注 |
|---|---|---|---|---|---|
| 0h（启动）| 0 | 0 | 0 | 0 | 拨号成功 |
| 1h | | | | | 持续连接 + 偶尔发消息 |
| 2h | | | | | |
| 4h | | | | | |
| 6h | | | | | |
| 8h | | | | | |
| 12h | | | | | |
| 18h | | | | | |
| 24h | | | | | |
| **总计** | | | | | |

**测试条件**：
- WebSocket 保持连接
- 心跳间隔 60s
- 每 5 分钟发送一条测试文字（~200B）
- 无语音传输

**通过标准**：
- 24h 总流量 < 500 MB
- TCP 连接保持率 100%（0 次断连）
- 心跳成功率 ≥ 99%

#### 长稳异常处理

| 异常 | 处理 |
|---|---|
| TCP 断连 1 次 | 记录原因（`+MIPURC: "closed"`），自动重连 |
| TCP 断连 > 3 次 | 停止测试，检查信号 + 流量包余量 |
| 模组无 AT 响应 | 通过 PWR_EN 重启模组，记录重启次数 |
| 流量飙升 > 1MB/min | 可能存在重传风暴，检查网络状况 |

### 4.5 R4 电压验证

| 测试点 | 拨号前(V) | 拨号中(V) | 发射峰值(V) | 最低(V) |
|---|---|---|---|---|
| ALDO4 输出 | | | | |
| ML307C VBAT | | | | |

**通过标准**：发射峰值时 ALDO4 ≥ 2.7V。若 < 2.7V → 需改 AXP2101 ALDO4 为 3.4V。

**实测方法**：
1. 万用表直流电压档，红表笔接 ALDO4 测试点，黑表笔接 GND
2. 触发 4G 发射（发一条长消息或 ping 大包）
3. 观察万用表电压最低值

---

## 5. 故障排查

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| `AT` 无响应 | 模组未上电 / 波特率错 | 检查 PWR_EN(GPIO4)=HIGH；波特率应为 921600 |
| `+CPIN: NOT INSERTED` | SIM 卡未插好 | 重新插拔 Nano SIM，确认缺口方向 |
| `+CME ERROR: 10` | SIM 卡 PIN 锁 | 用手机取消 PIN 码，或 `AT+CPIN="",""` |
| `+CSQ: 0,0` | 无信号 | 移到窗边 / 检查天线焊接 / 加外置 SMA 天线 |
| `+CEREG: 2,3` | 网络注册被拒 | SIM 卡欠费 / 运营商限制 / 换 APN |
| 拨号成功但 TCP 连不上 | APN 错误或 DNS 失败 | 试 `CMWAP` 代替 `CMNET`；手动 DNS `8.8.8.8` |
| TCP 频繁断连 | 信号弱 / 心跳超时 | 增大心跳间隔到 120s；检查 CSQ |
| 流量异常大 | 心跳过频 / 重传风暴 | 调整心跳到 60s；检查网络拥塞 |
| 4G 发射时系统重启 | ALDO4 电压跌落 | 见 §4.5 R4 电压验证 |
| `+CME ERROR: 4` | 操作不允许 | 等待网络注册完成再拨号 |

---

## 6. 风险标注

| 风险 ID | 描述 | 等级 | 本文档覆盖 | 缓解 |
|---|---|---|---|---|
| **R4** | ALDO4=2.9V 可能不够 ML307C 标称 3.4-4.2V | 🔴 高 | §4.5 电压验证 | M1 万用表实测发射时电压，< 2.7V 则改 PMIC |
| **R7** | 4G 流量费用 | 🟡 中 | §1 流量预估 | 申请定向流量包 ≥ 1GB/月 |
| R2.3 | 4G 延迟比 WiFi 慢 ~100ms | 🟢 低 | §4.3 延迟测试 | DSH < 2s 延迟目标可接受 |
| R2.7 | PPP 拨号 24h 稳定性 | 🟡 中 | §4.4 长稳测试 | 断连自动重连机制 |
| R2.8 | 室内信号差 | 🟡 中 | §4.2 CSQ 记录 | M5 做 WiFi↔4G failover |

---

## 7. CCA p4c5_4g 集成注意事项

CCA 当前实现（`p4c5_4g.cc`）有以下待修正项，M1 集成时需注意：

| 项 | CCA 当前值 | xiaozhi 值 | 建议 |
|---|---|---|---|
| UART 波特率 | 115200 | **921600** | 修正为 921600（AT 高速模式）|
| AT 响应超时 | 2000ms | — | 网络注册阶段需 ≥ 30s |
| TCP API | TODO stub | esp-ml307 CreateTcp() | 集成 esp-ml307 组件 |
| 网络注册 | TODO | WaitForNetworkReady() | 需实现 SIM→信号→注册→拨号全流程 |
| 流量统计 | 无 | 无（esp-ml307 无此 API）| 需自行实现应用层计数 |

**esp-ml307 组件引入**：
```yaml
# idf_component.yml
dependencies:
  78/esp-ml307: "~3.6.4"
```

**关键头文件**：`#include <at_modem.h>`

---

## 8. WiFi ↔ 4G Failover（M5 规划）

```
优先级决策树：
    if WiFi connected:
        use WiFi      # 便宜 + 快 + 省 4G 流量
    elif 4G connected:
        use 4G        # 备用网络
    else:
        offline mode   # 本地缓存，等网络恢复后上传
```

触发切换条件：
- WiFi → 4G：WiFi 断连 > 10s
- 4G → WiFi：WiFi 重新连接 > 30s（避免频繁切换）

> **注意**：M5 阶段实现，M1 仅做 4G 独立测试。

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：M1 真机验证后，CCA 填写 §4 实测数据
