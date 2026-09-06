# W1: ESP32-C5 + esp_hosted WiFi 集成调研报告

> **版本**：v1.0（2026-09-06，CCB W1-research）
> **参考**：esp_claw_p4c5 开源案例 `/tmp/esp_claw_unzip/`（Kevincoooool 维护）
> **目标**：提炼 WiFi 集成方案，供 CCA 实施

---

## 执行摘要

ESP32-P4 无内置 WiFi，通过 **SDIO 4-bit 总线**（40MHz）外挂 **ESP32-C5** 实现 WiFi 6 双频 + BLE 5.0。
esp_claw_p4c5 已验证此方案，核心组件是 `wifi_manager`（653 行 C），提供 STA/AP/APSTA 三种模式自动切换。
**配网方式：softAP + HTTP 配置页面**（非 BluFi，非 smartconfig），WiFi 凭据存 NVS。
**P4C5 现有 GPIO 无冲突**：SDIO 引脚（GPIO14-19, GPIO54）未被占用。
**集成成本：低** — 可直接移植 esp_claw 的 `wifi_manager` 组件 + `sdkconfig.defaults.board` 中的 ESP_HOSTED 配置。

---

## Q1: SDIO 引脚 + 复位

**来源**：`sdkconfig.defaults.board` L15-38

| 信号 | P4 GPIO | 方向 | 说明 |
|---|---|---|---|
| **SDIO_CLK** | **GPIO18** | P4→C5 | 时钟，40MHz |
| **SDIO_CMD** | **GPIO19** | 双向 | 命令/响应 |
| **SDIO_D0** | **GPIO14** | 双向 | 数据线 0 |
| **SDIO_D1** | **GPIO15** | 双向 | 数据线 1 |
| **SDIO_D2** | **GPIO16** | 双向 | 数据线 2 |
| **SDIO_D3** | **GPIO17** | 双向 | 数据线 3 |
| **C5_RESET** | **GPIO54** | P4→C5 | 低电平复位 C5 |

**关键配置**：
- 总线宽度：4-bit（`CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y`）
- 时钟频率：40MHz（`CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000`）
- TX/RX 队列深度：各 20（`CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE=20`）
- 校验和：**未启用**（`# CONFIG_ESP_HOSTED_SDIO_CHECKSUM is not set`）
- 芯片目标：ESP32-C5（`CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y`）

---

## Q2: wifi_manager 公共 API

**来源**：`wifi_manager.h`（75 行）+ `wifi_manager.c`（653 行）

| # | 函数 | 功能 | 行号 |
|---|---|---|---|
| 1 | `wifi_manager_init()` | 初始化 WiFi 事件循环 + netif + esp_wifi | L422 |
| 2 | `wifi_manager_start(config)` | 启动 STA/AP/APSTA（首次调用 esp_wifi_start）| L455 |
| 3 | `wifi_manager_apply_sta_config(config)` | 热切换 STA 配置（断开重连）| L479 |
| 4 | `wifi_manager_validate_config(config)` | 校验配置合法性 | L212 |
| 5 | `wifi_manager_wait_connected(timeout_ms)` | 阻塞等待连接（EventGroup）| L523 |
| 6 | `wifi_manager_register_state_callback(cb, ctx)` | 注册连接状态回调 | L538 |
| 7 | `wifi_manager_get_status(status)` | 获取当前状态（IP/模式/AP SSID）| L550 |
| 8 | `wifi_manager_scan_aps(records, max, count)` | WiFi 扫描（自动切换模式）| L570 |

**辅助 API**：
- `wifi_manager_get_ap_netif()` — 获取 AP netif（用于 captive DNS）

**数据结构**：
```c
typedef struct {
    const char *sta_ssid;          // STA SSID（空=无配置）
    const char *sta_password;
    const char *ap_ssid_prefix;    // AP SSID 前缀（默认 "esp-claw"）
    const char *ap_ssid;           // 自定义 AP SSID（可选）
    const char *ap_password;       // AP 密码（空=OPEN）
    const char *ap_behavior;       // "keep" | "close_on_sta"
    uint8_t ap_channel;            // AP 信道（默认 1）
    uint8_t ap_max_conn;           // AP 最大连接数（默认 4）
    uint32_t max_retry;            // STA 最大重试次数（默认 5）
} wifi_manager_config_t;
```

---

