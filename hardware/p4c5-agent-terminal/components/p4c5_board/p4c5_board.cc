/**
 * @file p4c5_board.cc
 * @brief P4C5 板级统一初始化实现
 *
 * 参考 xiaozhi main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc
 * 初始化顺序：I2C → PMIC → Display → Audio → 4G
 *
 * 关键设计决策：
 *   - 共享 I2C bus (I2C0): SDA=GPIO7, SCL=GPIO8
 *     挂载设备: AXP2101(0x34), ES8311(0x18), ES7210(0x40),
 *               ST7123(0x55) [修正: 之前误写0x5A], LSM6DS3(0x6A), MCP4725(0x60)
 *   - I2C handle 通过 p4c5_board_get_i2c_bus() 暴露给各子系统
 */

#include "p4c5_board.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <driver/i2c_master.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "p4c5_board";

/* ── 共享资源 ── */
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static bool s_initialized = false;

/* ── I2C 初始化 ── */
static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = P4C5_I2C_PORT,
        .sda_io_num = P4C5_I2C_SDA_GPIO,
        .scl_io_num = P4C5_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "I2C bus creation failed");
    ESP_LOGI(TAG, "I2C0 initialized: SDA=%d, SCL=%d, freq=%dHz (internal pullup)",
             P4C5_I2C_SDA_GPIO, P4C5_I2C_SCL_GPIO, P4C5_I2C_FREQ_HZ);
    return ESP_OK;
}

/* ── 公开 API ── */
/* 板级只负责 I2C bus 初始化。子系统 init (pmic/display/audio/4g)
   由 main.c 直接按序调用，参考 xiaozhi 启动模式。
   这样设计避免 p4c5_* 静态库链接器丢符号问题。 */

esp_err_t p4c5_board_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Board already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " p4c5-agent-terminal board init v%s", P4C5_BOARD_VERSION);
    ESP_LOGI(TAG, "========================================");

    /* Step 1: I2C bus (所有 I2C 设备共享) */
    ESP_LOGI(TAG, "[1/1] Initializing shared I2C bus...");
    ESP_RETURN_ON_ERROR(init_i2c(), TAG, "I2C init failed");

    s_initialized = true;
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Board init complete (I2C ready, subsystems from main.c)");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

void* p4c5_board_get_i2c_bus(void)
{
    return (void*)s_i2c_bus;
}

void p4c5_board_deinit(void)
{
    if (!s_initialized) return;

    ESP_LOGI(TAG, "Deinitializing board (I2C bus only)...");

    if (s_i2c_bus) {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Board deinitialized");
}
