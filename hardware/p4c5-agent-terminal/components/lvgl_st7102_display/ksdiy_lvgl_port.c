/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ksdiy_lvgl_port.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7102.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "esp_lv_adapter.h"
#include "axp2101.h"
#include "freertos/task.h"
#include "ksdiy_lvgl_font.h"

static const char *TAG = "ksdiy_lvgl";

#define KSDIY_LCD_H_RES                    480
#define KSDIY_LCD_V_RES                    800
#define KSDIY_LCD_BITS_PER_PIXEL           16
#define KSDIY_LCD_MIPI_LANE_NUM            2
#define KSDIY_LCD_MIPI_LANE_BITRATE_MBPS   820
#define KSDIY_LCD_MIPI_PWR_LDO_CHAN        3
#define KSDIY_LCD_MIPI_PWR_LDO_VOLTAGE_MV  2500
#define KSDIY_LCD_RESET_GPIO               (22)
/* Backlight enable: GPIO6 + LEDC PWM (duty 0~255 via ksdiy_lcd_set_brightness) */
#define KSDIY_LCD_BK_LIGHT_GPIO            GPIO_NUM_6
#define KSDIY_LCD_BK_LIGHT_ON_LEVEL        1
#define KSDIY_LCD_BK_LIGHT_PWM_FREQ_HZ     10000
#define KSDIY_LCD_BK_LIGHT_LEDC_TIMER      LEDC_TIMER_1
#define KSDIY_LCD_BK_LIGHT_LEDC_CHANNEL    LEDC_CHANNEL_1
#define KSDIY_LCD_BK_LIGHT_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define KSDIY_LCD_BK_LIGHT_DUTY_RES        LEDC_TIMER_10_BIT
/* 与 LCD_RST 共用 GPIO22，仅由 esp_lcd_panel_reset() 复位一次 */
#define KSDIY_TOUCH_RESET_GPIO             GPIO_NUM_NC
/* GPIO_NUM_NC = esp_lv_adapter 轮询读触摸；GPIO23 = IRQ（需 INT 中断正常） */
#define KSDIY_TOUCH_INT_GPIO               GPIO_NUM_23
#define KSDIY_TOUCH_I2C_PORT               I2C_NUM_0
#define KSDIY_TOUCH_I2C_SCL                8
#define KSDIY_TOUCH_I2C_SDA                7
#define KSDIY_TOUCH_I2C_SPEED_HZ           400000

#if LV_COLOR_DEPTH == 16
#define KSDIY_MIPI_DPI_PX_FORMAT LCD_COLOR_PIXEL_FORMAT_RGB565
#elif LV_COLOR_DEPTH == 24
#define KSDIY_MIPI_DPI_PX_FORMAT LCD_COLOR_PIXEL_FORMAT_RGB888
#else
#error "Unsupported LV_COLOR_DEPTH for ST7102 MIPI panel"
#endif

