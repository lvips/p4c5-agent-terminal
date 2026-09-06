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
#include "config.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_check.h>
#include <esp_lcd_panel_ops.h>
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

/* ── ST7102 初始化命令 ── */
/* TODO: 从 xiaozhi st7102_init_cmds.h 复制完整的 ksdiy_st7102_lcd_init_cmds */
/* 此处引用外部头文件，实际部署时需将 st7102_init_cmds.h 放入本项目 */

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

    /* 5. ST7102 panel (DPI) */
    esp_lcd_dpi_panel_config_t dpi_cfg = {};
    dpi_cfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_cfg.dpi_clock_freq_mhz = P4C5_LCD_DPI_CLK_MHZ;
    dpi_cfg.virtual_channel = 0;
    dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
    dpi_cfg.num_fbs = 1;
    dpi_cfg.video_timing.h_size = P4C5_LCD_WIDTH;
    dpi_cfg.video_timing.v_size = P4C5_LCD_HEIGHT;
    dpi_cfg.video_timing.hsync_back_porch = 40;
    dpi_cfg.video_timing.hsync_pulse_width = 2;
    dpi_cfg.video_timing.hsync_front_porch = 40;
    dpi_cfg.video_timing.vsync_back_porch = 10;
    dpi_cfg.video_timing.vsync_pulse_width = 2;
    dpi_cfg.video_timing.vsync_front_porch = 310;
    dpi_cfg.flags.use_dma2d = true;

    /* TODO: 加载 ksdiy_st7102_lcd_init_cmds（从 xiaozhi st7102_init_cmds.h 复制） */
    st7102_vendor_config_t vendor_cfg = {};
    /* vendor_cfg.init_cmds = ksdiy_st7102_lcd_init_cmds; */
    /* vendor_cfg.init_cmds_size = ...; */
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

    /* 6. ST7123 触摸 */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = {};
    tp_io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_ST7123_ADDRESS;
    tp_io_cfg.control_phase_bytes = 1;
    tp_io_cfg.lcd_cmd_bits = 16;
    tp_io_cfg.scl_speed_hz = 400000;
    tp_io_cfg.flags.disable_control_phase = 1;
    /* TODO: 需要 i2c_bus_handle（由 p4c5_board 注入） */

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = P4C5_LCD_WIDTH,
        .y_max = P4C5_LCD_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = P4C5_TP_INT_GPIO,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
    };
    /* TODO: esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io); */
    /* TODO: esp_lcd_touch_new_i2c_st7123(tp_io, &tp_cfg, &s_touch); */
    ESP_LOGW(TAG, "ST7123 touch — TODO: requires i2c_bus_handle from p4c5_board");

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
