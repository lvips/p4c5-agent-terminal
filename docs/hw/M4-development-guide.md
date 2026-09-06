# M4 阶段开发指南 — P4C5 Agent Terminal

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA（主力开发）
> **目的**：CCA 接手后 5 分钟内知道每个组件怎么移植、参考哪个 xiaozhi 文件、怎么验证

---

## 1. M4 阶段目标

**4 个核心目标**：

1. **硬件驱动全覆盖**：10 个组件（屏/触摸/音频/PMIC/4G/IMU/LED/SD/WiFi/协议层）全部可编译、可运行
2. **真机 PoC 验证**：屏幕点亮 + 4G 拨号 + 音频录放 + 电量读取，每个驱动 commit 前真机过一遍
3. **协议层集成**：dsh_client 14 类帧格式 100% 复用 OMT，WebSocket 连通 DSH
4. **LVGL UI 基础框架**：开机画面 + 状态栏 + 语音交互界面骨架

**验收清单**：

- [ ] `idf.py set-target esp32p4` 编译通过（零 error、零 warning）
- [ ] ST7102 LCD 点亮（LVGL demo 可交互）
- [ ] ST7123 触摸响应（坐标正确映射）
- [ ] ES8311 扬声器播放 1kHz 测试音
- [ ] ES7210 4 麦录音可回放
- [ ] AXP2101 电量读取（精度 ±3%）
- [ ] ML307C 4G 拨号成功（获得 IP）
- [ ] esp_hosted WiFi 连接（P4 ↔ C5 SDIO）
- [ ] WS2812 至少 1 颗 LED 变色
- [ ] SD 卡读写正常
- [ ] dsh_client WebSocket 连 DSH（本地模拟）

**时间预估**：3-4 周（CCA 全职）

---

## 2. ESP-IDF 项目脚手架

### 2.1 目录结构

```
p4c5-agent-terminal/
├── CMakeLists.txt                   # 顶层构建（指定 target=esp32p4）
├── sdkconfig.defaults               # 默认配置（IDF 5.5.5+）
├── partitions.csv                   # 分区表（4MB app + SPIFFS）
├── idf_component.yml                # 组件依赖声明
├── components/
│   ├── p4c5_board/                  # 板级统一初始化
│   ├── p4c5_pmic/                   # AXP2101 PMIC 驱动
│   ├── p4c5_display/                # ST7102 LCD + ST7123 触摸
│   ├── p4c5_audio/                  # ES8311 + ES7210 + NS4150B
│   ├── p4c5_4g/                     # ML307C 封装
│   ├── p4c5_imu/                    # LSM6DS3 IMU
│   ├── p4c5_led/                    # WS2812 × 4
│   ├── p4c5_storage/                # SDMMC SD 卡
│   ├── p4c5_lvgl/                   # LVGL UI 框架
│   └── dsh_client/                  # DSH 协议层（复用 OMT）
├── main/
│   ├── main.cc                      # 入口
│   ├── app_wifi.cc                  # WiFi (esp_hosted) 管理
│   ├── app_network.cc               # 4G/WiFi failover
│   └── app_ui.cc                    # LVGL UI 逻辑
└── test/                            # 单元测试
```

### 2.2 关键决策

| 项 | 决策 | 理由 |
|---|---|---|
| **IDF 版本** | **≥ 5.5.5**（xiaozhi 仅支持此版本）| xiaozhi zip 文件标注"仅支持 IDF5.5.5 编译"，当前本机 5.4.2 **必须升级** |
| **Target** | `esp32p4`（P4 主控）+ `esp32c5`（C5 协处理器单独烧录）| P4 无内置 WiFi，必须双芯 |
| **C++ 标准** | C++17（xiaozhi 全用 .cc）| 与 xiaozhi 保持一致 |
| **LVGL 版本** | v9.4.0（`lvgl/lvgl: ~9.4.0`）| xiaozhi 已验证 |
| **音频库** | `espressif/esp_audio_codec: ~2.4.1` | 封装 ES8311+ES7210，无需裸驱动 |

### 2.3 `idf_component.yml` 核心依赖

