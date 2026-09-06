# p4c5-agent-terminal 硬件资料总索引

> **版本**：v1（2026-09-06，DSH Day 0）
> **目的**：让你 5 分钟内搞清楚这块开发板是什么、能干嘛、怎么接

---

## 一、一句话

> **ESP32-P4C5 酷世DIY 开发板** = ESP32-P4（双核 RISC-V 400MHz 主控） + ESP32-C5（双频 WiFi6 协处理器） + 4.3 寸 ST7102 MIPI-DSI 触屏 + ML307C 4G 模组 + AXP2101 完整电源管理 + ES8311/ES7210 音频 codec + LSM6DS3 6 轴 IMU + WS2812 RGB LED + RS485 + SD 卡 + MIPI-CSI 摄像头接口

---

## 二、5 分钟速查

### 2.1 主芯片

| 项 | 规格 |
|---|---|
| 主控 | **ESP32-P4**（HP 双核 400MHz + LP 单核 40MHz，QFN104 10×10mm）|
| WiFi/BLE 协处理器 | **ESP32-C5**（双频 WiFi6 + BLE5 + Zigbee/Thread，QFN48 6×6mm）|
| P4↔C5 通信 | **esp_hosted v2.12+**（SDIO/SPI/UART 通用框架）|
| IDF 版本 | **v5.5.5+** 或 v6.2（v5.4.2 不支持本板）|
| PSRAM | 16MB 或 32MB（`ESP32-P4NRW16X` / `ESP32-P4NRW32X`）|

### 2.2 显示屏 & 触摸

| 项 | 规格 |
|---|---|
| 屏幕 | 4.3 寸 480×800（竖屏） |
| LCD 控制器 | **ST7102**（MIPI-DSI 2 lane, 820 Mbps/lane, DPI 时钟 37.8MHz）|
| LCD 复位 | GPIO22 |
| 背光 PWM | GPIO6 |
| 触摸 IC | **ST7123**（I2C 共享总线）|
| 触摸中断 | GPIO23 |

### 2.3 音频

| 项 | 规格 |
|---|---|
| DAC（输出）| **ES8311**（24kHz）|
| ADC（输入）| **ES7210**（4 mic 阵列）|
| PA（扬声器功放）| **NS4150B**（D 类，5V）使能 GPIO3 |
| MEMS 麦克风 | **MSM381A3729H9CP**（敏芯微）×4 个 |
| I2S 引脚 | MCLK=GPIO13, BCLK=GPIO12, WS=GPIO10, DOUT=GPIO9, DIN=GPIO11 |
| I2C 引脚 | SDA=GPIO7, SCL=GPIO8（共享总线）|

### 2.4 4G 蜂窝

| 项 | 规格 |
|---|---|
| 模组 | **ML307C-DC-CN**（中移 OneMO Cat.1）|
| 驱动 | `78/esp-ml307` v3.6.4 |
| UART | TX=GPIO53, RX=GPIO52 |
| POWER | GPIO4 |
| DTR（休眠）| GPIO51 |
| SIM 卡 | Nano SIM + USIM_DET 检测 |
| 支持 | TCP/IP / MQTT / HTTP / SSL / FTP / LwM2M / GNSS |

### 2.5 电源

| 项 | 规格 |
|---|---|
| PMIC | **AXP2101**（I2C 0x34）|
| DCDC1 | 3.3V（主电源）|
| ALDO1 | 1.8V |
| ALDO3 | 3.3V（音频）|
| ALDO4 | 2.9V（4G VBAT）|
| 电池 | 标准锂电池（充电由 AXP2101 管理）|

### 2.6 其他接口