## Q3: 配置要求（sdkconfig.defaults）

### 3.1 ESP_HOSTED 核心配置（37 项）

**来源**：`sdkconfig.defaults.board` L38-76

```
# 基本启用
CONFIG_ESP_HOSTED_ENABLED=y
CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y

# SDIO 主机接口
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_BUS_WIDTH=4
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000

# SDIO 引脚
CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=54
CONFIG_ESP_HOSTED_SDIO_PIN_CLK=18
CONFIG_ESP_HOSTED_SDIO_PIN_CMD=19
CONFIG_ESP_HOSTED_SDIO_PIN_D0=14
CONFIG_ESP_HOSTED_SDIO_PIN_D1=15
CONFIG_ESP_HOSTED_SDIO_PIN_D2=16
CONFIG_ESP_HOSTED_SDIO_PIN_D3=17

# 队列 + 校验
CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE=20
CONFIG_ESP_HOSTED_SDIO_RX_Q_SIZE=20
# CONFIG_ESP_HOSTED_SDIO_CHECKSUM is not set

# ESP_SDIO 兼容层（重复配置，esp_wifi_remote 使用）
CONFIG_ESP_SDIO_BUS_WIDTH=4
CONFIG_ESP_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_SDIO_GPIO_RESET_SLAVE=54
CONFIG_ESP_SDIO_PIN_CLK=18
CONFIG_ESP_SDIO_PIN_CMD=19
CONFIG_ESP_SDIO_PIN_D0=14
CONFIG_ESP_SDIO_PIN_D1=15
CONFIG_ESP_SDIO_PIN_D2=16
CONFIG_ESP_SDIO_PIN_D3=17
CONFIG_ESP_SDIO_TX_Q_SIZE=20
CONFIG_ESP_SDIO_RX_Q_SIZE=20
# CONFIG_ESP_SDIO_CHECKSUM is not set
CONFIG_ESP_GPIO_SLAVE_RESET_SLAVE=54
```

### 3.2 WiFi/BT 相关 menuconfig

**来源**：`sdkconfig.old` L1718-1731

| 配置项 | 值 | 说明 |
|---|---|---|
| `CONFIG_ESP_WIFI_NVS_ENABLED` | y | WiFi 配置持久化到 NVS |
| `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM` | 16 | 静态 RX 缓冲 |
| `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` | 32 | 动态 RX 缓冲 |
| `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM` | 32 | 动态 TX 缓冲 |
| `CONFIG_ESP_WIFI_AMPDU_TX_ENABLED` | y | A-MPDU 发送（WiFi 6）|
| `CONFIG_ESP_WIFI_AMPDU_RX_ENABLED` | y | A-MPDU 接收 |
| `CONFIG_ESP_WIFI_TX_BA_WIN` | 6 | TX Block Ack 窗口 |
| `CONFIG_ESP_WIFI_RX_BA_WIN` | 16 | RX Block Ack 窗口 |
| `CONFIG_ESP_WIFI_IRAM_OPT` | y | WiFi 代码放 IRAM |

### 3.3 APP_WIFI 应用配置

| 配置项 | 默认值 | 说明 |
|---|---|---|
| `CONFIG_APP_WIFI_SSID` | "" | STA SSID（空=AP 模式）|
| `CONFIG_APP_WIFI_PASSWORD` | "" | STA 密码 |
| `CONFIG_APP_WIFI_AP_SSID_PREFIX` | "esp-claw" | AP SSID 前缀 |
| `CONFIG_APP_WIFI_AP_CHANNEL` | 1 | AP 信道 |
| `CONFIG_APP_WIFI_AP_MAX_CONN` | 4 | AP 最大连接 |
| `CONFIG_APP_WIFI_MAX_RETRY` | 5 | STA 最大重试 |

---

## Q4: WiFi 启动流程

### 4.1 调用链（esp_claw main.c）

