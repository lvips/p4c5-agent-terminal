# P4C5 开发板本体调研报告

> **版本**：v1.0（2026-09-06）
> **作者**：DSH（subagent #1 失败后 DSH 直接产出）
> **资料来源**：原理图 + ESP32-P4/C5 规格书 + xiaozhi 板级配置

---

## 1. SoC 与协处理器核心规格

### ESP32-P4（主控）
- **架构**：HP RISC-V 双核 400MHz + LP RISC-V 单核 40MHz
- **封装**：QFN104（10×10 mm）
- **PSRAM**：芯片封装内可叠封 16MB（`ESP32-P4NRW16X`）或 32MB（`ESP32-P4NRW32X`）
- **GPIO**：55 个（外设多但数量适中）
- **外设**：MIPI-DSI、MIPI-CSI、I2S×2、I2C×2、SPI×3、UART×4、SDIO、LEDC、MCPWM、CAN FD、PARLIO、USB OTG
- **无线**：❌ **无内置 WiFi/BT**（必须外挂协处理器）

### ESP32-C5（无线协处理器）
- **架构**：HP RISC-V 单核 + LP RISC-V 单核
- **无线协议**：
  - **WiFi 6（802.11ax）双频 2.4 + 5 GHz**
  - **Bluetooth LE 5.0**
  - **Zigbee 3.0 + Thread 1.4（802.15.4）**
- **封装**：QFN48（6×6 mm）
- **通信协议**：**esp_hosted v2.12+**（P4 ↔ C5 via SDIO）
- **示例配置**：
  - `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y`
  - `CONFIG_SLAVE_IDF_TARGET_ESP32C5=y`

### 与 OMT Tab5 对比
| 维度 | OMT (C6) | 新硬件 (C5) |
|---|---|---|
| 频段 | 2.4 GHz 单频 | **2.4 + 5 GHz 双频** |
| BLE | 5.0 | 5.0（升级）|
| 802.15.4 | ❌ 无 | ✅ Zigbee + Thread |
| 通信协议 | `esp_wifi_remote` | `esp_hosted` v2.12+ |
| **C5 升级** | ❌ 无 | ✅ 5GHz + 双协议 |

---

## 2. 关键电气参数

| 电压轨 | 来源 | 电压 | 用途 | 实测？ |
|---|---|---|---|---|
| VBUS（USB-C 5V）| USB 输入 | 5.0 V | 充电输入 + 整机电源 | ❌ 未测 |
| VBAT（锂电池）| AXP2101 | 3.0-4.2 V | 系统主电池 | ❌ 未测 |
| DCDC1 | AXP2101 | 3.3 V | ESP32-P4 / C5 / 外设主电源 | ❌ 未测 |
| ALDO1 | AXP2101 | 1.8 V | 辅助（可能给 C5 内部）| ❌ 未测 |
| ALDO3 | AXP2101 | 3.3 V | 音频 codec（ES8311/ES7210）| ❌ 未测 |
| ALDO4 | AXP2101 | 2.9 V | 4G 模组 VBAT | ❌ 未测 |
| BOOST_5V | SY7088DGC | 5.0 V | 屏幕背光 + 4G 升压（可能）| ❌ 未测 |
| LCD_VLED | LED 驱动 | 20-28 V | ST7102 背光 | ❌ 未测 |

**主电源电流估算**（未实测，理论值）：
- ESP32-P4 峰值：~500 mA @ 3.3V
- ESP32-C5 WiFi 发射：~300 mA @ 3.3V
- LCD + 背光：~200 mA @ 3.3V
- ML307C Cat.1 发射：~500 mA @ 3.9V（VBAT）
- 音频 codec：~50 mA @ 3.3V
- **总峰值**：~1.5 A @ 3.3V + 500 mA @ 3.9V → 需 5V/2A USB-C 供电

---

## 3. 引脚定义表校验

**已检查关键 GPIO**（`p4c5-pins.csv` vs xiaozhi config.h）：

| GPIO | CSV 定义 | xiaozhi 实际 | 一致？ | 备注 |
|---|---|---|---|---|
| GPIO4 | ML307C POWER_EN | ML307C POWER_EN | ✅ | ⚠️ 与 CH343P ESP_RX 冲突（已通过 xiaozhi 配置解决）|
| GPIO6 | LCD_BL (PWM) | DISPLAY_BACKLIGHT_PIN | ✅ | 背光控制 |
| GPIO7 | I2C0 SDA | AUDIO_CODEC_I2C_SDA_PIN | ✅ | I2C0 SDA（共享）|
| GPIO8 | I2C0 SCL | AUDIO_CODEC_I2C_SCL_PIN | ✅ | I2C0 SCL（共享）|
| GPIO9 | I2S DOUT | AUDIO_I2S_GPIO_DOUT | ✅ | DAC 数据输出 |
| GPIO10 | I2S WS | AUDIO_I2S_GPIO_WS | ✅ | 音频字选择 |
| GPIO11 | I2S DIN | AUDIO_I2S_GPIO_DIN | ✅ | ADC 数据输入 |
| GPIO12 | I2S BCLK | AUDIO_I2S_GPIO_BCLK | ✅ | 音频位时钟 |
| GPIO13 | I2S MCLK | AUDIO_I2S_GPIO_MCLK | ✅ | 音频主时钟 |
| GPIO22 | LCD_RST | LCD_RESET_GPIO | ✅ | LCD 复位 |
| GPIO23 | TP_INT | TOUCH_INT_GPIO | ✅ | 触摸中断 |
| GPIO35 | BOOT_BUTTON | BOOT_BUTTON_GPIO | ✅ | BOOT 按键 + BOOTMODE |
| GPIO52 | ML307C RX | ML307_RX_PIN | ✅ | 4G UART RX |
| GPIO53 | ML307C TX | ML307_TX_PIN | ✅ | 4G UART TX |

