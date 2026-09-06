# pmic_axp2101

KSDIY ESP32-P4 开发板 **AXP2101** 电源管理芯片驱动，用于 DCDC/LDO 供电、充电与电源按键等配置。

## 功能

- I2C 寄存器读写封装
- 多路 DCDC / LDO 电压设置
- 电池充电、预充、电源键等 AXP2101 常用功能

## 依赖

- ESP-IDF >= 5.0
- `esp_driver_i2c`

## 使用

```yaml
dependencies:
  kevincoooool/pmic_axp2101: "^1.0.0"
```

```c
#include "axp2101.h"

esp_err_t err = axp2101_init(i2c_bus_handle);
```

通常在 `ksdiy_lvgl_port_init()` 之前完成 AXP 上电，或与触摸共用同一 I2C 总线（GPIO7/8）。

## 上传注册库

```bash
cd components/pmic_axp2101
compote registry login
compote component upload --namespace kevincoooool --name pmic_axp2101 --version 1.0.0
```