```
app_main()
  │
  ├─ init_nvs()                          ← NVS 初始化（WiFi 配置持久化）
  ├─ app_config_init() + app_config_load() ← 从 NVS/SD 加载配置
  ├─ esp_board_manager_init()            ← 板级外设初始化（含 esp_hosted 驱动）
  ├─ app_fs_init()                       ← 文件系统
  │
  ├─ wifi_manager_init()                 ← 创建 event group, netif, esp_wifi_init
  ├─ http_server_init()                  ← HTTP 配置服务器
  ├─ wifi_manager_register_state_callback() ← 注册状态回调
  │
  └─ wifi_manager_start(config)          ← 配置 STA/AP + esp_wifi_start()
        │
        ├─ sta_ssid != "" → APSTA 模式 → esp_wifi_connect()
        │     ├─ 成功 → WIFI_MODE_APSTA_OK → "close_on_sta" 可关 AP
        │     └─ 5 次重试失败 → fallback_to_ap() → WIFI_MODE_AP_FALLBACK
        │
        └─ sta_ssid == "" → PROVISION_AP 模式（仅 AP，等待配网）
              └─ AP SSID = "esp-claw-XXXXXX"（后 3 字节 MAC）

  └─ wifi_manager_wait_connected(30000)  ← 阻塞 30s 等待连接
  ─ http_server_start()                 ← 启动 HTTP 配网门户
  └─ captive_dns_start()                 ← Captive DNS（弹出门户）
```

### 4.2 三种模式自动切换

| 模式 | 触发条件 | WiFi 模式 | 行为 |
|---|---|---|---|
| **PROVISION_AP** | sta_ssid 为空 | WIFI_MODE_AP | 仅 AP，等待 HTTP 配网 |
| **APSTA_TRYING** | sta_ssid 非空 | WIFI_MODE_APSTA | STA 连接中 + AP 同时存在 |
| **APSTA_OK** | STA 连接成功 + ap_behavior="keep" | WIFI_MODE_APSTA | STA 已连 + AP 保留 |
| **STA_ONLY** | STA 连接成功 + ap_behavior="close_on_sta" | WIFI_MODE_STA | 仅 STA，AP 已关闭 |
| **AP_FALLBACK** | STA 5 次重试失败 | WIFI_MODE_AP | 回退到 AP 配网模式 |

### 4.3 状态机（wifi_event_handler）

```
WIFI_EVENT_STA_DISCONNECTED → 指数退避重连（1s → 2s → 4s → 8s → 16s → 30s max）
  → 5 次失败 → fallback_to_ap() → 回退纯 AP 模式

IP_EVENT_STA_GOT_IP → s_connected=true → 根据 ap_behavior 决定是否关 AP
```

---

## Q5: 配网方式

### 5.1 配网协议：**softAP + HTTP**（非 BluFi）

esp_claw **不使用 BluFi**，**不使用 smartconfig**。

**配网流程**：
1. 首次启动无 WiFi 配置 → 自动创建 AP（SSID: `esp-claw-XXXXXX`）
2. 手机连接此 AP → 自动弹出 captive portal
3. 浏览器访问 `http://192.168.4.1/` → HTTP 配置页面
4. 输入 SSID + 密码 → HTTP POST 保存配置 → 设备自动重连 STA
5. STA 连接成功后，根据 `ap_behavior` 决定是否关闭 AP

**配置存储**：**NVS**（`CONFIG_ESP_WIFI_NVS_ENABLED=y`）
- `app_config.c` 使用 NVS 持久化 `wifi_ssid`、`wifi_password`、`ap_behavior` 等
- 支持 `nvs_flash_erase()` 恢复出厂设置

### 5.2 AP SSID 生成规则

```
无自定义 SSID → "esp-claw-XXXXXX"（后 3 字节 = WiFi MAC 或 base MAC）
有自定义 SSID → 使用配置值
```

### 5.3 AP 密码

- 无密码 → `WIFI_AUTH_OPEN`（开放网络）
- 有密码（≥8 字符）→ `WIFI_AUTH_WPA2_PSK`

---

## Q6: 与 p4c5-agent-terminal 兼容性

### 6.1 GPIO 冲突检查

| SDIO 信号 | GPIO | p4c5 占用？ | 冲突 |
|---|---|---|---|
| SDIO_CLK | 18 |  未用 | ✅ 无冲突 |
| SDIO_CMD | 19 | ❌ 未用 | ✅ 无冲突 |
| SDIO_D0 | 14 | ❌ 未用 | ✅ 无冲突 |
| SDIO_D1 | 15 | ❌ 未用 | ✅ 无冲突 |
| SDIO_D2 | 16 | ❌ 未用 | ✅ 无冲突 |
| SDIO_D3 | 17 | ❌ 未用 | ✅ 无冲突 |
| C5_RESET | 54 | ❌ 未用 | ✅ 无冲突 |

