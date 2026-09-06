/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AXP2101_H
#define AXP2101_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "axp2101_registers.h"

// DCDC1
#define AXP2101_DCDC1_MIN_VOLTAGE    1.5f
#define AXP2101_DCDC1_MAX_VOLTAGE    3.4f
#define AXP2101_DCDC1_STEP_VOLTAGE   0.1f

// DCDC2
#define AXP2101_DCDC2_R1_MIN         0.5f
#define AXP2101_DCDC2_R1_MAX         1.2f
#define AXP2101_DCDC2_R1_STEP        0.01f
#define AXP2101_DCDC2_R2_MIN         1.22f
#define AXP2101_DCDC2_R2_MAX         1.54f
#define AXP2101_DCDC2_R2_STEP        0.02f
#define AXP2101_DCDC2_R2_OFFSET      71

// DCDC3
#define AXP2101_DCDC3_R1_MIN         0.5f
#define AXP2101_DCDC3_R1_MAX         1.2f
#define AXP2101_DCDC3_R1_STEP        0.01f
#define AXP2101_DCDC3_R2_MIN         1.22f
#define AXP2101_DCDC3_R2_MAX         1.54f
#define AXP2101_DCDC3_R2_STEP        0.02f
#define AXP2101_DCDC3_R2_OFFSET      71
#define AXP2101_DCDC3_R3_MIN         1.6f
#define AXP2101_DCDC3_R3_MAX         3.4f
#define AXP2101_DCDC3_R3_STEP        0.1f
#define AXP2101_DCDC3_R3_OFFSET      87

// DCDC4
#define AXP2101_DCDC4_R1_MIN         0.5f
#define AXP2101_DCDC4_R1_MAX         1.2f
#define AXP2101_DCDC4_R1_STEP        0.01f
#define AXP2101_DCDC4_R2_MIN         1.22f
#define AXP2101_DCDC4_R2_MAX         1.84f
#define AXP2101_DCDC4_R2_STEP        0.02f
#define AXP2101_DCDC4_R2_OFFSET      71

// DCDC5
#define AXP2101_DCDC5_MIN_VOLTAGE    1.4f
#define AXP2101_DCDC5_MAX_VOLTAGE    3.7f
#define AXP2101_DCDC5_STEP_VOLTAGE   0.1f


typedef enum {
    AXP2101_DCDC1 = 0,
    AXP2101_DCDC2 = 1,
    AXP2101_DCDC3 = 2,
    AXP2101_DCDC4 = 3,
    AXP2101_DCDC5 = 4
} axp2101_dcdc_t;

typedef enum {
    AXP2101_LDO_ALDO1   = 0,
    AXP2101_LDO_ALDO2   = 1,
    AXP2101_LDO_ALDO3   = 2,
    AXP2101_LDO_ALDO4   = 3,
    AXP2101_LDO_BLDO1   = 4,
    AXP2101_LDO_BLDO2   = 5,
    AXP2101_LDO_CPUSLDO = 6,
    AXP2101_LDO_DLDO1   = 7,
    AXP2101_LDO_DLDO2   = 8
} axp2101_ldo_t;

typedef enum {
    AXP2101_PRECHARGE_DISABLE = 0x00,
    AXP2101_PRECHARGE_25MA    = 0x01,
    AXP2101_PRECHARGE_50MA    = 0x02,
    AXP2101_PRECHARGE_75MA    = 0x03,
    AXP2101_PRECHARGE_100MA   = 0x04,
    AXP2101_PRECHARGE_125MA   = 0x05,
    AXP2101_PRECHARGE_150MA   = 0x06,
    AXP2101_PRECHARGE_175MA   = 0x07,
    AXP2101_PRECHARGE_200MA   = 0x08,
    AXP2101_PRECHARGE_225MA   = 0x09,
    AXP2101_PRECHARGE_250MA   = 0x0A,
    AXP2101_PRECHARGE_275MA   = 0x0B,
    AXP2101_PRECHARGE_300MA   = 0x0C,
    AXP2101_PRECHARGE_325MA   = 0x0D,
    AXP2101_PRECHARGE_350MA   = 0x0E,
    AXP2101_PRECHARGE_375MA   = 0x0F
} axp2101_precharge_current_t;