static const st7102_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0x99, (uint8_t []){0x71, 0x02, 0xa2}, 3, 0},
    {0x99, (uint8_t []){0x71, 0x02, 0xa3}, 3, 0},
    {0x99, (uint8_t []){0x71, 0x02, 0xa4}, 3, 0},
    {0xB0, (uint8_t []){0x22, 0x57, 0x1E, 0x61, 0x2F, 0x57, 0x61}, 7, 0},
    {0xB7, (uint8_t []){0x64, 0x64}, 2, 0},
    {0xBF, (uint8_t []){0xB4, 0xB4}, 2, 0},
    {0xC8, (uint8_t []){0x00, 0x00, 0x13, 0x24, 0x44, 0x00, 0x74, 0x03, 0xB8, 0x04,
                        0x11, 0x16, 0x08, 0x86, 0x04, 0x21, 0xD3, 0x02, 0x10, 0x0F,
                        0x22, 0x4D, 0x0E, 0x90, 0x09, 0x32, 0xF0, 0x0B, 0x40, 0x0E,
                        0xF3, 0x7D, 0x0E, 0xA9, 0xBF, 0x03, 0xC4}, 37, 0},
    {0xC9, (uint8_t []){0x00, 0x00, 0x13, 0x24, 0x44, 0x00, 0x74, 0x03, 0xB8, 0x04,
                        0x11, 0x16, 0x08, 0x86, 0x04, 0x21, 0xD3, 0x02, 0x10, 0x0F,
                        0x22, 0x4D, 0x0E, 0x90, 0x09, 0x32, 0xF0, 0x0B, 0x40, 0x0E,
                        0xF3, 0x7D, 0x0E, 0xA9, 0xBF, 0x03, 0xC4}, 37, 0},
    {0xD7, (uint8_t []){0x10, 0x0C, 0x36, 0x19, 0x90, 0x90}, 6, 0},
    {0xA3, (uint8_t []){0x51, 0x03, 0x80, 0xCF, 0x44, 0x00, 0x00, 0x00, 0x00, 0x04,
                        0x78, 0x78, 0x00, 0x1A, 0x00, 0x45, 0x05, 0x00, 0x00, 0x00,
                        0x00, 0x46, 0x00, 0x00, 0x02, 0x20, 0x52, 0x00, 0x05, 0x00,
                        0x00, 0xFF}, 32, 0},
    {0xA6, (uint8_t []){0x02, 0x00, 0x24, 0x55, 0x35, 0x00, 0x38, 0x00, 0x78, 0x78,
                        0x00, 0x24, 0x55, 0x36, 0x00, 0x37, 0x00, 0x78, 0x78, 0x02,
                        0xAC, 0x51, 0x3A, 0x00, 0x00, 0x00, 0x78, 0x78, 0x03, 0xAC,
                        0x21, 0x00, 0x04, 0x00, 0x00, 0x78, 0x78, 0x3e, 0x00, 0x06,
                        0x00, 0x00, 0x00, 0x00}, 44, 0},
    {0xA7, (uint8_t []){0x19, 0x19, 0x00, 0x64, 0x40, 0x07, 0x16, 0x40, 0x00, 0x04,
                        0x03, 0x78, 0x78, 0x00, 0x64, 0x40, 0x25, 0x34, 0x00, 0x00,
                        0x02, 0x01, 0x78, 0x78, 0x00, 0x64, 0x40, 0x4B, 0x5A, 0x00,
                        0x00, 0x02, 0x01, 0x78, 0x78, 0x00, 0x24, 0x40, 0x69, 0x78,
                        0x00, 0x00, 0x00, 0x00, 0x78, 0x78, 0x00, 0x44}, 48, 0},
    {0xAC, (uint8_t []){0x08, 0x0A, 0x11, 0x00, 0x13, 0x03, 0x1B, 0x18, 0x06, 0x1A,
                        0x19, 0x1B, 0x1B, 0x1B, 0x18, 0x1B, 0x09, 0x0B, 0x10, 0x02,
                        0x12, 0x01, 0x1B, 0x18, 0x06, 0x1A, 0x19, 0x1B, 0x1B, 0x1B,
                        0x18, 0x1B, 0xFF, 0x67, 0xFF, 0x67, 0x00}, 37, 0},
    {0xAD, (uint8_t []){0xCC, 0x40, 0x46, 0x11, 0x04, 0x78, 0x78}, 7, 0},
    {0xE8, (uint8_t []){0x30, 0x07, 0x00, 0x94, 0x94, 0x9C, 0x00, 0xE2, 0x04, 0x00,
                        0x00, 0x00, 0x00, 0xEF}, 14, 0},
    {0xE7, (uint8_t []){0x8B, 0x3C, 0x00, 0x0C, 0xF0, 0x5D, 0x00, 0x5D, 0x00, 0x5D,
                        0x00, 0x5D, 0x00, 0xFF, 0x00, 0x08, 0x7B, 0x00, 0x00, 0xC8,
                        0x6A, 0x5A, 0x08, 0x1A, 0x3C, 0x00, 0x81, 0x01, 0xCC, 0x01,
                        0x7F, 0xF0, 0x22}, 33, 0},
    {0x11, (uint8_t []){0x00}, 0, 600},
    {0x29, (uint8_t []){0x00}, 0, 120},
};

static bool s_initialized;
static bool s_panel_ready;
static bool s_touch_ready;
static bool s_bklight_ready;
static uint8_t s_bklight_brightness = 255;
static uint32_t s_bklight_max_duty;
static ksdiy_lvgl_display_t *s_display;
static i2c_master_bus_handle_t s_i2c_bus;
static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_io_handle;
static esp_lcd_touch_handle_t s_touch_handle;
i2c_master_bus_handle_t touch_i2c_bus_;

static void ksdiy_enable_dsi_phy_power(void)
{
    static esp_ldo_channel_handle_t s_ldo_mipi_phy;

    if (s_ldo_mipi_phy != NULL) {
        return;
    }

    const esp_ldo_channel_config_t ldo_config = {
        .chan_id = KSDIY_LCD_MIPI_PWR_LDO_CHAN,
        .voltage_mv = KSDIY_LCD_MIPI_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &s_ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY power enabled");
}

static void ksdiy_i2c_init(void)
{
    if (s_i2c_bus != NULL) {
        return;
    }

    const i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = KSDIY_TOUCH_I2C_PORT,
        .sda_io_num = KSDIY_TOUCH_I2C_SDA,
        .scl_io_num = KSDIY_TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &s_i2c_bus));
    touch_i2c_bus_ = s_i2c_bus;
    ESP_LOGI(TAG, "I2C bus initialized");
}