| 接口 | 规格 |
|---|---|
| USB-C | USB DP/DM（OTG）+ CH343P USB-UART |
| 烧录 | GPIO35 BOOTMODE 自动下载 |
| SD 卡 | SDMMC 4-bit（TF 卡座）|
| 摄像头 | MIPI-CSI 2 lane（SC2336 默认）+ USB UVC |
| IMU | LSM6DS3TR-C（I2C 共享总线）|
| LED | GPIO34 单色 + GPIO21 WS2812 RGB |
| RS485 | GPIO33 RX / GPIO31 TX / GPIO32 DE |
| DAC | MCP4725（1 通道，I2C 共享）|
| 物理按键 | GPIO35 BOOT + GPIO0 用户按键 |
| MIPI DSI PHY | LDO 通道 3，2.5V |

---

## 三、目录结构

```
docs/hw/
├── README.md                          # 本文件（5 分钟速查）
├── p4c5-spec.md                       # 完整规格书（7 章节）
├── p4c5-pins.csv                      # 引脚定义表（机器可读）
├── 00-新硬件差异矩阵.md                # vs OMT Tab5
├── datasheets/                        # 15 份 IC 规格书 PDF
│   ├── datasheets/                    # 12 份 IC PDF
│   └── ml307c/                        # ML307C AT 文档（14 份）
├── datasheets-summary/                # M0-A 调研产出（5 模块）
│   ├── p4c5-board.md
│   ├── ml307c.md
│   ├── audio-codecs.md
│   ├── pmic-axp2101.md
│   └── peripherals.md
├── schematic/                         # 原理图 + 阅读指南
│   └── 原理图.pdf
├── sdkconfig-references/              # xiaozhi sdkconfig diff
├── reference-codes/                   # xiaozhi 板级代码导读
├── tools/                             # 开发工具索引
└── shell/                             # 3D 外壳（STL + STEP）
```

---

## 四、按主题速查

| 你想看什么 | 跳到 |
|---|---|
| 完整规格（7 章节）| `p4c5-spec.md` |
| 引脚定义（可生成代码）| `p4c5-pins.csv` |
| 和 OMT Tab5 的差异 | `00-新硬件差异矩阵.md` |
| 某颗 IC 怎么接 | `datasheets-summary/<topic>.md` |
| 原理图怎么读 | `schematic/README.md` |
| xiaozhi 怎么配置的 | `sdkconfig-references/` |
| xiaozhi 代码怎么读 | `reference-codes/` |
| 烧录 / 调试工具 | `tools/` |
| 3D 打印外壳 | `shell/` |

---

## 五、关键资料链接

| 资源 | URL |
|---|---|
| xiaozhi-esp32 P4C5 适配版 | 本仓库 `硬件开源资料/ESP32P4C5开发板 酷世DIY/程序例程/仅支持IDF5.5.5编译/04.advanced.xiaozhi_ksdiy-p4c5.zip` |
| xiaozhi 上游 | https://github.com/78/xiaozhi-esp32 |
| KSDIY 官方仓库 | https://github.com/kevincoooool/ESP32P4_KSDIY |
| esp-ml307（4G 驱动） | https://github.com/78/esp-ml307 |
| esp_hosted | https://components.espressif.com/components/espressif/esp_hosted |
| ksdiy_p4c5_audio | https://components.espressif.com/components/kevincoooool/ksdiy_p4c5_audio |
| esp_lcd_st7102 | https://components.espressif.com/components/kevincoooool/esp_lcd_st7102 |
| 板介绍 | https://www.cnx-software.com/2025/12/17/compact-development-board-features-a-single-esp32-p4-esp32-c5-dual-band-wi-fi-6-module-mipi-d-splay-and-camera-interfaces/ |

---

## 六、开发前必读

1. **装 ESP-IDF 5.5.5+**（不要用 5.4.2，本板不支持）
2. **用 xiaozhi 工程作为起点** — 它已实现 90% 硬件初始化
3. **遵循文件边界**：CCA 写 `hardware/` + `docs/hw/`，CCB 写 `docs/research/`
4. **5 大模块资料已全** — 不需要再外网调研

---

## 七、版本

- v1.0 (2026-09-06): 初版，DSH Day 0 建立