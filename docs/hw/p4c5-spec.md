# ESP32-P4C5 酷世DIY 开发板 — 完整规格书

> **版本**：v1.0（2026-09-06，DSH Day 0）
> **基准**：原理图 PDF + 12 份 IC 规格书 + xiaozhi-esp32 P4C5 板级配置 + ESP-IDF 5.5.5
> **来源**：详见 `docs/hw/datasheets/` 和 `schematic/`

---

## 1. 芯片与核心架构

### 1.1 主控 ESP32-P4

- **架构**：HP RISC-V 双核 400MHz + LP RISC-V 单核 40MHz
- **封装**：QFN104（10×10 mm）
- **PSRAM**：芯片封装内可叠封 16 MB（ESP32-P4NRW16X）或 32 MB（ESP32-P4NRW32X）
- **外设**：55 个 GPIO，支持 MIPI-DSI、MIPI-CSI、I2S、I2C、SPI、UART、SDIO、LEDC、MCPWM、CAN FD、PARLIO 等
- **无线**：**无内置 WiFi/BT**（必须外挂协处理器）

### 1.2 协处理器 ESP32-C5

- **架构**：HP RISC-V 单核 + LP RISC-V 单核
- **无线**：**双频 WiFi6（2.4 + 5GHz, 802.11ax）** + **Bluetooth LE 5** + **Zigbee 3.0** + **Thread 1.4**
- **封装**：QFN48（6×6 mm）
- **通信协议**：**esp_hosted v2.12+**（SDIO/SPI/UART 通用框架，P4 ↔ C5）
- **示例配置**：`CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y`、`CONFIG_SLAVE_IDF_TARGET_ESP32C5=y`

### 1.3 软件基线

| 项 | 版本 |
|---|---|
| **ESP-IDF** | **v5.5.5+**（推荐）或 v6.2 |
| **LVGL** | **v9.4.0** |
| **esp_hosted** | v2.12+ |
| **esp-ml307** | v3.6.4 |
| **esp-sr（可选）** | v2.3.0 |
| **esp_codec_dev** | v1.5+ |

---

## 2. 显示屏（MIPI-DSI）

| 项 | 规格 |
|---|---|
| **屏幕尺寸** | 4.3 寸 |
| **分辨率** | 480 × 800（竖屏）|
| **LCD 控制器** | **ST7102**（MIPI-DSI 2 lane）|
| **触摸 IC** | **ST7123**（I2C）|
| **驱动** | `kevincoooool/esp_lcd_st7102` v1.0.4 + Espressif 注册表 `esp_lcd_touch_st7123` |
| **LCD_RST** | GPIO22 |
| **背光 PWM** | GPIO6（默认高电平）|
| **MIPI 速率** | 820 Mbps/lane（2 lane）|
| **DPI 时钟** | 37.8 MHz |
| **MIPI 时序** | hbp=40, hspw=2, hfp=40, vbp=10, vspw=2, vfp=310 |
| **像素格式** | RGB565 |
| **DMA2D** | 已启用（`use_dma2d = true`）|
| **MIPI PHY 电源** | LDO 通道 3，2.5V（必须先 enable）|

---

## 3. 音频（输入 + 输出）

| 项 | 规格 | 引脚 |
|---|---|---|
| **DAC（扬声器输出）**| **ES8311**（I2C 0x18）| I2S0 |
| **ADC（麦克风输入）**| **ES7210**（I2C 0x40，4 mic 通道）| I2S0 |
| **PA（扬声器功放）**| **NS4150B**（D 类 5V）| GPIO3（PA_EN）|
| **MEMS 麦克风**| **MSM381A3729H9CP**（敏芯微）×4 | 差分接 ES7210 MIC1-4 |
| **采样率**| 24 kHz（输入 + 输出）| — |
| **MCLK** | — | GPIO13 |
| **BCLK** | — | GPIO12 |
| **WS** | — | GPIO10 |
| **DOUT（输出到 PA）**| — | GPIO9 |
| **DIN（从 MIC 输入）**| — | GPIO11 |
| **音频 I2C SDA** | 共享总线 | GPIO7 |
| **音频 I2C SCL** | 共享总线 | GPIO8 |

**音频驱动 component**：`kevincoooool/ksdiy_p4c5_audio` v1.0.2（已封装 BoxAudioCodec，自动初始化 ES8311+ES7210+NS4150B）

---

## 4. 4G 蜂窝通信

