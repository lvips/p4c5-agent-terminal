# esp_lcd_st7102

ST7102 液晶屏驱动组件，支持 **RGB 并行** 与 **MIPI DSI** 接口，适用于 ESP32-P4 等带 LCD 外设的芯片。

## 功能

- ST7102 初始化命令表可自定义
- RGB / MIPI DSI 面板创建与复位
- 与 ESP-IDF `esp_lcd` 框架对接

## 依赖

- ESP-IDF >= 5.0
- `esp_lcd`

## 使用

在工程 `main/idf_component.yml` 中添加：

```yaml
dependencies:
  kevincoooool/esp_lcd_st7102: "^1.0.0"
```

代码中参考 Espressif LCD 示例，包含头文件 `esp_lcd_st7102.h`，在 `esp_lcd_panel_dev_config_t.vendor_config` 中传入 ST7102 厂商配置。

## 本地开发（同仓库）

```yaml
  kevincoooool/esp_lcd_st7102:
    version: "^1.0.0"
    override_path: "../../components/esp_lcd_st7102"
```

## 上传注册库

```bash
cd components/esp_lcd_st7102
compote registry login
compote component upload --namespace kevincoooool --name esp_lcd_st7102 --version 1.0.0
```