```yaml
dependencies:
  espressif/esp_audio_codec: ~2.4.1
  espressif/esp_codec_dev: ~1.5.4
  78/esp-ml307: ~3.6.4
  78/esp-wifi-connect: ~3.1.1
  espressif/esp_hosted:
    version: 2.12.3
    rules:
      - if: target in [esp32p4]
  lvgl/lvgl: ~9.4.0
  esp_lvgl_port: ~2.7.0
  espressif/led_strip: ~3.0.2
  espressif/esp_lcd_touch_st7123: ^1.0.0
  espressif/esp_video:
    version: ==1.3.1
    rules:
      - if: target in [esp32p4]
  llgok/cpp_bus_driver:
    version: 1.1.0
    rules:
      - if: target in [esp32p4]
  # 本地组件
  esp_lcd_st7102:
    path: ../components/esp_lcd_st7102
  pmic_axp2101:
    path: ../components/pmic_axp2101
```

### 2.4 编译步骤

```bash
# 1. 升级 IDF（当前 5.4.2 → 5.5.5+）
cd ~/esp/esp-idf && git fetch && git checkout v5.5.5 && ./install.sh

# 2. 设置 target
idf.py set-target esp32p4

# 3. 编译
idf.py build

# 4. 烧录 P4
idf.py -p /dev/ttyACM0 flash monitor

# 5. 烧录 C5（WiFi 协处理器，单独项目）
cd components/esp_hosted/slave && idf.py set-target esp32c5 && idf.py build
```

---

## 3. 10 个组件移植指南

### 3.1 p4c5_board（板级统一初始化）

- **要做什么**：实现 `P4c5Board` 类，按顺序初始化 I2C → PMIC → Display → Audio → 4G → Buttons。参考 xiaozhi 的 `KevinP4c54gBoard` 构造顺序。
- **参考 xiaozhi**：`main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc`（全文 346 行）+ `config.h`
- **本项目文件**：`components/p4c5_board/p4c5_board.cc` + `config.h`
- **依赖规格书**：`p4c5-spec.md` + `p4c5-pins.csv`
- **验证**：编译通过 + 串口输出 `I (xxx) KevinP4c5_4g: AXP2101 codec rails enabled`
- **风险**：初始化顺序错误可能导致 PMIC 未上电就访问 I2C 设备 → **PMIC 必须最先初始化**

### 3.2 p4c5_pmic（AXP2101 电源管理）

- **要做什么**：直接复用 xiaozhi 的 `pmic_axp2101` 组件（4 个源文件），外加 `Axp2101` C++ 包装类（读电量/充电状态）。14 个特殊寄存器配置必须**双源核对**（xiaozhi 代码 + AXP2101 规格书）。
- **参考 xiaozhi**：
  - `components/pmic_axp2101/axp2101.c` + `.h`（C API：init/set_voltage/set_enabled）
  - `components/pmic_axp2101/axp2101_registers.c` + `.h`（寄存器定义）
  - `main/boards/common/axp2101.cc` + `.h`（C++ Axp2101 类：IsCharging/GetBatteryLevel）
- **本项目文件**：`components/p4c5_pmic/`（直接拷贝上述 6 个文件 + 封装）
- **依赖规格书**：`docs/hw/datasheets/AXP2101*.pdf` + `docs/hw/datasheets-summary/pmic-axp2101.md`
- **验证**：`axp2101_print_pmu_status()` 输出正确的 VBAT/VBUS/充电状态
- **风险**：**⚠️ R2 — 14 个寄存器（0x14/0x15/0x16/0x22/0x24/0x27/0x50/0x61/0x62/0x63/0x64/0x90/0x93）未逐一核对规格书**，CCA 写驱动时必须逐个比对

### 3.3 p4c5_display（ST7102 LCD + ST7123 触摸）

- **要做什么**：复用 `esp_lcd_st7102` + `lvgl_st7102_display` 两个组件。MIPI-DSI 2-lane 初始化前**必须先** `esp_ldo_acquire_channel(3, 2500)` 给 PHY 供电。
- **参考 xiaozhi**：
  - `components/esp_lcd_st7102/esp_lcd_st7102_mipi.c`（LCD 驱动）
  - `components/lvgl_st7102_display/ksdiy_example_display.c`（LVGL 显示端口）
  - `components/lvgl_st7102_display/ksdiy_lvgl_port.c`（LVGL 触摸集成）
  - `main/boards/kevin-p4c5-4g/st7102_init_cmds.h`（初始化命令序列）
