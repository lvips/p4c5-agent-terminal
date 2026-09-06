# W1 完成总结报告

> **任务**: ESP32-P4 + ESP32-C5 通过 esp_hosted + SDIO 建立 WiFi 链路
> **完成日期**: 2026-09-07
> **状态**: ✅ 完成, WiFi STA connected (192.168.1.248), DSH client init 成功

---

## 🎯 交付成果

| 项目 | 状态 |
|---|---|
| SDIO transport driver 真正 link 进 binary | ✅ |
| ESP32-C5 协处理器 RPC 通信 | ✅ |
| WiFi STA 连接到 `ZTE-SONG-2.4G` | ✅ |
| DHCP 获取 IP `192.168.1.248` | ✅ |
| DSH client init (ws://...) | ✅ |
| heartbeat 显示 wifi=✅ connected | ✅ |
| 设备稳定运行 (无重启/panic) | ✅ |

---

## 🐛 排查过程 (8 个 commits, 多次返工)

### 错误 1: `wifi_manager_start()` → `ESP_ERR_INVALID_ARG`
- **根因**: `ap_behavior = "fallback"` 无效值
- **修复**: 改 `"keep"` (commit `fea477a`)

### 错误 2: TWDT timeout (30s) + `wait_connected(30000)` 阻塞
- **根因**: wait 阻塞超过 watchdog timeout
- **修复**: wait_connected 改 20s (commit `c325ce2`)

### 错误 3: heartbeat `wifi=connected` 硬编码
- **根因**: 心跳显示永远是 "connected", 无法反映真实状态
- **修复**: 用 `wifi_manager_get_status()` 真实检测 (commit `effdfaf`)

### 错误 4: `esp_wifi_init 0x3001` panic (黑白闪屏)
- **临时修复**: `#if 0` 禁用 WiFi init (commit `a8a9908`)

### 错误 5: 屏幕黑白闪屏持续
- **根因**: `tcpip_send_msg_wait_sem (Invalid mbox)` assert
- **触发链**: DSH WebSocket 在无 WiFi 状态下尝试连接 → lwIP getaddrinfo → 无 netif → assert → panic → 重启 → 屏幕重新初始化
- **修复**: 完整禁用 DSH init + heartbeat 中的 wifi/dsh 调用 (commit `e1ca06b`)

### 错误 6: 启用 WiFi 后仍是 SPI transport
- **终极根因**: `CONFIG_ESP_HOST_WIFI_ENABLED=y` 强制开了
  → `ESP_WIFI_REMOTE_ENABLED=n`
  → `Kconfig.idf_v5.5.in` (含 SLAVE_IDF_TARGET) 不被加载
  → `ESP_HOSTED_CP_TARGET_ESP32C5` 的 `depends on SLAVE_IDF_TARGET_ESP32C5` 无法满足
  → fallback 到 `ESP_HOSTED_CP_TARGET_ESP32H2` + SPI transport
  → **SDIO transport 完全没 link 进 binary**
- **修复**: `sdkconfig.defaults` 注释 `ESP_HOST_WIFI_ENABLED`, 加 `ESP_WIFI_REMOTE_ENABLED=y` (commit `bae736c`)

---

## 🔑 关键教训 (DSH 调研纪律)

1. **不能盲改 sdkconfig**: ESP-IDF build 会重新评估 Kconfig 覆盖 patch
2. **必须改 sdkconfig.defaults**: Kconfig source-of-truth 在 defaults
3. **Kconfig cascade 链**: 一个 default `n` 会导致整个菜单不可用
4. **`ESP_HOST_WIFI_ENABLED=y` 与 `ESP_WIFI_REMOTE_ENABLED=y` 互斥**:
   - IDF 5.5 默认开 `ESP_HOST_WIFI_ENABLED` (因为 `SOC_WIRELESS_HOST_SUPPORTED=y`)
   - 我们 P4+C5 路径必须显式禁用 `ESP_HOST_WIFI_ENABLED`, 启用 `ESP_WIFI_REMOTE_ENABLED`

---

## 📊 验证证据 (60s monitor 跟踪)

```
[0/5] Board init (I2C) ✅
[1/5] PMIC init (AXP2101) ✅
[2/5] Display init (ST7102 480x800) ✅
[3/5] Audio init (ES8311+ES7210) ✅
[4/6] WiFi init (esp_hosted SDIO via ESP32-C5) ✅
  ├─ H_SDIO_DRV: sdio_data_to_rx_buf_task started
  ├─ transport: Attempt connection with slave: retry[0]
  ├─ RPC_WRAP: ESP Event: wifi station started
  └─ WiFi STA connected ✅
[5/6] DSH client init... ✅
  └─ DSH client init: url=ws://dsh.example.com/ws, device=p4c5-001
💓 bat=0% wifi=✅ connected ip=192.168.1.248
```

---

## 🚧 W2 待办 (DSH 验证)

| 任务 | 状态 |
|---|---|
| 启动 mock_dsh_server.py (port 8765) | 待办 |
| 改 dsh_client URL 到 `ws://192.168.1.55:8765/ws` | 待办 |
| 真机验证 14 类下行帧 | 待办 |
| 真机验证 4 类上行帧 | 待办 |
| 端到端 PC Adapter ↔ ESP32 ↔ Server 数据流 | 待办 |

---

## 📚 参考

- [W1-wifi-research-report.md](../research/W1-wifi-research-report.md) - 调研报告
- [esp_claw_p4c5](https://github.com/kevincoooool/ESP32P4_KSDIY) - 参考实现
- [esp_hosted 文档](https://github.com/espressif/esp-hosted) - 官方文档
- [dsh-client-frame-protocol.md](../hw/dsh-client-frame-protocol.md) - DSH 协议规范