| 项 | 规格 |
|---|---|
| **模组型号** | **ML307C-DC-CN**（中移 OneMO Cat.1）|
| **网络制式** | LTE-TDD/LTE-FDD Cat.1 + GSM + GNSS（GPS + 北斗）|
| **驱动** | `78/esp-ml307` v3.6.4 |
| **AT 指令集** | OneMO 扩展 AT（见 `datasheets/ml307c/AT_Commands_Reference_Guide_4G_Series_V2.0.7.pdf`）|
| **PPP 拨号** | 通过 esp-ml307 内部封装（无需裸 AT）|
| **支持协议** | TCP/IP / MQTT / HTTP/HTTPS / FTP / SSL / LwM2M / LBS / GNSS |
| **UART TX** | GPIO53 |
| **UART RX** | GPIO52 |
| **POWER_EN** | GPIO4（高电平使能）|
| **DTR（休眠控制）**| GPIO51 |
| **SIM 卡槽** | Nano SIM + USIM_DET 卡在位检测 |
| **4G VBAT** | AXP2101 ALDO4（2.9V，可控上下电）|

---

## 5. WiFi 无线

| 项 | 规格 |
|---|---|
| **频段** | **2.4 GHz + 5 GHz 双频**（WiFi6 802.11ax）|
| **BLE** | 5.0 LE |
| **802.15.4** | Zigbee 3.0 / Thread 1.4（未来扩展）|
| **通信协议** | **esp_hosted v2.12+**（P4 ↔ C5 via SDIO）|
| **配网方式** | BluFi（xiaozhi 已实现）|

⚠️ **架构变化**：OMT 用 ESP32-C6 单独模组 + `esp_wifi_remote`；本板用 ESP32-C5 + `esp_hosted`。**新硬件推荐直接用 esp_hosted 协议栈**。

---

## 6. 电源管理（AXP2101 PMIC）

| 项 | 规格 | 电压 |
|---|---|---|
| **PMIC 型号** | **AXP2101**（I2C 地址 0x34）| — |
| **DCDC1** | 主电源 | **3.3 V** |
| **DCDC2** | （未用）| — |
| **DCDC3** | （未用）| — |
| **DCDC4** | （未用）| — |
| **DCDC5** | （未用）| — |
| **ALDO1** | 辅助电源 | **1.8 V** |
| **ALDO2** | （未用）| — |
| **ALDO3** | 音频 codec 电源 | **3.3 V** |
| **ALDO4** | 4G 模组 VBAT | **2.9 V** |
| **充电管理** | 内置（无需外置充电 IC）| — |
| **库仑计** | 精确电量估算（±3%）| — |
| **充电状态** | 实时监测（充电中 / 放电中 / 充满）| — |

**PMIC 驱动**：项目本地 `pmic_axp2101` 组件（xiaozhi 已写好）或 Espressif 注册表 `axp2101`。

⚠️ **vs OMT Tab5**：OMT 用 NP-F 电池 + 简单 ADC 电压检测（精度 ±15%）；本板用 AXP2101 库仑计（±3%）。

---

## 7. USB / UART / 调试

| 项 | 规格 | 引脚 |
|---|---|---|
| **USB-UART IC** | **CH343P**（USB CDC-ACM，OS 自带驱动）| — |
| **ESP32_TX（P4 → CH343P）**| — | GPIO37 |
| **ESP32_RX（CH343P → P4）**| — | GPIO4 ⚠️ 与 4G POWER 复用 |
| **BOOTMODE（DTR → GPIO35）**| 自动进入下载模式 | GPIO35 |
| **USB-C 接口** | 供电 + 调试 | — |
| **USB OTG（DP/DM）**| USB 2.0 OTG（可外接 U 盘 / 键盘 / UVC 摄像头）| — |
| **USB_VBUS 检测** | — | GPIO50 |

⚠️ **GPIO4 冲突**：CH343P 的 ESP_RX 和 ML307C 的 POWER_PIN 都使用 GPIO4。xiaozhi 配置把 GPIO4 给 ML307C POWER，CH343P 调试串口走 USB-C CDC，不占 GPIO。

---

## 8. 其他外设