**结论**：SDIO 引脚**完全空闲**，无 GPIO 冲突。

### 6.2 启动顺序调整

| 当前 p4c5 顺序 | 建议调整 |
|---|---|
| Board → PMIC → Display → Audio → 4G | Board → PMIC → **WiFi (esp_hosted init)** → Display → Audio |
| 4G 组件 [4/5] | **替换为 WiFi 组件**（4G 搁置）|

**注意**：
- `esp_board_manager_init()` 会自动初始化 esp_hosted 驱动（SDIO 枚举 + C5 复位）
- WiFi init 必须在 PMIC 之后（需要 DCDC1=3.3V 稳定）
- Display/Audio 不受影响（走 I2C/MIPI/I2S，与 SDIO 独立）

### 6.3 AXP2101 ALDO4 影响

- WiFi 模式下 **ML307C 4G 模组不需要上电**
- ALDO4 可保持关闭（节省 ~15mA）
- DCDC1 (3.3V) 需确保稳定 — esp_hosted SDIO 通信需要 3.3V

### 6.4 组件依赖

| esp_claw 组件 | p4c5 移植难度 | 说明 |
|---|---|---|
| `wifi_manager` | **低** — 直接复制 | 纯 C，无 esp_claw 依赖 |
| `esp_hosted` | **零** — ESP-IDF 组件 | menuconfig 启用即可 |
| `esp_wifi_remote` | **零** — ESP-IDF 组件 | menuconfig 启用即可 |
| `nvs_flash` | **零** — ESP-IDF 标准 | 已可用 |
| `http_server` 配网门户 | **中** — 需移植 | 含 HTML 前端，可简化 |
| captive DNS | **低** — 可选 | 增强体验，非必须 |

---

## CCA 集成 Checklist

- [ ] **1. 复制 `sdkconfig.defaults.board` 的 ESP_HOSTED 配置**到 p4c5 项目（37 项）
- [ ] **2. 复制 `wifi_manager.c` + `wifi_manager.h`** 为新组件 `components/wifi_manager/`
- [ ] **3. 删除/禁用 `p4c5_4g` 组件**（4G 搁置，避免 UART/ML307 初始化冲突）
- [ ] **4. 在 `app_main.c` 中添加 `nvs_flash_init()`**（wifi_manager 依赖 NVS）
- [ ] **5. 在 `app_main.c` 中调用 `wifi_manager_init()` + `wifi_manager_start()`**
- [ ] **6. 添加 `wifi_ssid` / `wifi_password` 配置项**（NVS 或硬编码测试）
- [ ] **7. 验证 SDIO 枚举日志**：启动时应有 `esp_hosted: SDIO slave detected` 类似日志
- [ ] **8. 验证 WiFi STA 连接**：`wifi_manager_wait_connected(30000)` 应返回 ESP_OK
- [ ] **9. 验证 AP fallback**：断开路由器 → 应自动回退到 `esp-claw-XXXXXX` AP
- [ ] **10. 验证 GPIO54 C5 复位**：上电时 GPIO54 应有复位脉冲
- [ ] **11. 确认 DCDC1=3.3V 稳定**：esp_hosted SDIO 40MHz 需要稳定 3.3V
- [ ] **12. ALDO4 可关闭**：无 4G 需求时 `p4c5_pmic_set_4g_power(false)` 节省功耗
- [ ] **13. 可选：移植 HTTP 配网门户**（或先用硬编码 SSID 测试）
- [ ] **14. 可选：BLE 5.0** — esp_hosted 同时提供 BLE 接口，但需额外初始化
- [ ] **15. 验证功耗**：WiFi STA 连接后待机电流（目标 < 100mA）

---

**来源**：
- esp_claw wifi_manager: `/tmp/esp_claw_unzip/components/common/wifi_manager/` (653+75 lines)
- esp_claw sdkconfig: `/tmp/esp_claw_unzip/application/edge_agent/boards/ksdiy/p4_c5_4_3/sdkconfig.defaults.board`
- esp_claw main: `/tmp/esp_claw_unzip/application/edge_agent/main/main.c` (L249-345)
- p4c5 config: `hardware/p4c5-agent-terminal/main/config.h`
- p4c5 4G: `hardware/p4c5-agent-terminal/components/p4c5_4g/p4c5_4g.cc`

**下次更新**：CCA 实施后验证结果