- **本项目文件**：`components/p4c5_display/`（拷贝上述组件 + 移植显示逻辑）
- **依赖规格书**：ST7102 规格书（xiaozhi 组件内含）+ `esp_lcd_touch_st7123`（Espressif 注册表）
- **验证**：LVGL demo 显示 + 触摸坐标正确（480×800 竖屏）
- **风险**：MIPI DSI PHY LDO 通道号（3）必须正确，否则屏幕不亮且无报错

### 3.4 p4c5_audio（ES8311 + ES7210 + NS4150B）

- **要做什么**：用 `espressif/esp_audio_codec` 库的 `BoxAudioCodec` 模式——不需要裸写 ES8311/ES7210 寄存器。传入 I2C 总线句柄 + I2S 引脚 + PA 引脚即可。
- **参考 xiaozhi**：
  - `main/audio/codecs/box_audio_codec.cc` + `.h`（核心：封装 ES8311+ES7210+I2S）
  - `main/audio/audio_codec.cc` + `.h`（AudioCodec 抽象基类）
  - `main/audio/processors/afe_audio_processor.cc`（AEC 回声消除 + 4 麦波束成形）
- **本项目文件**：`components/p4c5_audio/`（移植 box_audio_codec + AFE）
- **依赖规格书**：`docs/hw/datasheets-summary/audio-codecs.md`（引脚/采样率/通道全在里面）
- **验证**：播放 1kHz WAV → 扬声器有声；对麦克风说话 → 录音回放清晰
- **关键参数**：采样率 24kHz、I2S 引脚 MCLK=13/BCLK=12/WS=10/DOUT=9/DIN=11、PA_EN=GPIO3

### 3.5 p4c5_4g（ML307C 4G 模组）

- **要做什么**：用 `78/esp-ml307` v3.6.4 封装——不需要裸 AT 指令。继承 `Ml307Board` 模式，配置 TX/RX/POWER 引脚。上电前先确认 AXP2101 ALDO4 = 2.9V 已输出。
- **参考 xiaozhi**：
  - `main/boards/common/ml307_board.cc` + `.h`（Ml307Board 类：StartNetwork/GetNetwork）
  - `main/boards/common/dual_network_board.cc` + `.h`（WiFi↔4G 切换）
  - `config.h`：`ML307_TX_PIN=GPIO53, ML307_RX_PIN=GPIO52, ML307_POWER_PIN=GPIO4`
- **本项目文件**：`components/p4c5_4g/`（封装 esp-ml307 + failover 逻辑）
- **依赖规格书**：`docs/hw/datasheets-summary/ml307c.md` + `docs/hw/datasheets/ml307c/`（14 份 AT 文档）
- **验证**：`AT+CSQ` 返回信号值 > 10；`AT+MIPCALL=1,1` 返回 IP 地址
- **风险**：**⚠️ R4 — ALDO4 = 2.9V vs ML307C 规格书 3.4-4.2V 冲突**，M4 第一阶段必须实机验证供电是否足够

### 3.6 p4c5_imu（LSM6DS3 6 轴）

- **要做什么**：用 `cpp_bus_driver` v1.1.0 库（xiaozhi 已验证 on esp32p4）。读 WHO_AM_I (0x0F) 确认芯片型号，配置加速度/陀螺仪输出率。
- **参考 xiaozhi**：`idf_component.yml` 中 `llgok/cpp_bus_driver: 1.1.0`（target=esp32p4）
- **本项目文件**：`components/p4c5_imu/`（封装 cpp_bus_driver + 横竖屏旋转逻辑）
- **依赖规格书**：`docs/hw/datasheets-summary/peripherals.md` §1
- **验证**：I2C 扫描到 0x6A；读 WHO_AM_I 返回 0x6A；倾斜板子看到加速度变化
- **风险**：**⚠️ R3 — LSM6DS3 芯片版本差异**（原理图标 TR-C，规格书是 LSM），实机读 WHO_AM_I 确认