| 接口 | 规格 | 引脚 / 备注 |
|---|---|---|
| **IMU** | LSM6DS3TR-C（I2C，共享总线）| 重力感应 + 横竖屏旋转 |
| **单色 LED** | GPIO34 | 状态指示 |
| **RGB LED** | WS2812 ×4 颗 | GPIO21（DATA）|
| **SD 卡** | SDMMC 4-bit（TF 卡座）| SD_DATA0-3 + SD_CMD + SD_CLK |
| **MIPI-CSI 摄像头** | 2 lane（SC2336 默认 1920×1080@30fps）| MIPI_CLKP/N + MIPI_D0P/N + MIPI_D1P/N |
| **RS485** | 半双工 | GPIO33 RX / GPIO31 TX / GPIO32 DE |
| **DAC** | MCP4725A0T-E/CH（1 通道 12-bit）| I2C 共享总线 |
| **物理按键** | 2 个 | GPIO35（BOOT）+ GPIO0 |
| **MIPI DSI PHY** | LDO 通道 3，2.5V | 必须先 enable |
| **MIPI CSI PHY** | LDO 通道 4 | （待实测）|

---

## 9. 物理规格

| 项 | 规格 |
|---|---|
| **板尺寸** | （待原理图确认）|
| **3D 外壳** | `shell/背盖.stl` + `shell/按键.stl` + `shell/上壳5.stl` + `shell/ESP32P4_KSDIY_P4C5.step` |
| **工作温度** | （待规格书确认）|
| **存储温度** | （待规格书确认）|

---

## 10. 不在本板 / 不使用的资源

- ❌ **WireGuard**：本板不需要（4G 模组自带 VPN 能力）
- ❌ **microlink**：OMT 自定义 4G 组件，本板用 esp-ml307
- ❌ **M5Stack Tab5 BSP**：OMT 专用 BSP，本板用 esp_lcd_st7102 + esp_lcd_touch_st7123 + ksdiy_p4c5_audio

---

## 11. 参考工程

| 工程 | 来源 | 用途 |
|---|---|---|
| **xiaozhi-esp32 P4C5 适配** | `硬件开源资料/.../程序例程/仅支持IDF5.5.5编译/04.advanced.xiaozhi_ksdiy-p4c5.zip` | 完整 AI 助手参考实现 |
| **p4c5_board_test** | `.../04.advanced.p4c5_board_test.zip` | 板级硬件综合测试 |
| **mipi_lcd_touch** | `.../02.beginner.mipi_lcd_touch.zip` | LCD + 触摸最简 demo |
| **lvgl_squareline_demo** | `.../03.development.lvgl_squareline_demo.zip` | LVGL 9 + SquareLine UI |
| **esp_claw_p4c5** | `.../04.advanced.esp_claw_p4c5.zip` | 乐鑫 AI Agent 框架示例 |

---

## 12. 风险标注

| ID | 风险 | 影响 | 缓解 |
|---|---|---|---|
| R1 | GPIO4 冲突（CH343P RX vs ML307 POWER）| 中 | xiaozhi 配置已分配 GPIO4 给 ML307，CH343P 走 USB-C CDC |
| R2 | LSM6DS3TR-C vs LSM6DSLTR 差异 | 低 | xiaozhi 用 cpp_bus_driver 兼容 |
| R3 | MIPI DSI PHY LDO 通道编号（3 vs 其他）| 中 | 必须先 `esp_ldo_acquire_channel(3, 2500)` |
| R4 | xiaozhi sdkconfig 内存配置 | 中 | 参考 `sdkconfig-references/` diff |
| R5 | ESP-IDF 5.4.2 不支持 | 高 | 必须升级到 5.5.5+ |
| R6 | ESP32-P4 内置 WiFi 缺失 | 高 | 必须外挂 ESP32-C5 |
| R7 | 4G 模组功耗大 | 中 | M5 阶段需要降功耗策略 |

---

## 13. 引用

- **OMT 项目**：`/Volumes/ZT-1T/项目开发/OMT/`（v1 硬件 Tab5）
- **macs 项目**：`/Volumes/ZT-1T/项目开发/multi-agent-collab-system/`（协作方法论）
- **硬件原理图**：`docs/hw/schematic/原理图.pdf`
- **IC 规格书目录**：`docs/hw/datasheets/`
- **ML307C AT 文档**：`docs/hw/datasheets/ml307c/`
- **xiaozhi 配置参考**：`docs/hw/sdkconfig-references/`

---

**版本**：v1.0（2026-09-06, DSH Day 0）
**下次更新**：M0-A 完成后，由 CCB 写入 datasheets-summary 详细数据