/**
 * @file p4c5_display.cc
 * @brief P4C5 显示子系统实现 — ST7102 + ST7123
 *
 * 基于 xiaozhi main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc 移植。
 * 使用 esp_lcd_st7102 组件（从 xiaozhi components/esp_lcd_st7102 复制）
 * 和 espressif/esp_lcd_touch_st7123 组件。
 *
 * 参考：
 *   - xiaozhi: kevin_p4c5_4g_board.cc InitializeSt7102Display()
 *   - xiaozhi: kevin_p4c5_4g_board.cc InitializeSt7123Touch()
 *   - xiaozhi: components/esp_lcd_st7102/
 */

#include "p4c5_display.h"
#include "p4c5_board.h"
#include "config.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_check.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_io_i2c.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_st7102.h>
#include <esp_lcd_touch_st7123.h>
#include <esp_ldo_regulator.h>

static const char* TAG = "p4c5_display";

/* ── 内部状态 ── */
static esp_lcd_panel_handle_t     s_panel = NULL;
static esp_lcd_panel_io_handle_t  s_panel_io = NULL;
static esp_lcd_touch_handle_t     s_touch = NULL;
static esp_lcd_dsi_bus_handle_t   s_dsi_bus = NULL;
static esp_ldo_channel_handle_t   s_mipi_ldo = NULL;
static uint8_t s_bl_level = 255;
static bool s_initialized = false;

/* ── ST7102 MIPI 初始化命令 ── */
/* 来源：ksdiy_lvgl_port.c（xiaozhi kevin P4C5 验证过的命令序列） */
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
    {0x11, (uint8_t []){0x00}, 0, 600},  /* Sleep Out + 600ms delay */
    {0x29, (uint8_t []){0x00}, 0, 120},  /* Display ON + 120ms delay */
};

#define ST7102_INIT_CMD_COUNT  (sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]))

/* ── 内部函数 ── */

static esp_err_t enable_mipi_phy_power(void)
{
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = P4C5_LCD_MIPI_PHY_LDO_CHAN,
        .voltage_mv = P4C5_LCD_MIPI_PHY_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &s_mipi_ldo),
                        TAG, "MIPI PHY LDO acquire failed");
    ESP_LOGI(TAG, "MIPI DSI PHY LDO: channel %d, %dmV",
             P4C5_LCD_MIPI_PHY_LDO_CHAN, P4C5_LCD_MIPI_PHY_LDO_MV);
    return ESP_OK;
}

static esp_err_t init_backlight(void)
{
    /* 使用 LEDC PWM 控制背光 */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer failed");

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = P4C5_LCD_BL_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 255,  /* 默认最亮 */
        .hpoint     = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "LEDC channel failed");
    ESP_LOGI(TAG, "Backlight PWM initialized (GPIO %d)", P4C5_LCD_BL_GPIO);
    return ESP_OK;
}

/* ── 公开 API ── */