### 3.7 p4c5_led（WS2812 RGB LED ×4）

- **要做什么**：用 `espressif/led_strip` v3.0.2，配置 RMT 通道驱动 GPIO21。4 颗 LED 级联，实现呼吸灯/状态指示/颜色编码。
- **参考 xiaozhi**：`main/led/gpio_led.cc` + `.h`（单色 LED 控制）、`main/led/circular_strip.cc`（RGB LED 灯环控制逻辑参考）
- **本项目文件**：`components/p4c5_led/`（封装 led_strip + 状态机）
- **依赖规格书**：`docs/hw/datasheets-summary/peripherals.md` §2
- **验证**：LED 依次变红→绿→蓝→白；亮度可调
- **风险**：WS2812 需 5V 供电 vs GPIO 3.3V 电平——原理图是否已做电平转换需实机确认

### 3.8 p4c5_storage（SDMMC SD 卡）

- **要做什么**：用 ESP-IDF 内置 `esp_vfs_fat_sdmmc_mount()` API，配置 SDMMC 4-bit 模式。用于日志持久化 + LVGL 资源 + OTA 缓存。
- **参考 xiaozhi**：无直接参考（xiaozhi 未用 SD 卡）——需参考 ESP-IDF examples `storage/sd_card/sdmmc`
- **本项目文件**：`components/p4c5_storage/`（封装 SD 卡挂载/卸载 + 文件系统 API）
- **依赖规格书**：`docs/hw/datasheets-summary/peripherals.md` §6（SDMMC 4-bit 引脚定义）
- **验证**：`fopen("/sdcard/test.txt", "w")` 写入成功；`ls /sdcard/` 可见文件
- **风险**：SDMMC 引脚专用，4-bit 模式占用 6 个 GPIO，确认与其他外设无冲突

### 3.9 p4c5_lvgl（LVGL UI 框架）

- **要做什么**：集成 LVGL v9.4.0 + `esp_lvgl_port` v2.7.0，实现开机画面 + 状态栏（电量/信号/时间）+ 语音交互界面骨架（对话气泡 + 表情动画）。
- **参考 xiaozhi**：
  - `main/display/lcd_display.cc` + `.h`（LCD 显示抽象）
  - `main/display/lvgl_display/lvgl_display.cc`（LVGL 显示管理）
  - `main/display/lvgl_display/lvgl_theme.cc`（主题配置）
  - `main/display/emote_display.cc`（表情动画）
- **本项目文件**：`components/p4c5_lvgl/`（移植显示逻辑 + 自定义 UI 页面）
- **依赖规格书**：`p4c5-spec.md` §2（屏幕参数 480×800 RGB565）
- **验证**：LVGL benchmark 跑通（FPS ≥ 30）；自定义 UI 页面可切换
- **风险**：DMA2D 加速必须开启（`use_dma2d = true`），否则 FPS 低于 20

### 3.10 dsh_client（DSH 协议层 — 复用 OMT）

- **要做什么**：**100% 复用 OMT** 的 14 类帧格式协议层（FRAME_HELLO/AUTH/TEXT/VOICE/IMAGE/FILE/CONTROL/STATUS/EVENT/ACK/ERROR/PING/PONG/BINARY）。底层传输走 WebSocket（通过 esp-ml307 TCP 或 esp_http_client）。
- **参考 OMT**：`/Volumes/ZT-1T/项目开发/OMT/hardware/tab5-adapter/`（PC 端 Adapter）+ OMT 设备端协议代码
- **参考 xiaozhi**：`main/protocols/websocket_protocol.cc` + `.h`（WebSocket 客户端参考）
- **本项目文件**：`components/dsh_client/`（从 OMT 移植 + 适配 esp-ml307 传输层）
- **依赖规格书**：`ARCHITECTURE.md` §3.2（14 类帧格式定义）
- **验证**：连本地 DSH 模拟器 → FRAME_HELLO 发出 → FRAME_AUTH 通过 → FRAME_TEXT 收发正常
- **风险**：WebSocket 通过 4G 走 PPP 拨号，需确认 MTU 不超限（Cat.1 MTU ≈ 1500）

---

## 4. 真机验证清单

### 4.1 每个组件的验证步骤