typedef enum {
    AXP2101_CHG_CURR_0MA     = 0x00,
    AXP2101_CHG_CURR_100MA   = 0x02,
    AXP2101_CHG_CURR_125MA   = 0x03,
    AXP2101_CHG_CURR_150MA   = 0x04,
    AXP2101_CHG_CURR_175MA   = 0x07,
    AXP2101_CHG_CURR_200MA   = 0x08,
    AXP2101_CHG_CURR_300MA   = 0x09,
    AXP2101_CHG_CURR_400MA   = 0x0A,
    AXP2101_CHG_CURR_500MA   = 0x0B,
    AXP2101_CHG_CURR_600MA   = 0x0C,
    AXP2101_CHG_CURR_700MA   = 0x0D,
    AXP2101_CHG_CURR_800MA   = 0x0E,
    AXP2101_CHG_CURR_900MA   = 0x0F,
    AXP2101_CHG_CURR_1000MA  = 0x10
} axp2101_charge_current_t;

typedef enum {
    AXP2101_TERM_CURR_0MA   = 0x00,
    AXP2101_TERM_CURR_25MA  = 0x01,
    AXP2101_TERM_CURR_50MA  = 0x02,
    AXP2101_TERM_CURR_75MA  = 0x03,
    AXP2101_TERM_CURR_100MA = 0x04,
    AXP2101_TERM_CURR_125MA = 0x05,
    AXP2101_TERM_CURR_150MA = 0x06,
    AXP2101_TERM_CURR_175MA = 0x07,
    AXP2101_TERM_CURR_200MA = 0x08
} axp2101_term_current_ma_t;

typedef enum {
    AXP2101_CHG_VOLT_4V00  = 0x01,
    AXP2101_CHG_VOLT_4V10  = 0x02,
    AXP2101_CHG_VOLT_4V20  = 0x03,
    AXP2101_CHG_VOLT_4V35  = 0x04,
    AXP2101_CHG_VOLT_4V40  = 0x05
} axp2101_charge_voltage_t;

typedef enum {
    AXP2101_BATT_NOT_PRESENT = 0,
    AXP2101_BATT_DISCHARGING, 
    AXP2101_BATT_CHARGING, 
    AXP2101_BATT_CHARGED
} axp2101_batt_status_t;

typedef enum {
    // IRQ_ENABLE_0 (0x40)
    AXP2101_IRQ_SOC_WARNING_LEVEL2       = (0x40 << 8) | BIT7,
    AXP2101_IRQ_SOC_WARNING_LEVEL1       = (0x40 << 8) | BIT6,
    AXP2101_IRQ_GAUGE_WATCHDOG_TIMEOUT   = (0x40 << 8) | BIT5,
    AXP2101_IRQ_GAUGE_NEW_SOC            = (0x40 << 8) | BIT4,
    AXP2101_IRQ_BATT_OVER_TEMP_CHARGE    = (0x40 << 8) | BIT3,
    AXP2101_IRQ_BATT_UNDER_TEMP_CHARGE   = (0x40 << 8) | BIT2,
    AXP2101_IRQ_BATT_OVER_TEMP_WORK      = (0x40 << 8) | BIT1,
    AXP2101_IRQ_BATT_UNDER_TEMP_WORK     = (0x40 << 8) | BIT0,

    // IRQ_ENABLE_1 (0x41)
    AXP2101_IRQ_VBUS_INSERTED            = (0x41 << 8) | BIT7,
    AXP2101_IRQ_VBUS_REMOVED             = (0x41 << 8) | BIT6,
    AXP2101_IRQ_BATT_INSERTED            = (0x41 << 8) | BIT5,
    AXP2101_IRQ_BATT_REMOVED             = (0x41 << 8) | BIT4,
    AXP2101_IRQ_PWR_KEY_SHORT_PRESS      = (0x41 << 8) | BIT3,
    AXP2101_IRQ_PWR_KEY_LONG_PRESS       = (0x41 << 8) | BIT2,
    AXP2101_IRQ_PWR_KEY_NEG_EDGE         = (0x41 << 8) | BIT1,
    AXP2101_IRQ_PWR_KEY_POS_EDGE         = (0x41 << 8) | BIT0,

    // IRQ_ENABLE_2 (0x42)
    AXP2101_IRQ_WATCHDOG_EXPIRED         = (0x42 << 8) | BIT7,
    AXP2101_IRQ_LDO_OVERCURRENT          = (0x42 << 8) | BIT6,
    AXP2101_IRQ_BATFET_OVERCURRENT       = (0x42 << 8) | BIT5,
    AXP2101_IRQ_CHARGE_DONE              = (0x42 << 8) | BIT4,
    AXP2101_IRQ_CHARGE_START             = (0x42 << 8) | BIT3,
    AXP2101_IRQ_DIE_TEMP_LEVEL1          = (0x42 << 8) | BIT2,
    AXP2101_IRQ_SAFETY_TIMER_EXPIRED     = (0x42 << 8) | BIT1,
    AXP2101_IRQ_BATT_OVERVOLTAGE         = (0x42 << 8) | BIT0
} axp2101_irq_t;

