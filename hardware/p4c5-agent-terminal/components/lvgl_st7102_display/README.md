# lvgl_st7102_display

KSDIY **4.3 寸 MIPI 屏（ST7102 480×800）** + **ST7123 触摸** + **LVGL 9** 移植层，集成 AXP2101 上电与防撕裂显示。

## 功能

- `ksdiy_lvgl_port_init()`：DSI 屏、触摸、LVGL 适配器一键初始化
- `ksdiy_lvgl_lock()` / `ksdiy_lvgl_unlock()`：线程安全访问 LVGL
- `touch_i2c_bus_`：触摸/AXP/CSI SCCB 可复用 I2C 总线句柄
- `ksdiy_example_display_bootstrap()`：启动阶段三行状态文字（可选）

## 依赖

- ESP-IDF >= 5.0
- LVGL 8.x / 9.x
- `espressif/esp_lvgl_adapter`
- `espressif/esp_lcd_touch_st7123`
- `kevincoooool/esp_lcd_st7102`
- `kevincoooool/pmic_axp2101`

## 使用

```yaml
dependencies:
  kevincoooool/lvgl_st7102_display: "^1.0.0"
```

```c
#include "lvgl_st7102_port.h"

nvs_flash_init();
ksdiy_lvgl_port_init();

if (ksdiy_lvgl_lock(100)) {
    /* 创建 LVGL 控件 */
    ksdiy_lvgl_unlock();
}
```

工程 `CMakeLists.txt` 中仍需将组件目录加入 `EXTRA_COMPONENT_DIRS`，或通过 Component Manager 拉取上述依赖。

## 本地同仓库开发

```yaml
  kevincoooool/lvgl_st7102_display:
    version: "^1.0.0"
    override_path: "../../components/lvgl_st7102_display"
```

目录内 `ESP-IDF/` 为历史参考工程，**不会**打入注册库包（已在 `idf_component.yml` 中 exclude）。

## 上传注册库

```bash
cd components/lvgl_st7102_display
compote registry login
compote component upload --namespace kevincoooool --name lvgl_st7102_display --version 1.0.0
```

> 建议先上传 `esp_lcd_st7102`、`pmic_axp2101`，再上传本组件。