| 组件 | 电压测点 | 编译命令 | 烧录命令 | 串口预期输出 | 关键风险 |
|---|---|---|---|---|---|
| **p4c5_board** | DCDC1=3.3V, ALDO1=1.8V, ALDO3=3.3V, ALDO4=2.9V | `idf.py build` | `idf.py flash monitor` | `I ... Board initialized` | 初始化顺序 |
| **p4c5_pmic** | VBAT 端子电压（3.0-4.2V） | 同上 | 同上 | `AXP2101 PMU Status: ...` | **R2: 14 寄存器** |
| **p4c5_display** | MIPI PHY LDO=2.5V | 同上 | 同上 | `LCD init done, display on` | PHY LDO 通道 |
| **p4c5_audio** | ALDO3=3.3V (codec 供电) | 同上 | 同上 | `Audio codec ready, sample rate=24000` | I2S 时序 |
| **p4c5_4g** | ALDO4=2.9V (VBAT) | 同上 | 同上 | `ML307 powered on, signal=XX` | **R4: 电压冲突** |
| **p4c5_imu** | ALDO3=3.3V | 同上 | 同上 | `IMU WHO_AM_I=0x6A` | **R3: 芯片版本** |
| **p4c5_led** | GPIO21=3.3V (DATA) | 同上 | 同上 | `LED strip init, 4 leds` | 电平转换 |
| **p4c5_storage** | 3.3V (SD 卡 VDD) | 同上 | 同上 | `SD card mounted, size=XXXMB` | 引脚冲突 |
| **p4c5_lvgl** | — | 同上 | 同上 | `LVGL benchmark: XX FPS` | DMA2D 加速 |
| **dsh_client** | — | 同上 | 同上 | `WebSocket connected, FRAME_HELLO sent` | MTU 限制 |

### 4.2 关键风险真机检查（来自可信度评估报告）

| 风险 ID | 描述 | 验证方法 | 通过标准 |
|---|---|---|---|
| **R2** | AXP2101 14 寄存器未核对 | 逐个读回写入值 + 规格书比对 | 所有寄存器读回值 = 写入值 |
| **R3** | LSM6DS3 芯片版本不一致 | 读 WHO_AM_I + 看丝印 | WHO_AM_I=0x6A，丝印与驱动匹配 |
| **R4** | ML307C VBAT = 2.9V vs 规格书 3.4-4.2V | 万用表测 VBAT + 4G 拨号测试 | 拨号成功且信号稳定 |
| **R5** | GPIO4 冲突（CH343P vs ML307 POWER）| 验证调试串口走 USB-C CDC | 串口可收发 + GPIO4 可控 ML307 |
| **R6** | 0% 实测 | 每 commit 前真机验证 | 无"未实测"参数进入 release |
| **R7** | 4G 流量费用 | 查运营商流量统计 | 使用定向流量包，1h 流量 < 50MB |

---

## 5. 编译验证流程

### 5.1 整体编译

```bash
# 必须条件：
# 1. IDF >= 5.5.5（当前 5.4.2 需升级）
# 2. target = esp32p4
# 3. 所有 idf_component.yml 依赖下载成功

idf.py set-target esp32p4
idf.py build
# 通过标准：0 error, 0 warning（warning 也视为需修复）
```

### 5.2 单元测试

```bash
# 每个组件的 test/ 目录下放 Unity 测试
idf.py test
# 必须测试的用例：
# - p4c5_pmic: AXP2101 I2C 通信（mock I2C）
# - p4c5_audio: I2S 通道创建（mock I2S）
# - p4c5_4g: AT 指令解析（mock UART）
# - dsh_client: 14 类帧编解码（纯逻辑测试）
```

### 5.3 静态分析

```bash
# 方式一：IDF 内置 cppcheck
idf.py cppcheck

# 方式二：clang-tidy（如果已安装）
find components/ -name "*.cc" | xargs clang-tidy --checks='*,-llvmlibc-*'
# 通过标准：无内存泄漏、无未初始化变量、无数组越界
```

### 5.4 烧录 + 串口监控