/**
 * @brief Checks if the connected AXP2101 chip has a valid chip ID
 * 
 * @return ESP_OK if the chip ID matches expected value, error otherwise
 */
esp_err_t axp2101_check_chip_id();

/**
 * @brief Enables a specific IRQ source
 * 
 * @param irq The IRQ bitmask (use axp2101_irq_t enum)
 * @return ESP_OK on success
 */
esp_err_t axp2101_enable_irq(axp2101_irq_t irq);

/**
 * @brief Initializes the AXP2101 driver and underlying I2C handle
 * 
 * @param i2c_bus Handle to the I2C master bus
 * @return ESP_OK on success
 */
esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief Initializes IRQ GPIO and attaches an ISR handler
 * 
 * @param irq_gpio GPIO pin connected to the AXP2101 IRQ output
 * @param isr_handler ISR callback function
 * @param arg Argument to pass to the ISR
 * @return ESP_OK on success
 */
esp_err_t axp2101_irq_init(gpio_num_t irq_gpio, gpio_isr_t isr_handler, void *arg);

/**
 * @brief Gets high-level battery status: charging, full, absent, or idle
 * 
 * Uses PMU_STATUS_1[3] (battery present) and PMU_STATUS_2[2:0] (charge phase).
 * 
 * @param status Pointer to store battery status (enum)
 * @return ESP_OK on success
 */
esp_err_t axp2101_get_battery_status(axp2101_batt_status_t *pStatus);

/**
 * @brief Enables or disables battery detection (bit 7 of register 0x68)
 * 
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_battery_detection_enabled(bool enable);

/**
 * @brief Enables or disables ADC measurement for battery voltage
 * 
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_battery_voltage_measurement_enabled(bool enable);

/**
 * @brief Sets the fast charge current limit
 * 
 * @param current Desired charge current (enum)
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_charge_current(axp2101_charge_current_t current);

/**
 * @brief Sets the CV charge target voltage
 * 
 * @param voltage Desired target voltage (enum)
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_charge_voltage(axp2101_charge_voltage_t voltage);

/**
 * @brief Enables or disables a specific DCDC regulator
 * 
 * @param dcdc DCDC channel (0–4)
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_dcdc_enabled(axp2101_dcdc_t dcdc, bool enable);

/**
 * @brief Sets the output voltage of a specific DCDC regulator
 * 
 * @param dcdc DCDC channel
 * @param voltage Output voltage in volts
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_dcdc_voltage(axp2101_dcdc_t dcdc, float voltage);

/**
 * @brief Enables or disables a specific LDO regulator
 * 
 * @param ldo LDO channel
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_ldo_enabled(axp2101_ldo_t ldo, bool enable);

/**
 * @brief Sets the output voltage of a specific LDO regulator (ALDO1-4, BLDO1-2)
 * 
 * @param ldo LDO channel
 * @param voltage Output voltage in volts (range 0.5V - 3.5V, step 0.1V)
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_ldo_voltage(axp2101_ldo_t ldo, float voltage);

/**
 * @brief Sets the precharge current level
 * 
 * @param current Desired precharge current (enum)
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_precharge_current(axp2101_precharge_current_t current);

/**
 * @brief Enables or disables ADC measurement for system voltage (VSYS)
 * 
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_system_voltage_measurement_enabled(bool enable);

/**
 * @brief Sets the termination current and enables/disables charge termination
 * 
 * @param current Termination current level (enum)
 * @param enable True to enable termination, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_termination_current(axp2101_term_current_ma_t current, bool enable);

/**
 * @brief Enables or disables the TS pin (thermistor input)
 * 
 * @param enable True to enable TS pin, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_ts_enabled(bool enable);

/**
 * @brief Enables or disables ADC measurement for VBUS voltage
 * 
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t axp2101_set_vbus_measurement_enabled(bool enable);

/**
 * @brief Logs decoded PMU status registers (chip state, charging, VBUS, etc.)
 */
