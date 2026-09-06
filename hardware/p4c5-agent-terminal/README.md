# p4c5-agent-terminal 固件

基于 ESP-IDF 的便携式 AI Agent 远程终端固件，运行在 ESP32-P4C5 酷世 DIY 开发板上。

## 项目目标

- 便携版 DSH —— 户外/路上/沙发上，用语音 + 触屏指挥家里 PC 上的 DSH Agent 干活
- 硬件平台：ESP32-P4 主控 + ESP32-C5 WiFi + ST7102 480×800 MIPI-DSI 屏 + ML307C 4G
- 协议栈：WebSocket + 14 类帧（沿用 OMT）

## 快速开始

### 编译

```bash
cd hardware/p4c5-agent-terminal

# 设置 ESP-IDF 环境（需 v5.5.5+）
export IDF_PATH=/Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5
source $IDF_PATH/export.sh

# 设置 target 并编译
idf.py set-target esp32p4
idf.py build
```

### 烧录

```bash
# 方法 1: 使用 idf.py
idf.py -p /dev/cu.usbmodem1401 flash

# 方法 2: 使用脚本
bash scripts/flash.sh /dev/cu.usbmodem1401
```

### 监视串口

```bash
idf.py -p /dev/cu.usbmodem1401 monitor
```

## 组件架构

```
hardware/p4c5-agent-terminal/
├── CMakeLists.txt          # 顶层构建
├── partitions.csv          # 16MB Flash 分区表
├── sdkconfig.defaults      # 默认配置
├── main/
│   ├── app_main.c          # 入口
│   ├── config.h            # 引脚定义（基于 p4c5-pins.csv）
│   ├── idf_component.yml   # 依赖清单
│   └── Kconfig.projbuild   # menuconfig 选项
├── components/
│   ├── p4c5_board/         # 板级统一初始化（PMIC→Display→Audio→4G）
│   ├── p4c5_audio/         # ES8311(DAC) + ES7210(ADC) + NS4150B(PA)
│   ├── p4c5_display/       # ST7102 MIPI-DSI LCD + ST7123 触摸
│   ├── p4c5_pmic/          # AXP2101 电源管理
│   └── p4c5_4g/            # ML307C Cat.1 4G 模组
└── scripts/
    └── flash.sh            # 一键烧录
```

## 第三方依赖

| 组件 | 版本 | 用途 | 来源 |
|---|---|---|---|
| espressif/esp_audio_codec | ~2.4.1 | ES8311/ES7210 codec | ESP Component Registry |
| 78/esp-ml307 | ~3.6.4 | ML307C 4G 协议栈 | ESP Component Registry |
| 78/esp-wifi-connect | ~3.1.1 | WiFi 连接管理 | ESP Component Registry |
| lvgl/lvgl | ~9.4.0 | 图形库 | ESP Component Registry |
| esp_lvgl_port | ~2.7.0 | LVGL 适配层 | ESP Component Registry |
| espressif/esp_lcd_touch_st7123 | ^1.0.0 | 触摸驱动 | ESP Component Registry |
| espressif/esp_hosted | 2.12.3 | P4↔C5 WiFi 协处理 | ESP Component Registry |
| esp_lcd_st7102 | (path) | LCD 驱动 | xiaozhi components/ |
| pmic_axp2101 | (path) | PMIC 驱动 | xiaozhi components/ |

## xiaozhi 参考对照

| 本组件 | xiaozhi 对应文件 |
|---|---|
| config.h | main/boards/kevin-p4c5-4g/config.h |
| p4c5_audio.cc | main/audio/codecs/box_audio_codec.cc |
| p4c5_display.cc | main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc (InitializeSt7102Display) |
| p4c5_pmic.cc | main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc (Pmic class) |
| p4c5_4g.cc | main/boards/common/ml307_board.cc |
| esp_lcd_st7102 | components/esp_lcd_st7102/ (直接复制) |
| pmic_axp2101 | components/pmic_axp2101/ (直接复制) |

## M4 待办

各组件标记为 `TODO` 的未实现功能：

### p4c5_audio
- [ ] `p4c5_audio_set_i2c_bus()` — I2C handle 注入（当前为 NULL）
- [ ] `p4c5_audio_deinit()` — 完整资源释放
- [ ] AEC 参考通道实际连通测试

### p4c5_display
- [ ] ST7102 init cmds 加载（需从 xiaozhi `st7102_init_cmds.h` 复制）
- [ ] ST7123 触摸完整初始化（需 i2c_bus handle）
- [ ] LVGL port 集成（`esp_lvgl_port` 初始化 + display/touch 注册）

### p4c5_pmic
- [ ] `pmic_write_reg()` / `pmic_read_reg()` — I2C 直写实现
- [ ] `p4c5_pmic_get_battery_level()` — AXP2101 0xA0 读取
- [ ] `p4c5_pmic_is_charging()` / `is_discharging()` — 状态位读取

### p4c5_4g
- [ ] esp-ml307 高层 API 集成（网络注册、TCP/IP 栈）
- [ ] `p4c5_4g_tcp_connect/send/recv/close` — 完整实现
- [ ] AT+CSQ / AT+CPIN? 响应解析
- [ ] ⚠️ ALDO4=2.9V vs ML307C 3.4-4.2V 实测验证

### p4c5_board
- [ ] 各子系统 init 失败后的优雅降级
- [ ] I2C 设备扫描（启动时打印所有设备地址）

## 许可证

Apache 2.0