/**
 * AXP2101 与触摸/codec 共用本组件创建的 I2C 总线；上电轨只在此处初始化一次，
 * 避免音频组件再挂一次 I2C 设备或重复配置 PMIC。
 */
static void pmic_axp2101_init_codec_rails(void)
{
    if (touch_i2c_bus_ == NULL) {
        return;
    }

    esp_err_t err = axp2101_init(touch_i2c_bus_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 I2C device add failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "AXP2101 initialized");
    if (axp2101_check_chip_id() != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 chip id mismatch, skip rail config");
        return;
    }
    ESP_LOGI(TAG, "AXP2101 chip id checked");
    ESP_ERROR_CHECK(axp2101_set_dcdc_voltage(AXP2101_DCDC1, 3.3f));
    ESP_ERROR_CHECK(axp2101_set_dcdc_enabled(AXP2101_DCDC1, true));
    ESP_ERROR_CHECK(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO1, 1.8f));
    ESP_ERROR_CHECK(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO1, true));
    ESP_ERROR_CHECK(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO3, 3.3f));
    ESP_ERROR_CHECK(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO3, true));
    ESP_ERROR_CHECK(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f));
    ESP_ERROR_CHECK(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO4, true));
    ESP_LOGI(TAG, "AXP2101 rails set");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "AXP2101 rails ready (DCDC1 / ALDO3 for codec)");
}

static void ksdiy_touch_get_rotation_flags(esp_lv_adapter_rotation_t rotation, bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    bool swap = false;
    bool x_mirror = false;
    bool y_mirror = false;

    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        swap = true;
        x_mirror = false;
        y_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        swap = false;
        x_mirror = true;
        y_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        swap = true;
        x_mirror = true;
        y_mirror = false;
        break;
    case ESP_LV_ADAPTER_ROTATE_0:
    default:
        swap = false;
        x_mirror = false;
        y_mirror = false;
        break;
    }

    *swap_xy = swap;
    *mirror_x = x_mirror;
    *mirror_y = y_mirror;
}

bool ksdiy_lvgl_lock(int timeout_ms)
{
    if (timeout_ms == 0) {
        timeout_ms = -1;
    }
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void ksdiy_lvgl_unlock(void)
{
    esp_lv_adapter_unlock();
}

ksdiy_lvgl_display_t *ksdiy_lvgl_get_display(void)
{
    return s_display;
}

esp_lcd_panel_handle_t ksdiy_lvgl_get_panel_handle(void)
{
    return s_panel_handle;
}

esp_lcd_touch_handle_t ksdiy_touch_get_handle(void)
{
    return s_touch_handle;
}

bool ksdiy_lvgl_is_ready(void)
{
    return s_initialized;
}

static void ksdiy_panel_hw_init(void)
{
    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = KSDIY_LCD_MIPI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = KSDIY_LCD_MIPI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    const esp_lcd_dbi_io_config_t dbi_config = ST7102_MIPI_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &s_io_handle));

    esp_lcd_dpi_panel_config_t dpi_config = ST7102_MIPI_480_800_PANEL_60HZ_DPI_CONFIG(KSDIY_MIPI_DPI_PX_FORMAT);
    dpi_config.num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_avoid_mode, rotation);

    const st7102_vendor_config_t vendor_config = {
        .init_cmds = s_lcd_init_cmds,
        .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = KSDIY_LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7102(s_io_handle, &panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));
}

esp_err_t ksdiy_lcd_backlight_init(void)
{
    if (s_bklight_ready) {
        return ESP_OK;
    }

    s_bklight_max_duty = (1U << KSDIY_LCD_BK_LIGHT_DUTY_RES) - 1U;

    const ledc_timer_config_t timer_cfg = {
        .speed_mode = KSDIY_LCD_BK_LIGHT_LEDC_MODE,
        .duty_resolution = KSDIY_LCD_BK_LIGHT_DUTY_RES,
        .timer_num = KSDIY_LCD_BK_LIGHT_LEDC_TIMER,
        .freq_hz = KSDIY_LCD_BK_LIGHT_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight LEDC timer failed: %s", esp_err_to_name(err));
        return err;
    }

    const ledc_channel_config_t ch_cfg = {
        .speed_mode = KSDIY_LCD_BK_LIGHT_LEDC_MODE,
        .channel = KSDIY_LCD_BK_LIGHT_LEDC_CHANNEL,
        .timer_sel = KSDIY_LCD_BK_LIGHT_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = KSDIY_LCD_BK_LIGHT_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight LEDC channel GPIO%d failed: %s",
                 (int)KSDIY_LCD_BK_LIGHT_GPIO, esp_err_to_name(err));
        return err;
    }

    s_bklight_ready = true;
    ESP_LOGI(TAG, "Backlight PWM on GPIO%d (%d Hz, LEDC T%d CH%d)",
             (int)KSDIY_LCD_BK_LIGHT_GPIO, KSDIY_LCD_BK_LIGHT_PWM_FREQ_HZ,
             (int)KSDIY_LCD_BK_LIGHT_LEDC_TIMER, (int)KSDIY_LCD_BK_LIGHT_LEDC_CHANNEL);
    return ksdiy_lcd_set_brightness(s_bklight_brightness ? s_bklight_brightness : 255);
}

