/**
 * @file p4c5_pmic.cc
 * @brief P4C5 AXP2101 PMIC 实现
 *
 * 基于 xiaozhi:
 *   - components/pmic_axp2101/axp2101.c  (C 底层 API)
 *   - main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc (Pmic 类, 14 寄存器初始化)
 *
 * 初始化顺序（参考 xiaozhi KevinP4c54gBoard::InitializePmicRails）：
 *   1. axp2101_init(i2c_bus)
 *   2. axp2101_check_chip_id()
 *   3. 配置 4 路电压 + 使能
 *   4. Pmic 构造函数写入 14 个特殊寄存器
 *
 * ⚠️ R2 风险：ALDO4=2.9V 可能不够 ML307C 标称 3.4-4.2V，M1 实测验证。
 */

#include "p4c5_pmic.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 引用 pmic_axp2101 组件的 C API */
extern "C" {
    esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus);
    esp_err_t axp2101_check_chip_id(void);
    esp_err_t axp2101_set_dcdc_voltage(int dcdc, float voltage);
    esp_err_t axp2101_set_dcdc_enabled(int dcdc, bool enable);
    esp_err_t axp2101_set_ldo_voltage(int ldo, float voltage);
    esp_err_t axp2101_set_ldo_enabled(int ldo, bool enable);
}

static const char* TAG = "p4c5_pmic";

/* ── AXP2101 rail 索引（匹配 pmic_axp2101 组件定义） ── */
#define PMIC_DCDC1     0
#define PMIC_ALDO1     0
#define PMIC_ALDO3     2
#define PMIC_ALDO4     3

/* ── 内部状态 ── */
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static bool s_initialized = false;

/* ── I2C 直写辅助（用于特殊寄存器初始化） ── */
static esp_err_t pmic_write_reg(uint8_t reg, uint8_t val)
{
    // TODO: 实现 I2C 直写（通过 i2c_master_transmit 到 0x34）
    // 临时使用 pmic_axp2101 组件的内部 API 或自行实现
    (void)reg;
    (void)val;
    ESP_LOGW(TAG, "pmic_write_reg(0x%02X, 0x%02X) — TODO: implement", reg, val);
    return ESP_OK;
}

static uint8_t pmic_read_reg(uint8_t reg)
{
    // TODO: 实现 I2C 直读
    (void)reg;
    return 0;
}

/**
 * 写入 14 个特殊寄存器
 * 直接移植自 xiaozhi Pmic::Pmic() 构造函数。
 * 这些寄存器控制：
 *   - 0x22: 电池检测配置
 *   - 0x27: 充电指示
 *   - 0x93: IRQ 使能
 *   - 0x90: IRQ 通道
 *   - 0x61-0x64: 电量计算参数
 *   - 0x14-0x16, 0x24, 0x50: 其他电源管理配置
 */
static esp_err_t write_special_registers(void)
{
    ESP_LOGI(TAG, "Writing 14 special AXP2101 registers");

    /* 电池检测 */
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x22, 0b110), TAG, "reg 0x22 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x27, 0x10), TAG, "reg 0x27 failed");

    /* IRQ 配置 */
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x93, 0x1C), TAG, "reg 0x93 failed");
    uint8_t val = pmic_read_reg(0x90);
    val = val | 0x02;
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x90, val), TAG, "reg 0x90 failed");

    /* 电量计参数 */
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x64, 0x03), TAG, "reg 0x64 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x61, 0x05), TAG, "reg 0x61 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x62, 0x0A), TAG, "reg 0x62 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x63, 0x15), TAG, "reg 0x63 failed");

    /* 其他电源管理 */
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x14, 0x00), TAG, "reg 0x14 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x15, 0x00), TAG, "reg 0x15 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x16, 0x05), TAG, "reg 0x16 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x24, 0x01), TAG, "reg 0x24 failed");
    ESP_RETURN_ON_ERROR(pmic_write_reg(0x50, 0x14), TAG, "reg 0x50 failed");

    return ESP_OK;
}

/* ── 公开 API ── */

esp_err_t p4c5_pmic_init(void* i2c_bus)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_bus = (i2c_master_bus_handle_t)i2c_bus;
    ESP_LOGI(TAG, "Initializing AXP2101 PMIC (addr=0x%02X)", P4C5_PMIC_I2C_ADDR);

    /* 1. 底层初始化 */
    ESP_RETURN_ON_ERROR(axp2101_init(s_i2c_bus), TAG, "axp2101_init failed");

    /* 2. 芯片 ID 检查 */
    ESP_RETURN_ON_ERROR(axp2101_check_chip_id(), TAG, "Chip ID mismatch");

    /* 3. 配置电源轨 */
    ESP_RETURN_ON_ERROR(axp2101_set_dcdc_voltage(PMIC_DCDC1, P4C5_PMIC_DCDC1_MV / 1000.0f),
                        TAG, "DCDC1 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_dcdc_enabled(PMIC_DCDC1, true),
                        TAG, "DCDC1 enable failed");

    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(PMIC_ALDO1, P4C5_PMIC_ALDO1_MV / 1000.0f),
                        TAG, "ALDO1 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(PMIC_ALDO1, true),
                        TAG, "ALDO1 enable failed");

    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(PMIC_ALDO3, P4C5_PMIC_ALDO3_MV / 1000.0f),
                        TAG, "ALDO3 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(PMIC_ALDO3, true),
                        TAG, "ALDO3 enable failed");

    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(PMIC_ALDO4, P4C5_PMIC_ALDO4_MV / 1000.0f),
                        TAG, "ALDO4 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(PMIC_ALDO4, true),
                        TAG, "ALDO4 enable failed");

    /* 等待电源稳定 */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 4. 写入 14 个特殊寄存器 */
    ESP_RETURN_ON_ERROR(write_special_registers(), TAG, "Special registers failed");

    s_initialized = true;
    ESP_LOGI(TAG, "AXP2101 initialized: DCDC1=3.3V, ALDO1=1.8V, ALDO3=3.3V, ALDO4=2.9V");
    return ESP_OK;
}

esp_err_t p4c5_pmic_check_id(void)
{
    return axp2101_check_chip_id();
}

esp_err_t p4c5_pmic_get_battery_level(int* level)
{
    if (!level) return ESP_ERR_INVALID_ARG;
    // TODO: 从 AXP2101 0xA0 (电量计) 读取百分比
    // 参考 xiaozhi Axp2101::GetBatteryLevel()
    *level = 50;  // 临时占位
    ESP_LOGW(TAG, "get_battery_level — TODO: implement, returning 50%%");
    return ESP_OK;
}

esp_err_t p4c5_pmic_is_charging(bool* charging)
{
    if (!charging) return ESP_ERR_INVALID_ARG;
    // TODO: 从 AXP2101 0x01 (充电状态位) 读取
    // 参考 xiaozhi Axp2101::IsCharging()
    *charging = false;  // 临时占位
    ESP_LOGW(TAG, "is_charging — TODO: implement, returning false");
    return ESP_OK;
}

esp_err_t p4c5_pmic_is_discharging(bool* discharging)
{
    if (!discharging) return ESP_ERR_INVALID_ARG;
    // TODO: 从 AXP2101 0x01 (放电状态位) 读取
    // 参考 xiaozhi Axp2101::IsDischarging()
    *discharging = true;  // 临时占位
    ESP_LOGW(TAG, "is_discharging — TODO: implement, returning true");
    return ESP_OK;
}

void p4c5_pmic_deinit(void)
{
    // TODO: 关闭所有电源轨
    ESP_LOGW(TAG, "PMIC deinit — TODO: full cleanup");
    s_initialized = false;
}