```bash
# 烧录 P4 主控
idf.py -p /dev/ttyACM0 flash monitor

# 烧录 C5 协处理器（WiFi，单独项目）
cd components/esp_hosted/slave
idf.py set-target esp32c5
idf.py -p /dev/ttyACM1 flash

# 监控要点：
# - 启动时间 < 5s（从上电到 LVGL 开机画面）
# - 4G 拨号时间 < 30s（冷启动）
# - 内存使用 < 80% PSRAM（32MB × 80% = 25.6MB）
# - 无 panic / assert / guru meditation error
```

---

## 6. M4 → M5 过渡

### 6.1 M4 完成后未实现（留给 M5）

| 功能 | 状态 | M5 计划 |
|---|---|---|
| 低功耗策略（4G 休眠/WiFi 省电） | 未实现 | M5 核心任务 |
| WiFi ↔ 4G 自动 failover | 框架已建 | M5 做策略优化 |
| 24h 长稳测试 | 未开始 | M5 必须跑 |
| OTA 升级（通过 4G） | 未实现 | M5 实现 |
| 摄像头 MIPI-CSI | 暂不实现 | 视需求决定 |
| RS485 / MCP4725 | 暂不实现 | 视需求决定 |
| BLE 5.0 配网 | 框架已建 | M5 完善 |

### 6.2 低功耗优化目标（M5）

| 场景 | 目标电流 | 策略 |
|---|---|---|
| 待机（屏幕关 + 4G 休眠）| < 5mA | AXP2101 关 ALDO4 + ML307 DTR 休眠 |
| 轻量工作（屏幕亮 + WiFi）| < 200mA | 降低背光 + CPU 降频 |
| 通话（4G + 音频）| < 600mA | 优化 AEC + 减少 PA 增益 |
| 深度睡眠 | < 0.5mA | ESP32-P4 ULP + AXP2101 最小配置 |

### 6.3 24h 长稳测试计划（M5）

```
Day 1: 连续 4G 拨号 + WebSocket 连接 + 每小时发送 1 次 TEXT 帧
Day 2: 连续音频录放 + 每 30 分钟 1 次语音交互
通过标准：
  - 0 次断连（4G 重连 < 10s）
  - 0 次内存泄漏（PSRAM 使用量波动 < 5%）
  - 0 次看门狗复位
```

### 6.4 量产就绪清单（M5 完成时）

- [ ] BOM 成本核算（目标 < ¥500）
- [ ] 3D 外壳装配验证（`shell/*.stl`）
- [ ] 生产测试固件（一键检测所有外设）
- [ ] 用户手册（开机/配网/充电/故障排除）
- [ ] FCC/CE 预认证检查（4G 模组已认证则免测）

---

## 附录 A：复用的关键库清单

| 库 | 版本 | 用途 | 来源 |
|---|---|---|---|
| `espressif/esp_audio_codec` | ~2.4.1 | ES8311 + ES7210 音频编解码 | Espressif 注册表 |
| `78/esp-ml307` | ~3.6.4 | ML307C 4G 拨号/TCP/MQTT/HTTP | 78 注册表 |
| `78/esp-wifi-connect` | ~3.1.1 | WiFi 配网（BluFi）| 78 注册表 |
| `espressif/esp_hosted` | 2.12.3 | P4 ↔ C5 SDIO 通信 | Espressif 注册表 |
| `espressif/led_strip` | ~3.0.2 | WS2812 RGB LED | Espressif 注册表 |
| `lvgl/lvgl` | ~9.4.0 | UI 框架 | LVGL 注册表 |
| `llgok/cpp_bus_driver` | 1.1.0 | IMU I2C 驱动 | 注册表 |

## 附录 B：复用的 xiaozhi 组件清单

| xiaozhi 组件 | 路径 | 用途 |
|---|---|---|
| `esp_lcd_st7102` | `components/esp_lcd_st7102/` | ST7102 MIPI-DSI LCD 驱动 |
| `lvgl_st7102_display` | `components/lvgl_st7102_display/` | LVGL 显示端口 + 触摸集成 |
| `pmic_axp2101` | `components/pmic_axp2101/` | AXP2101 PMIC C API |

---

**版本**：v1.0（2026-09-06, CCB）
**下次更新**：M4 阶段实机验证后，由 CCA 补充实测参数