esp_err_t ksdiy_touch_bare_init(void)
{
    if (s_touch_ready || s_touch_handle != NULL) {
        return ESP_OK;
    }
    if (s_i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not ready for touch");
        return ESP_ERR_INVALID_STATE;
    }

    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    ksdiy_touch_get_rotation_flags(rotation, &swap_xy, &mirror_x, &mirror_y);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    tp_io_config.scl_speed_hz = KSDIY_TOUCH_I2C_SPEED_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_config, &tp_io_handle),
                        TAG, "touch panel IO create failed");

    const esp_lcd_touch_config_t touch_config = {
        .x_max = KSDIY_LCD_H_RES,
        .y_max = KSDIY_LCD_V_RES,
        .rst_gpio_num = KSDIY_TOUCH_RESET_GPIO,
        .int_gpio_num = KSDIY_TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_st7123(tp_io_handle, &touch_config, &s_touch_handle),
                        TAG, "ST7123 create failed");

    s_touch_ready = true;
    ESP_LOGI(TAG, "ST7123 touch ready (I2C SCL=%d SDA=%d RST=%d INT=%d)",
             KSDIY_TOUCH_I2C_SCL, KSDIY_TOUCH_I2C_SDA,
             (int)KSDIY_TOUCH_RESET_GPIO, (int)KSDIY_TOUCH_INT_GPIO);
    return ESP_OK;
}

esp_err_t ksdiy_lcd_set_brightness(uint8_t brightness)
{
    if (!s_bklight_ready) {
        esp_err_t err = ksdiy_lcd_backlight_init();
        if (err != ESP_OK) {
            return err;
        }
    }

    s_bklight_brightness = brightness;

    uint32_t duty = ((uint32_t)brightness * s_bklight_max_duty) / 255U;
    if (!KSDIY_LCD_BK_LIGHT_ON_LEVEL) {
        duty = s_bklight_max_duty - duty;
    }

    esp_err_t err = ledc_set_duty(KSDIY_LCD_BK_LIGHT_LEDC_MODE,
                                  KSDIY_LCD_BK_LIGHT_LEDC_CHANNEL, duty);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(KSDIY_LCD_BK_LIGHT_LEDC_MODE, KSDIY_LCD_BK_LIGHT_LEDC_CHANNEL);
}

uint8_t ksdiy_lcd_get_brightness(void)
{
    return s_bklight_brightness;
}

void ksdiy_panel_bare_init(void)
{
    if (s_panel_ready) {
        return;
    }

    (void)ksdiy_lcd_backlight_init();
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << KSDIY_LCD_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(KSDIY_LCD_RESET_GPIO, 1);
    ksdiy_enable_dsi_phy_power();
    ksdiy_i2c_init();
    pmic_axp2101_init_codec_rails();
    ksdiy_panel_hw_init();
    ESP_ERROR_CHECK(ksdiy_touch_bare_init());
    s_panel_ready = true;
}

void ksdiy_lvgl_port_init(void)
{
    if (s_initialized) {
        return;
    }
    ksdiy_panel_bare_init();

    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 40 * 1024;
    adapter_config.task_priority = 10;
    adapter_config.stack_in_psram = false;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
        s_panel_handle, s_io_handle, KSDIY_LCD_H_RES, KSDIY_LCD_V_RES, rotation);
    s_display = esp_lv_adapter_register_display(&display_config);
    if (s_display == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL display");
        abort();
    }

    ESP_ERROR_CHECK(ksdiy_touch_bare_init());

    const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_display, s_touch_handle);
    if (esp_lv_adapter_register_touch(&touch_cfg) == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL touch");
        abort();
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_theme_t *theme = lv_theme_default_init(
            s_display,
            lv_palette_main(LV_PALETTE_BLUE),
            lv_palette_main(LV_PALETTE_RED),
            true,
            KSDIY_UI_FONT);
#if LVGL_VERSION_MAJOR >= 9
        lv_display_set_theme(s_display, theme);
#else
        lv_disp_set_theme(s_display, theme);
#endif
        esp_lv_adapter_unlock();
        ESP_LOGI(TAG, "default LVGL theme font: myFont (KSDIY)");
    }

    s_initialized = true;
}