esp_err_t p4c5_display_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing display: ST7102 %dx%d MIPI-DSI 2-lane",
             P4C5_LCD_WIDTH, P4C5_LCD_HEIGHT);

    /* 1. LCD RST */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << P4C5_LCD_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&rst_cfg), TAG, "LCD RST gpio_config failed");
    gpio_set_level(P4C5_LCD_RST_GPIO, 1);

    /* 2. MIPI PHY LDO */
    ESP_RETURN_ON_ERROR(enable_mipi_phy_power(), TAG, "MIPI PHY power failed");

    /* 3. MIPI DSI bus */
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = P4C5_LCD_MIPI_DSI_LANES,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = P4C5_LCD_MIPI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &s_dsi_bus), TAG, "DSI bus failed");

    /* 4. Panel IO (DBI) */
    esp_lcd_dbi_io_config_t dbi_cfg = ST7102_MIPI_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_cfg, &s_panel_io),
                        TAG, "Panel IO failed");

    /* 5. ST7102 panel (DPI) — 使用 xiaozhi 验证过的 DPI 配置宏 */
    esp_lcd_dpi_panel_config_t dpi_cfg = ST7102_MIPI_480_800_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_cfg.num_fbs = 1;

    /* 🔴 T13 修复: 加载 ST7102 初始化命令（之前是 TODO 空数组，导致黑屏） */
    st7102_vendor_config_t vendor_cfg = {};
    vendor_cfg.init_cmds = s_lcd_init_cmds;
    vendor_cfg.init_cmds_size = ST7102_INIT_CMD_COUNT;
    vendor_cfg.mipi_config.dsi_bus = s_dsi_bus;
    vendor_cfg.mipi_config.dpi_config = &dpi_cfg;
    vendor_cfg.flags.use_mipi_interface = 1;

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7102(s_panel_io, &panel_cfg, &s_panel),
                        TAG, "ST7102 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "Display on failed");

    /* 6. ST7123 触摸 — 🔴 T12-R11: 接入 p4c5_board 提供的 I2C bus */
    i2c_master_bus_handle_t touch_i2c_bus = (i2c_master_bus_handle_t)p4c5_board_get_i2c_bus();
    esp_lcd_panel_io_handle_t tp_io = NULL;
    if (touch_i2c_bus) {
        /* ⚠️ 使用项目 config.h 定义的地址 0x5A，而非 Espressif 默认的 0x55
         * 酷世 P4C5 开发板 ST7123 地址 = 0x5A (见 p4c5_board.cc 注释) */
        esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
        tp_io_cfg.dev_addr = P4C5_TP_I2C_ADDR;   /* 0x5A */
        tp_io_cfg.scl_speed_hz = 400000;

        esp_err_t tp_io_err = esp_lcd_new_panel_io_i2c(touch_i2c_bus, &tp_io_cfg, &tp_io);
        if (tp_io_err == ESP_OK) {
            esp_lcd_touch_config_t tp_cfg = {
                .x_max = P4C5_LCD_WIDTH,
                .y_max = P4C5_LCD_HEIGHT,
                .rst_gpio_num = GPIO_NUM_NC,
                .int_gpio_num = P4C5_TP_INT_GPIO,
                .levels = { .reset = 0, .interrupt = 0 },
                .flags = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
            };
            esp_err_t tp_err = esp_lcd_touch_new_i2c_st7123(tp_io, &tp_cfg, &s_touch);
            if (tp_err == ESP_OK) {
                ESP_LOGI(TAG, "ST7123 touch initialized ✅ (addr=0x%02X)", P4C5_TP_I2C_ADDR);
            } else {
                ESP_LOGW(TAG, "ST7123 touch create failed: %s", esp_err_to_name(tp_err));
            }
        } else {
            ESP_LOGW(TAG, "Touch panel IO create failed: %s", esp_err_to_name(tp_io_err));
        }
    } else {
        ESP_LOGW(TAG, "ST7123 touch: I2C bus not available (p4c5_board not init?)");
    }

    /* 7. 背光 */
    ESP_RETURN_ON_ERROR(init_backlight(), TAG, "Backlight init failed");

    s_initialized = true;
    ESP_LOGI(TAG, "Display initialized: %dx%d RGB565, BL=255", P4C5_LCD_WIDTH, P4C5_LCD_HEIGHT);
    return ESP_OK;
}

esp_err_t p4c5_display_bl_set(uint8_t level)
{
    s_bl_level = level;
    return ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, level);
}

uint8_t p4c5_display_bl_get(void)
{
    return s_bl_level;
}

esp_err_t p4c5_display_power_on(bool on)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;
    return esp_lcd_panel_disp_on_off(s_panel, on);
}

void p4c5_display_deinit(void)
{
    // TODO: 释放 panel, DSI bus, LDO 资源
    ESP_LOGW(TAG, "Display deinit — TODO: full cleanup");
    s_initialized = false;
}

void* p4c5_display_get_panel(void)
{
    return (void*)s_panel;
}

void* p4c5_display_get_touch(void)
{
    return (void*)s_touch;
}