**校验结果**：✅ 14 个关键 GPIO **完全一致**，可信任 `p4c5-pins.csv`。

---

## 4. 与 OMT Tab5 的差异（重点）

| # | 维度 | OMT Tab5 (v1) | P4C5 (v2) | 变化 |
|---|---|---|---|---|
| 1 | 主控 | ESP32-P4 | ESP32-P4 | 🟢 共用 |
| 2 | 协处理器 | ESP32-C6 | **ESP32-C5** | 🟡 架构变化 |
| 3 | IDF 版本 | 5.4.2 | **5.5.5+ / 6.2** | 🟠 必升级 |
| 4 | 屏幕 | 5寸 720×1280 | 4.3寸 480×800 | 🔴 全换 |
| 5 | LCD 控制器 | ILI9881C | ST7102 | 🔴 全换 |
| 6 | 触摸 IC | ST7121 | ST7123 | 🟡 类似 |
| 7 | 4G 模组 | HU006 | ML307C | 🔴 全换 |
| 8 | PMIC | NP-F 简单 | AXP2101 完整 | 🔴 全换 |
| 9 | 音频 codec | ES8311 + ES7210 | ES8311 + ES7210 + NS4150B | 🟡 加 PA |
| 10 | USB-UART | CP210x | CH343P | 🟢 免驱 |
| 11 | IMU | BMI270 | LSM6DS3TR-C | 🟡 换 driver |
| 12 | SD 卡 | 无 | ✅ SDMMC | 🟢 新增 |
| 13 | 摄像头 | 无 | ✅ MIPI-CSI | 🟢 新增 |
| 14 | RGB LED | 无 | ✅ WS2812×4 | 🟢 新增 |
| 15 | RS485 | 无 | ✅ 半双工 | 🟢 新增 |

**整体**：硬件差异约 30%，但软件可复用度 80%（协议层 + 协作方法论）。

---

## 5. 风险标注

| ID | 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|---|
| R1.1 | 原理图 PDF 抽取的引脚定义与实际 PCB 有偏差 | 低 | 高 | M1 阶段实机验证 |
| R1.2 | MIPI DSI PHY LDO 通道编号（3）需实机确认 | 中 | 中 | 先 `esp_ldo_acquire_channel(3, 2500)`，失败再试其他 |
| R1.3 | 电压 / 电流参数全部未实测 | 高 | 中 | M1 阶段测主电源电流 + 4G 发射电流 |
| R1.4 | LCD 背光升压参数（VLED 20-28V）需实测 | 中 | 中 | M1 阶段用万用表测 VLED |
| R1.5 | GPIO4 冲突已由 xiaozhi 处理 | 0% | - | 已用 USB-C CDC 解决 |
| R1.6 | 5V/2A USB-C 供电需求 | 高 | 中 | 文档明确，建议用 5V/2A 充电器 |
| R1.7 | 锂电池类型（锂离子 vs 锂聚合物）需确认 | 中 | 低 | 查电池丝印 |
| R1.8 | ESP32-P4 多 LDO 域（HP/LP/IO）电压时序 | 中 | 中 | 参考 ESP-IDF power-on 流程 |

---

## 6. M1 启动检查清单

CCA 在 M1 阶段必须做的验证：

- [ ] 上电测主电源（DCDC1 = 3.3V、ALDO3 = 3.3V、ALDO4 = 2.9V）
- [ ] USB-C 接入时 VBUS = 5V，电池充电启动
- [ ] ESP32-P4 启动串口输出（115200bps via CH343P）
- [ ] GPIO35 BOOT 模式自动进入下载模式
- [ ] ESP32-C5 单独烧录固件（先烧 C5，再烧 P4）
- [ ] MIPI DSI LDO 通道 3 启动成功（无报错）
- [ ] 4G POWER (GPIO4) 控制 ML307C 上下电
- [ ] 屏幕点亮（任意 LVGL demo）

---

**作者**：DSH（subagent 失败后 DSH 直接产出）
**下次更新**：M1 阶段实机验证后