void axp2101_print_pmu_status(void);

/**
 * @brief Prints decoded IRQ status for IRQ register 0 (0x40)
 * 
 * Each bit in the status byte represents a specific interrupt source,
 * such as battery temperature warnings and SOC events.
 * 
 * @param status Raw status byte read from IRQ Status 0 register
 */
void axp2101_print_irq_status_0(uint8_t status);

/**
 * @brief Prints decoded IRQ status for IRQ register 1 (0x41)
 * 
 * Each bit in the status byte represents a VBUS, battery insert/remove,
 * or power button interrupt.
 * 
 * @param status Raw status byte read from IRQ Status 1 register
 */
void axp2101_print_irq_status_1(uint8_t status);

/**
 * @brief Prints decoded IRQ status for IRQ register 2 (0x42)
 * 
 * Each bit in the status byte corresponds to charging-related or fault interrupts,
 * such as charge done, overcurrent, overvoltage, or watchdog timeout.
 * 
 * @param status Raw status byte read from IRQ Status 2 register
 */
void axp2101_print_irq_status_2(uint8_t status);

/**
 * @brief Enable VBAT / VBUS / VSYS / TDIE ADC channels (register 0x30).
 */
esp_err_t axp2101_enable_pmu_adc_channels(void);

/**
 * @brief Read battery voltage from ADC (register 0x34/0x35), unit mV.
 */
esp_err_t axp2101_get_vbat_voltage_mv(uint16_t *mv);

/**
 * @brief Read VBUS voltage from ADC (register 0x38/0x39), unit mV.
 */
esp_err_t axp2101_get_vbus_voltage_mv(uint16_t *mv);

/**
 * @brief Read VSYS voltage from ADC (register 0x3A/0x3B), unit mV.
 */
esp_err_t axp2101_get_vsys_voltage_mv(uint16_t *mv);

/**
 * @brief Read die temperature from ADC (register 0x3C/0x3D), unit °C.
 */
esp_err_t axp2101_get_die_temperature_c(float *temp_c);

/** 电池电流方向（PMU_STATUS_2[6:5]） */
typedef enum {
    AXP2101_BAT_CUR_STANDBY = 0,
    AXP2101_BAT_CUR_CHARGE,
    AXP2101_BAT_CUR_DISCHARGE,
} axp2101_bat_current_dir_t;

/** 工作电流查询结果（AXP2101 无 IBAT ADC，充电侧为寄存器设定/阶段推算） */
typedef struct {
    axp2101_bat_current_dir_t dir;
    uint16_t work_ma;           /**< 工作电流 mA（充电为设定值；放电为功率估算） */
    uint16_t input_limit_ma;    /**< VBUS 输入限流设定（reg 0x16），无 VBUS 时为 0 */
    bool thermal_regulation;    /**< 是否处于热调节降额 */
    bool is_estimated;          /**< work_ma 是否为估算值 */
    char phase[16];             /**< 充电阶段或 "放电"/"待机" */
} axp2101_work_current_info_t;

/**
 * @brief 解析预充电电流寄存器（0x61[3:0]）为 mA
 */
uint16_t axp2101_decode_precharge_ma(uint8_t reg);

/**
 * @brief 解析快充恒流寄存器（0x62[4:0]）为 mA
 */
uint16_t axp2101_decode_fastcharge_ma(uint8_t reg);

/**
 * @brief 解析输入限流寄存器（0x16[2:0]）为 mA
 */
uint16_t axp2101_decode_input_limit_ma(uint8_t reg);

/**
 * @brief 读取电池电流方向（充电/放电/待机）
 */
esp_err_t axp2101_get_battery_current_direction(axp2101_bat_current_dir_t *dir);

/**
 * @brief 获取当前工作电流信息（供 UI 显示）
 *
 * @param vbat_mv 电池电压 mV，用于放电功率估算
 * @param vbus_ok 是否有有效 VBUS
 * @param info    输出结构体
 */
esp_err_t axp2101_get_work_current_info(uint16_t vbat_mv, bool vbus_ok,
                                        axp2101_work_current_info_t *info);

#endif
