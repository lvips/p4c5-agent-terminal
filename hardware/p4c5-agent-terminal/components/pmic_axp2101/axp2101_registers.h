/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AXP2101_REGISTERS_H
#define AXP2101_REGISTERS_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#define AXP2101_SLAVE_ADDRESS               (0x34)
#define AXP2101_CHIP_ID                     (0x4A)

// Status & Buffers
#define AXP2101_REG_PMU_STATUS_1            (0x00)
#define AXP2101_REG_PMU_STATUS_2            (0x01)
#define AXP2101_REG_PMU_CHIP_ID             (0x03)
#define AXP2101_REG_DATA_BUFFER_0           (0x04)
#define AXP2101_REG_DATA_BUFFER_1           (0x05)
#define AXP2101_REG_DATA_BUFFER_2           (0x06)
#define AXP2101_REG_DATA_BUFFER_3           (0x07)

// Core PMU Configuration
#define AXP2101_REG_PMU_COMMON_CFG          (0x10)
#define AXP2101_REG_BATFET_CTRL             (0x12)
#define AXP2101_REG_TEMP_CTRL               (0x13)
#define AXP2101_REG_MIN_SYS_VOL_CTRL        (0x14)
#define AXP2101_REG_INPUT_VOLT_LIMIT_CTRL   (0x15)
#define AXP2101_REG_INPUT_CURR_LIMIT_CTRL   (0x16)
#define AXP2101_REG_RESET_FUEL_GAUGE        (0x17)
#define AXP2101_REG_CHARGE_GAUGE_WDT_CTRL   (0x18)
#define AXP2101_REG_WATCHDOG_CTRL           (0x19)
#define AXP2101_REG_LOW_BATT_WARNING        (0x1A)

// Power-on/off Status
#define AXP2101_REG_PWRON_STATUS            (0x20)
#define AXP2101_REG_PWROFF_STATUS           (0x21)
#define AXP2101_REG_PWROFF_EN               (0x22)
#define AXP2101_REG_DCDC_PWROFF_CTRL        (0x23)
#define AXP2101_REG_PWROFF_VSYS_THRESHOLD   (0x24)
#define AXP2101_REG_PWROK_DELAY_SEQ         (0x25)
#define AXP2101_REG_SLEEP_WAKEUP_CTRL       (0x26)
#define AXP2101_REG_IRQ_PWRON_TIMING        (0x27)

// Fast Power-On Config
#define AXP2101_REG_FAST_PWRON_0            (0x28)
#define AXP2101_REG_FAST_PWRON_1            (0x29)
#define AXP2101_REG_FAST_PWRON_2            (0x2A)
#define AXP2101_REG_FAST_PWRON_3            (0x2B)

// ADC
#define AXP2101_REG_ADC_ENABLE              (0x30)
// ADC result registers
#define AXP2101_REG_VBAT_H                  (0x34)
#define AXP2101_REG_VBAT_L                  (0x35)
#define AXP2101_REG_TS_H                    (0x36)
#define AXP2101_REG_TS_L                    (0x37)
#define AXP2101_REG_VBUS_H                  (0x38)
#define AXP2101_REG_VBUS_L                  (0x39)
#define AXP2101_REG_VSYS_H                  (0x3A)
#define AXP2101_REG_VSYS_L                  (0x3B)
#define AXP2101_REG_TDIE_H                  (0x3C)
#define AXP2101_REG_TDIE_L                  (0x3D)

// IRQ Handling
#define AXP2101_REG_IRQ_ENABLE_0            (0x40)
#define AXP2101_REG_IRQ_ENABLE_1            (0x41)
#define AXP2101_REG_IRQ_ENABLE_2            (0x42)
#define AXP2101_REG_IRQ_STATUS_0            (0x48)
#define AXP2101_REG_IRQ_STATUS_1            (0x49)
#define AXP2101_REG_IRQ_STATUS_2            (0x4A)

// TS (Thermistor / Temp Sensor)
#define AXP2101_REG_TS_CTRL                 (0x50)
#define AXP2101_REG_TS_HYST_LOW_TO_NORMAL   (0x52)
#define AXP2101_REG_TS_HYST_HIGH_TO_NORMAL  (0x53)
#define AXP2101_REG_VLTF_CHG                (0x54)
#define AXP2101_REG_VHTF_CHG                (0x55)
#define AXP2101_REG_VLTF_WORK               (0x56)
#define AXP2101_REG_VHTF_WORK               (0x57)
#define AXP2101_REG_JEITA_ENABLE            (0x58)
#define AXP2101_REG_JEITA_SETTING           (0x59)
#define AXP2101_REG_JEITA_COOL              (0x5A)
#define AXP2101_REG_JEITA_WARM              (0x5B)

// Charging
#define AXP2101_REG_PRECHARGE_SETTING       (0x61)
#define AXP2101_REG_FASTCHARGE_SETTING      (0x62)
#define AXP2101_REG_TERM_CURR_SETTING       (0x63)
#define AXP2101_REG_CHG_VOLTAGE_SETTING     (0x64)
#define AXP2101_REG_THERMAL_REGULATION      (0x65)
#define AXP2101_REG_CHG_TIMEOUT_SETTING     (0x67)
#define AXP2101_REG_BATT_DETECT_CTRL        (0x68)
#define AXP2101_REG_CHGLED_CTRL             (0x69)
#define AXP2101_REG_BACKUP_CHG_VOLTAGE      (0x6A)

// Power Output Control
#define AXP2101_REG_DCDC_EN_CTRL            (0x80)
#define AXP2101_REG_DCDC_PWM_CTRL           (0x81)
#define AXP2101_REG_DCDC1_VOLTAGE           (0x82)
#define AXP2101_REG_DCDC2_VOLTAGE           (0x83)
#define AXP2101_REG_DCDC3_VOLTAGE           (0x84)
#define AXP2101_REG_DCDC4_VOLTAGE           (0x85)
#define AXP2101_REG_DCDC5_VOLTAGE           (0x86)
#define AXP2101_REG_LDO_EN_CTRL_0           (0x90)
#define AXP2101_REG_LDO_EN_CTRL_1           (0x91)
#define AXP2101_REG_ALDO1_VOLTAGE           (0x92)
#define AXP2101_REG_ALDO2_VOLTAGE           (0x93)
#define AXP2101_REG_ALDO3_VOLTAGE           (0x94)
#define AXP2101_REG_ALDO4_VOLTAGE           (0x95)
#define AXP2101_REG_BLDO1_VOLTAGE           (0x96)
#define AXP2101_REG_BLDO2_VOLTAGE           (0x97)
#define AXP2101_REG_CPUSLDO_VOLTAGE         (0x98)
#define AXP2101_REG_DLDO1_VOLTAGE           (0x99)
#define AXP2101_REG_DLDO2_VOLTAGE           (0x9A)

// Fuel Gauge
#define AXP2101_REG_BATTERY_PARAM           (0xA1)
#define AXP2101_REG_FUEL_GAUGE_CTRL         (0xA2)
#define AXP2101_REG_BATTERY_PERCENTAGE      (0xA4)

esp_err_t axp2101_init_i2c(i2c_master_bus_handle_t bus, const i2c_device_config_t *pCfg);

/**
 * @brief Gets ADC enable status
 * 
 * [7] Enable TS (thermistor) voltage measurement  
 * [6] Enable VBUS voltage measurement  
 * [5] Enable battery voltage measurement  
 * [4] Enable system voltage (VSYS) measurement  
 * [3] Enable die temperature (TDIE) measurement  
 * [2:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve ADC enable status
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_adc_enable(uint8_t *pData);

/**
 * @brief Sets ADC enable configuration
 * 
 * [7] Enable TS (thermistor) voltage measurement  
 * [6] Enable VBUS voltage measurement  
 * [5] Enable battery voltage measurement  
 * [4] Enable system voltage (VSYS) measurement  
 * [3] Enable die temperature (TDIE) measurement  
 * [2:0] Reserved  
 * 
 * @param data Value to write to ADC enable register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_adc_enable(uint8_t data);

/**
 * @brief Gets backup battery charging voltage setting
 * 
 * [1:0] Backup battery charge voltage  
 *   - 00: 3.3 V  
 *   - 01: 3.4 V  
 *   - 10: 3.5 V  
 *   - 11: 3.6 V  
 * [7:2] Reserved  
 * 
 * @param pData Pointer to byte to retrieve backup charge voltage setting
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_backup_chg_voltage(uint8_t *pData);

/**
 * @brief Sets backup battery charging voltage
 * 
 * [1:0] Backup battery charge voltage  
 *   - 00: 3.3 V  
 *   - 01: 3.4 V  
 *   - 10: 3.5 V  
 *   - 11: 3.6 V  
 * [7:2] Reserved  
 * 
 * @param data Value to write to backup charge voltage register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_backup_chg_voltage(uint8_t data);

/**
 * @brief Gets BATFET control configuration
 * 
 * [7] BATFET enable  
 *     - 0: Disable (BATFET off)  
 *     - 1: Enable (BATFET on)  
 * [3] BATFET turns off when charging done  
 *     - 0: No action  
 *     - 1: Turn off BATFET when charging complete  
 * [0] Force BATFET off  
 *     - 0: Normal operation  
 *     - 1: Force BATFET off  
 * Other bits are reserved  
 * 
 * @param pData Pointer to byte to retrieve BATFET control state
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_batfet_ctrl(uint8_t *pData);

/**
 * @brief Sets BATFET control configuration
 * 
 * [7] BATFET enable  
 *     - 0: Disable (BATFET off)  
 *     - 1: Enable (BATFET on)  
 * [3] BATFET turns off when charging done  
 *     - 0: No action  
 *     - 1: Turn off BATFET when charging complete  
 * [0] Force BATFET off  
 *     - 0: Normal operation  
 *     - 1: Force BATFET off  
 * Other bits should be kept at default or reserved values  
 * 
 * @param data Value to write to BATFET control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_batfet_ctrl(uint8_t data);

/**
 * @brief Gets battery detection control configuration
 * 
 * [7] Enable battery voltage detection before charging  
 * [6] Enable pre-charging before detection  
 * [5] Enable charger output when no battery is present  
 * [4] Enable re-detection on charger plug/unplug  
 * [3] Enable automatic battery detection when system boots  
 * [2] Trigger manual battery detection (set to 1 to trigger)  
 * [1:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve battery detection control state
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_batt_detect_ctrl(uint8_t *pData);

/**
 * @brief Sets battery detection control configuration
 * 
 * [7] Enable battery voltage detection before charging  
 * [6] Enable pre-charging before detection  
 * [5] Enable charger output when no battery is present  
 * [4] Enable re-detection on charger plug/unplug  
 * [3] Enable automatic battery detection when system boots  
 * [2] Trigger manual battery detection (set to 1 to trigger)  
 * [1:0] Reserved  
 * 
 * @param data Value to write to battery detection control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_batt_detect_ctrl(uint8_t data);

/**
 * @brief Gets charger gauge and watchdog timer control configuration
 * 
 * [7] Enable fuel gauge  
 * [6] Enable battery temperature ADC  
 * [5] Enable charge/discharge Coulomb counter  
 * [4] Enable battery voltage ADC  
 * [3] Enable internal current detection  
 * [2] Enable charger watchdog timer  
 * [1:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_charge_gauge_wdt_ctrl(uint8_t *pData);

/**
 * @brief Sets charger gauge and watchdog timer control configuration
 * 
 * [7] Enable fuel gauge  
 * [6] Enable battery temperature ADC  
 * [5] Enable charge/discharge Coulomb counter  
 * [4] Enable battery voltage ADC  
 * [3] Enable internal current detection  
 * [2] Enable charger watchdog timer  
 * [1:0] Reserved  
 * 
 * @param data Value to write to control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_charge_gauge_wdt_ctrl(uint8_t data);

/**
 * @brief Gets charging timeout setting
 * 
 * [7] Enable charger timeout  
 * [6:4] Timeout duration  
 *   - 000: 5 hours  
 *   - 001: 6 hours  
 *   - 010: 7 hours  
 *   - 011: 8 hours  
 *   - 100: 9 hours  
 *   - 101: 10 hours  
 *   - 110: 11 hours  
 *   - 111: 12 hours  
 * [3:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve timeout configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_chg_timeout_setting(uint8_t *pData);

/**
 * @brief Sets charging timeout setting
 * 
 * [7] Enable charger timeout  
 * [6:4] Timeout duration  
 *   - 000: 5 hours  
 *   - 001: 6 hours  
 *   - 010: 7 hours  
 *   - 011: 8 hours  
 *   - 100: 9 hours  
 *   - 101: 10 hours  
 *   - 110: 11 hours  
 *   - 111: 12 hours  
 * [3:0] Reserved  
 * 
 * @param data Value to write to timeout configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_chg_timeout_setting(uint8_t data);

/**
 * @brief Gets charging target voltage setting
 *
 * AXP2101 REG 0x64: CV charger voltage setting (datasheet 6.13.2.62)
 * [7:3] Read-only (always 0)
 * [2:0] Charge voltage limit:
 *   000 = 5.0V   001 = 4.0V   010 = 4.1V
 *   011 = 4.2V   100 = 4.35V  101 = 4.4V
 *   11X = reserved
 * Default: 011b (4.2V)
 *
 * @param pData Pointer to byte to retrieve voltage configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_chg_voltage_setting(uint8_t *pData);

/**
 * @brief Sets charging target voltage
 *
 * AXP2101 REG 0x64: CV charger voltage setting (datasheet 6.13.2.62)
 * [7:3] Read-only (hardware ignores writes)
 * [2:0] Charge voltage limit:
 *   000 = 5.0V   001 = 4.0V   010 = 4.1V
 *   011 = 4.2V   100 = 4.35V  101 = 4.4V
 *   11X = reserved
 * Default: 011b (4.2V)
 *
 * @param data Value to write to voltage configuration register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_chg_voltage_setting(uint8_t data);

/**
 * @brief Gets CHGLED control configuration
 * 
 * [2:0] Charging LED behavior  
 *   - 000: LED off  
 *   - 001: LED on  
 *   - 010: Blink when charging  
 *   - 011: Blink when not charging  
 *   - 100: On when charging, off when not  
 *   - 101: Off when charging, on when not  
 *   - 110–111: Reserved  
 * [7:3] Reserved  
 * 
 * @param pData Pointer to byte to retrieve CHGLED mode
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_chgled_ctrl(uint8_t *pData);

/**
 * @brief Sets CHGLED control configuration
 * 
 * [2:0] Charging LED behavior  
 *   - 000: LED off  
 *   - 001: LED on  
 *   - 010: Blink when charging  
 *   - 011: Blink when not charging  
 *   - 100: On when charging, off when not  
 *   - 101: Off when charging, on when not  
 *   - 110–111: Reserved  
 * [7:3] Reserved  
 * 
 * @param data Value to write to CHGLED control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_chgled_ctrl(uint8_t data);

/**
 * @brief Gets value from data buffer 0
 * 
 * This is a user-accessible general-purpose register.  
 * It does not affect PMU operation and can be used to store application-specific state.
 * 
 * @param pData Pointer to byte to retrieve buffer content
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_data_buffer_0(uint8_t *pData);

/**
 * @brief Sets value in data buffer 0
 * 
 * This is a user-accessible general-purpose register.  
 * It does not affect PMU operation and can be used to store application-specific state.
 * 
 * @param data Value to store in data buffer 0
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_data_buffer_0(uint8_t data);

/**
 * @brief Gets value from data buffer 1
 * 
 * This is a user-accessible general-purpose register.  
 * It does not affect PMU operation and can be used to store application-specific state.
 * 
 * @param pData Pointer to byte to retrieve buffer content
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_data_buffer_1(uint8_t *pData);

/**
 * @brief Sets value in data buffer 1
 * 
 * This is a user-accessible general-purpose register.  
 * It does not affect PMU operation and can be used to store application-specific state.
 * 
 * @param data Value to store in data buffer 1
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_data_buffer_1(uint8_t data);

/**
 * @brief Gets value from data buffer 2
 * 
 * This is a user-defined register without fixed function.  
 * Can be used freely by the application for storing intermediate data.
 * 
 * @param pData Pointer to byte to retrieve buffer content
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_data_buffer_2(uint8_t *pData);

/**
 * @brief Sets value in data buffer 2
 * 
 * This is a user-defined register without fixed function.  
 * Can be used freely by the application for storing intermediate data.
 * 
 * @param data Value to store in data buffer 2
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_data_buffer_2(uint8_t data);

/**
 * @brief Gets value from data buffer 3
 * 
 * This is a user-defined register without fixed function.  
 * Can be used freely by the application for storing intermediate data.
 * 
 * @param pData Pointer to byte to retrieve buffer content
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_data_buffer_3(uint8_t *pData);

/**
 * @brief Sets value in data buffer 3
 * 
 * This is a user-defined register without fixed function.  
 * Can be used freely by the application for storing intermediate data.
 * 
 * @param data Value to store in data buffer 3
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_data_buffer_3(uint8_t data);

/**
 * @brief Gets PMU chip ID
 * 
 * @param pData Pointer to byte to retrieve chip id
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pmu_chip_id(uint8_t *pData);

/**
 * @brief Gets DCDC1 output voltage setting
 * 
 * [6:0] Voltage setting (step = 12.5 mV)  
 *   - 0x00 = 0.5 V  
 *   - 0x70 = 1.4 V  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve DCDC1 voltage
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc1_voltage(uint8_t *pData);

/**
 * @brief Sets DCDC1 output voltage
 * 
 * [6:0] Voltage setting (step = 12.5 mV)  
 *   - 0x00 = 0.5 V  
 *   - 0x70 = 1.4 V  
 * [7] Reserved  
 * 
 * @param data Value to set DCDC1 output voltage
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc1_voltage(uint8_t data);

/**
 * @brief Gets DCDC/LDO enable control
 * 
 * [7] DCDC1 enable  
 * [6] DCDC2 enable  
 * [5] DCDC3 enable  
 * [4] DCDC4 enable  
 * [3] Reserved  
 * [2] ALDO1 enable  
 * [1] ALDO2 enable  
 * [0] BLDO enable  
 * 
 * @param pData Pointer to byte to retrieve enable state
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc_en_ctrl(uint8_t *pData);

/**
 * @brief Sets DCDC/LDO enable control
 * 
 * [7] DCDC1 enable  
 * [6] DCDC2 enable  
 * [5] DCDC3 enable  
 * [4] DCDC4 enable  
 * [3] Reserved  
 * [2] ALDO1 enable  
 * [1] ALDO2 enable  
 * [0] BLDO enable  
 * 
 * @param data Value to configure regulator enable states
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc_en_ctrl(uint8_t data);

/**
 * @brief Gets DCDC PWM mode control
 * 
 * [7:6] DCDC1 mode  
 * [5:4] DCDC2 mode  
 * [3:2] DCDC3 mode  
 * [1:0] DCDC4 mode  
 * 
 * For each regulator:  
 *   - 00: PWM auto  
 *   - 01: Forced PWM  
 *   - 10: Low power  
 *   - 11: Reserved  
 * 
 * @param pData Pointer to byte to retrieve PWM mode
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc_pwm_ctrl(uint8_t *pData);

/**
 * @brief Sets DCDC PWM mode control
 * 
 * [7:6] DCDC1 mode  
 * [5:4] DCDC2 mode  
 * [3:2] DCDC3 mode  
 * [1:0] DCDC4 mode  
 * 
 * For each regulator:  
 *   - 00: PWM auto  
 *   - 01: Forced PWM  
 *   - 10: Low power  
 *   - 11: Reserved  
 * 
 * @param data Value to configure all DCDC PWM modes
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc_pwm_ctrl(uint8_t data);

/**
 * @brief Gets DCDC/LDO auto power-off configuration
 * 
 * [7] DCDC1 auto power-off enable  
 * [6] DCDC2 auto power-off enable  
 * [5] DCDC3 auto power-off enable  
 * [4] DCDC4 auto power-off enable  
 * [3] ALDO1 auto power-off enable  
 * [2] ALDO2 auto power-off enable  
 * [1] BLDO auto power-off enable  
 * [0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve power-off configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc_pwroff_ctrl(uint8_t *pData);

/**
 * @brief Sets DCDC/LDO auto power-off configuration
 * 
 * [7] DCDC1 auto power-off enable  
 * [6] DCDC2 auto power-off enable  
 * [5] DCDC3 auto power-off enable  
 * [4] DCDC4 auto power-off enable  
 * [3] ALDO1 auto power-off enable  
 * [2] ALDO2 auto power-off enable  
 * [1] BLDO auto power-off enable  
 * [0] Reserved  
 * 
 * @param data Value to write to power-off configuration register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc_pwroff_ctrl(uint8_t data);

/**
 * @brief Gets fast power-on configuration register 0
 * 
 * [7:6] Long press time select  
 *   - 00: 1 s  
 *   - 01: 2 s  
 *   - 10: 3 s  
 *   - 11: 4 s  
 * [5:3] Short press time select  
 *   - 000: 10 ms  
 *   - 001: 100 ms  
 *   - 010: 500 ms  
 *   - 011: 1 s  
 *   - others: Reserved  
 * [2:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve config
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_fast_pwron_0(uint8_t *pData);

/**
 * @brief Sets fast power-on configuration register 0
 * 
 * [7:6] Long press time select  
 *   - 00: 1 s  
 *   - 01: 2 s  
 *   - 10: 3 s  
 *   - 11: 4 s  
 * [5:3] Short press time select  
 *   - 000: 10 ms  
 *   - 001: 100 ms  
 *   - 010: 500 ms  
 *   - 011: 1 s  
 *   - others: Reserved  
 * [2:0] Reserved  
 * 
 * @param data Value to write to config register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_fast_pwron_0(uint8_t data);

/**
 * @brief Gets fast power-on configuration register 1
 * 
 * [7] Disable shutdown via long press  
 * [6] Enable short press reset  
 * [5] Enable forced shutdown with long press  
 * [4:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve config
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_fast_pwron_1(uint8_t *pData);

/**
 * @brief Sets fast power-on configuration register 1
 * 
 * [7] Disable shutdown via long press  
 * [6] Enable short press reset  
 * [5] Enable forced shutdown with long press  
 * [4:0] Reserved  
 * 
 * @param data Value to write to config register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_fast_pwron_1(uint8_t data);

/**
 * @brief Gets fast power-on configuration register 2
 * 
 * [7:4] Power-on delay time  
 *   - Each bit = 0.5 s, total delay = (value * 0.5) s  
 * [3:0] Power-off delay time  
 *   - Each bit = 0.5 s, total delay = (value * 0.5) s  
 * 
 * @param pData Pointer to byte to retrieve delay configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_fast_pwron_2(uint8_t *pData);

/**
 * @brief Sets fast power-on configuration register 2
 * 
 * [7:4] Power-on delay time  
 *   - Each bit = 0.5 s, total delay = (value * 0.5) s  
 * [3:0] Power-off delay time  
 *   - Each bit = 0.5 s, total delay = (value * 0.5) s  
 * 
 * @param data Value to write to delay configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_fast_pwron_2(uint8_t data);

/**
 * @brief Gets fast power-on configuration register 3
 * 
 * [7] Enable auto restart after shutdown  
 * [6] Reserved  
 * [5] Shutdown system if VSYS < threshold  
 * [4] Enable long press reset  
 * [3:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve shutdown behavior
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_fast_pwron_3(uint8_t *pData);

/**
 * @brief Sets fast power-on configuration register 3
 * 
 * [7] Enable auto restart after shutdown  
 * [6] Reserved  
 * [5] Shutdown system if VSYS < threshold  
 * [4] Enable long press reset  
 * [3:0] Reserved  
 * 
 * @param data Value to write to shutdown behavior config
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_fast_pwron_3(uint8_t data);

/**
 * @brief Gets input current limit configuration
 * 
 * [4:0] Input current limit setting  
 *   - Step size: 50 mA/bit  
 *   - Value 00000 = 100 mA  
 *   - Value 11111 = 1.7 A  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve current limit
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_input_curr_limit_ctrl(uint8_t *pData);

/**
 * @brief Sets input current limit configuration
 * 
 * [4:0] Input current limit setting  
 *   - Step size: 50 mA/bit  
 *   - Value 00000 = 100 mA  
 *   - Value 11111 = 1.7 A  
 * [7:5] Reserved  
 * 
 * @param data Value to write to current limit register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_input_curr_limit_ctrl(uint8_t data);

/**
 * @brief Gets input voltage limit configuration
 * 
 * [6:0] Input voltage limit setting  
 *   - Step size: 80 mV/bit  
 *   - Value 0000000 = 3.88 V  
 *   - Value 1111111 = 5.00 V  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve voltage limit
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_input_volt_limit_ctrl(uint8_t *pData);

/**
 * @brief Sets input voltage limit configuration
 * 
 * [6:0] Input voltage limit setting  
 *   - Step size: 80 mV/bit  
 *   - Value 0000000 = 3.88 V  
 *   - Value 1111111 = 5.00 V  
 * [7] Reserved  
 * 
 * @param data Value to write to voltage limit register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_input_volt_limit_ctrl(uint8_t data);

/**
 * @brief Gets IRQ enable register 0 configuration
 * 
 * [7] Enable IRQ: VBUS removed  
 * [6] Enable IRQ: VBUS connected  
 * [5] Enable IRQ: VBUS low  
 * [4] Enable IRQ: VBUS valid  
 * [3] Enable IRQ: Battery inserted  
 * [2] Enable IRQ: Battery removed  
 * [1] Enable IRQ: Battery active  
 * [0] Enable IRQ: Battery inactive  
 * 
 * @param pData Pointer to byte to retrieve IRQ enable settings
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_enable_0(uint8_t *pData);

/**
 * @brief Sets IRQ enable register 0
 * 
 * [7] Enable IRQ: VBUS removed  
 * [6] Enable IRQ: VBUS connected  
 * [5] Enable IRQ: VBUS low  
 * [4] Enable IRQ: VBUS valid  
 * [3] Enable IRQ: Battery inserted  
 * [2] Enable IRQ: Battery removed  
 * [1] Enable IRQ: Battery active  
 * [0] Enable IRQ: Battery inactive  
 * 
 * @param data Value to write to IRQ enable register 0
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_enable_0(uint8_t data);

/**
 * @brief Gets IRQ enable register 1 configuration
 * 
 * [7] Enable IRQ: Charging done  
 * [6] Enable IRQ: Charging started  
 * [5] Enable IRQ: Charging timeout  
 * [4] Enable IRQ: Charging error  
 * [3] Enable IRQ: Charging current less than termination current  
 * [2] Enable IRQ: Battery over temperature  
 * [1] Enable IRQ: Battery under temperature  
 * [0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve IRQ enable settings
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_enable_1(uint8_t *pData);

/**
 * @brief Sets IRQ enable register 1
 * 
 * [7] Enable IRQ: Charging done  
 * [6] Enable IRQ: Charging started  
 * [5] Enable IRQ: Charging timeout  
 * [4] Enable IRQ: Charging error  
 * [3] Enable IRQ: Charging current less than termination current  
 * [2] Enable IRQ: Battery over temperature  
 * [1] Enable IRQ: Battery under temperature  
 * [0] Reserved  
 * 
 * @param data Value to write to IRQ enable register 1
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_enable_1(uint8_t data);

/**
 * @brief Gets IRQ enable register 2 configuration
 * 
 * [7] Enable IRQ: Power-on event  
 * [6] Enable IRQ: Power-off event  
 * [5] Enable IRQ: Over-current on DCDC  
 * [4] Enable IRQ: Short-circuit event  
 * [3] Enable IRQ: Watchdog timeout  
 * [2] Enable IRQ: VBUS over-voltage  
 * [1] Enable IRQ: Temperature warning  
 * [0] Enable IRQ: Low battery warning  
 * 
 * @param pData Pointer to byte to retrieve IRQ enable settings
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_enable_2(uint8_t *pData);

/**
 * @brief Sets IRQ enable register 2
 * 
 * [7] Enable IRQ: Power-on event  
 * [6] Enable IRQ: Power-off event  
 * [5] Enable IRQ: Over-current on DCDC  
 * [4] Enable IRQ: Short-circuit event  
 * [3] Enable IRQ: Watchdog timeout  
 * [2] Enable IRQ: VBUS over-voltage  
 * [1] Enable IRQ: Temperature warning  
 * [0] Enable IRQ: Low battery warning  
 * 
 * @param data Value to write to IRQ enable register 2
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_enable_2(uint8_t data);

/**
 * @brief Gets IRQ power-on timing configuration
 * 
 * [7:6] Power-on delay timing after power key press  
 *   - 00: 128 ms  
 *   - 01: 256 ms  
 *   - 10: 512 ms  
 *   - 11: 1 s  
 * [5:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_pwron_timing(uint8_t *pData);

/**
 * @brief Sets IRQ power-on timing configuration
 * 
 * [7:6] Power-on delay timing after power key press  
 *   - 00: 128 ms  
 *   - 01: 256 ms  
 *   - 10: 512 ms  
 *   - 11: 1 s  
 * [5:0] Reserved  
 * 
 * @param data Value to write to power-on timing register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_pwron_timing(uint8_t data);

/**
 * @brief Gets IRQ status register 0
 * 
 * [7] VBUS removed  
 * [6] VBUS connected  
 * [5] VBUS low  
 * [4] VBUS valid  
 * [3] Battery inserted  
 * [2] Battery removed  
 * [1] Battery active  
 * [0] Battery inactive  
 * 
 * Note: Writing 1 to a bit clears the corresponding IRQ flag.
 * 
 * @param pData Pointer to byte to retrieve IRQ flags
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_status_0(uint8_t *pData);

/**
 * @brief Clears IRQ status register 0 bits
 * 
 * [7] Clear VBUS removed  
 * [6] Clear VBUS connected  
 * [5] Clear VBUS low  
 * [4] Clear VBUS valid  
 * [3] Clear battery inserted  
 * [2] Clear battery removed  
 * [1] Clear battery active  
 * [0] Clear battery inactive  
 * 
 * Note: Write 1 to clear, write 0 to leave unchanged.
 * 
 * @param data Bitmask of IRQs to clear
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_status_0(uint8_t data);

/**
 * @brief Gets IRQ status register 1
 * 
 * [7] Charging done  
 * [6] Charging started  
 * [5] Charging timeout  
 * [4] Charging error  
 * [3] Charging current < termination current  
 * [2] Battery over-temperature  
 * [1] Battery under-temperature  
 * [0] Reserved  
 * 
 * Note: Writing 1 to a bit clears the corresponding IRQ flag.
 * 
 * @param pData Pointer to byte to retrieve IRQ flags
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_status_1(uint8_t *pData);

/**
 * @brief Clears IRQ status register 1 bits
 * 
 * [7] Clear charging done  
 * [6] Clear charging started  
 * [5] Clear charging timeout  
 * [4] Clear charging error  
 * [3] Clear current < termination IRQ  
 * [2] Clear battery over-temp IRQ  
 * [1] Clear battery under-temp IRQ  
 * [0] Reserved  
 * 
 * Note: Write 1 to clear, write 0 to leave unchanged.
 * 
 * @param data Bitmask of IRQs to clear
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_status_1(uint8_t data);

/**
 * @brief Gets IRQ status register 2
 * 
 * [7] Power-on event  
 * [6] Power-off event  
 * [5] DCDC over-current  
 * [4] Short-circuit event  
 * [3] Watchdog timeout  
 * [2] VBUS over-voltage  
 * [1] Temperature warning  
 * [0] Low battery warning  
 * 
 * Note: Writing 1 to a bit clears the corresponding IRQ flag.
 * 
 * @param pData Pointer to byte to retrieve IRQ flags
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_irq_status_2(uint8_t *pData);

/**
 * @brief Clears IRQ status register 2 bits
 * 
 * [7] Clear power-on event  
 * [6] Clear power-off event  
 * [5] Clear DCDC over-current  
 * [4] Clear short-circuit IRQ  
 * [3] Clear watchdog timeout  
 * [2] Clear VBUS over-voltage  
 * [1] Clear temperature warning  
 * [0] Clear low battery warning  
 * 
 * Note: Write 1 to clear, write 0 to leave unchanged.
 * 
 * @param data Bitmask of IRQs to clear
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_irq_status_2(uint8_t data);

/**
 * @brief Gets JEITA cool temperature threshold
 * 
 * [7:0] JEITA cool threshold  
 *   - Unit: 0.5 °C/bit  
 *   - Typical setting example: 0x1E = 15 °C  
 * 
 * @param pData Pointer to byte to retrieve cool temperature threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_jeita_cool(uint8_t *pData);

/**
 * @brief Sets JEITA cool temperature threshold
 * 
 * [7:0] JEITA cool threshold  
 *   - Unit: 0.5 °C/bit  
 *   - Example: 0x1E = 15 °C  
 * 
 * @param data Value to write to cool threshold register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_jeita_cool(uint8_t data);

/**
 * @brief Gets JEITA enable configuration
 * 
 * [7] JEITA function enable  
 * [6] Use TS pin as analog input (0 = JEITA mode, 1 = analog input)  
 * [5] Charging current reduction in cool mode enable  
 * [4] Charging voltage reduction in cool mode enable  
 * [3] Charging current reduction in warm mode enable  
 * [2] Charging voltage reduction in warm mode enable  
 * [1:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve JEITA enable flags
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_jeita_enable(uint8_t *pData);

/**
 * @brief Sets JEITA enable configuration
 * 
 * [7] JEITA function enable  
 * [6] Use TS pin as analog input (0 = JEITA mode, 1 = analog input)  
 * [5] Charging current reduction in cool mode enable  
 * [4] Charging voltage reduction in cool mode enable  
 * [3] Charging current reduction in warm mode enable  
 * [2] Charging voltage reduction in warm mode enable  
 * [1:0] Reserved  
 * 
 * @param data Value to write to JEITA enable register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_jeita_enable(uint8_t data);

/**
 * @brief Gets JEITA setting configuration
 * 
 * [7:4] Charging voltage reduction (warm or cool)  
 *   - Each step ≈ 40 mV  
 *   - 0x0 = 0 mV reduction  
 *   - 0x1 = 40 mV  
 *   - ...  
 *   - 0xF = 600 mV  
 * 
 * [3:0] Charging current reduction (warm or cool)  
 *   - Each step ≈ 10% of normal charging current  
 *   - 0x0 = 0% reduction  
 *   - 0x1 = 10%  
 *   - ...  
 *   - 0xF = 150% (cap/override)  
 * 
 * @param pData Pointer to byte to retrieve JEITA setting
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_jeita_setting(uint8_t *pData);

/**
 * @brief Sets JEITA setting configuration
 * 
 * [7:4] Charging voltage reduction (warm or cool)  
 *   - Step size ≈ 40 mV  
 * [3:0] Charging current reduction (warm or cool)  
 *   - Step size ≈ 10%  
 * 
 * @param data Value to write to JEITA setting register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_jeita_setting(uint8_t data);

/**
 * @brief Gets JEITA warm temperature threshold
 * 
 * [7:0] JEITA warm threshold  
 *   - Unit: 0.5 °C/bit  
 *   - Example: 0x50 = 40 °C  
 * 
 * @param pData Pointer to byte to retrieve warm temperature threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_jeita_warm(uint8_t *pData);

/**
 * @brief Sets JEITA warm temperature threshold
 * 
 * [7:0] JEITA warm threshold  
 *   - Unit: 0.5 °C/bit  
 *   - Example: 0x50 = 40 °C  
 * 
 * @param data Value to write to warm threshold register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_jeita_warm(uint8_t data);

/**
 * @brief Gets low battery warning threshold
 * 
 * [6:0] Low battery warning voltage threshold  
 *   - Step size: 0.1 V  
 *   - Range: 2.5 V (0x00) to 5.2 V (0x1B)  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_low_batt_warning(uint8_t *pData);

/**
 * @brief Sets low battery warning threshold
 * 
 * [6:0] Low battery warning voltage threshold  
 *   - Step size: 0.1 V  
 *   - Range: 2.5 V (0x00) to 5.2 V (0x1B)  
 * [7] Reserved  
 * 
 * @param data Value to write to threshold register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_low_batt_warning(uint8_t data);

/**
 * @brief Gets minimum system voltage control
 * 
 * [6:0] Minimum system voltage setting  
 *   - Step size: 100 mV  
 *   - Range: 3.0 V (0x00) to 4.2 V (0x12)  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve min system voltage
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_min_sys_vol_ctrl(uint8_t *pData);

/**
 * @brief Sets minimum system voltage
 * 
 * [6:0] Minimum system voltage setting  
 *   - Step size: 100 mV  
 *   - Range: 3.0 V (0x00) to 4.2 V (0x12)  
 * [7] Reserved  
 * 
 * @param data Value to write to system voltage register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_min_sys_vol_ctrl(uint8_t data);

/**
 * @brief Gets PMU common configuration
 * 
 * [7] Enable external power key  
 * [6] Enable internal pull-up on PWRON  
 * [5] Enable power-down on PWRON long press  
 * [4] Enable startup on PWRON low  
 * [3] Enable auto power-down on VSYS low  
 * [2] Reserved  
 * [1] Enable automatic boot  
 * [0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pmu_common_cfg(uint8_t *pData);

/**
 * @brief Sets PMU common configuration
 * 
 * [7] Enable external power key  
 * [6] Enable internal pull-up on PWRON  
 * [5] Enable power-down on PWRON long press  
 * [4] Enable startup on PWRON low  
 * [3] Enable auto power-down on VSYS low  
 * [2] Reserved  
 * [1] Enable automatic boot  
 * [0] Reserved  
 * 
 * @param data Value to write to common configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_pmu_common_cfg(uint8_t data);

/**
 * @brief Gets PMU status 1
 * 
 * [5] VBUS good indication - 0: not good 1: good
 * [4] BATFET state - 0: close 1: open
 * [3] Battery present - 0: absent 1: present
 * [2] Battery in active mode - 0: normal 1: active mode
 * [1] Thermal regulation status - 0: normal 1: thermal regulation
 * [0] Current limit state - 0: not active 1: active
 * 
 * @param pData Pointer to byte to retrieve data
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pmu_status_1(uint8_t *pData);
/**
 * @brief Gets PMU status 2
 * 
 * [2:0] Charging phase:
 *   0=Trickle, 1=Pre-charge, 2=CC, 3=CV, 4=Charge done, 5=Not charging
 * [7:3] Other status bits (see datasheet)
 * 
 * @param pData Pointer to byte to retrieve status
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pmu_status_2(uint8_t *pData);

/**
 * @brief Gets precharge current setting
 * 
 * [7:4] Reserved  
 * [3:0] Precharge current setting  
 *   - Step size: 25 mA/bit  
 *   - Range: 25 mA (0x1) to 375 mA (0xF)  
 *   - 0x0 = precharge disabled  
 * 
 * @param pData Pointer to byte to retrieve setting
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_precharge_setting(uint8_t *pData);

/**
 * @brief Sets precharge current setting
 * 
 * [7:4] Reserved  
 * [3:0] Precharge current setting  
 *   - Step size: 25 mA/bit  
 *   - Range: 25 mA (0x1) to 375 mA (0xF)  
 *   - 0x0 = precharge disabled  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_precharge_setting(uint8_t data);

/**
 * @brief Gets power-off enable control
 * 
 * [7] Enable PWRON long press shutdown  
 * [6] Enable VSYS low shutdown  
 * [5] Enable watchdog timeout shutdown  
 * [4:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve setting
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pwroff_en(uint8_t *pData);

/**
 * @brief Sets power-off enable control
 * 
 * [7] Enable PWRON long press shutdown  
 * [6] Enable VSYS low shutdown  
 * [5] Enable watchdog timeout shutdown  
 * [4:0] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_pwroff_en(uint8_t data);

/**
 * @brief Gets power-off status
 * 
 * [7:3] Reserved  
 * [2] Shutdown caused by watchdog timeout  
 * [1] Shutdown caused by PWRON long press  
 * [0] Shutdown caused by VSYS low  
 * 
 * @param pData Pointer to byte to retrieve power-off cause
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pwroff_status(uint8_t *pData);

/**
 * @brief Gets VSYS voltage threshold for shutdown
 * 
 * [6:0] VSYS shutdown threshold  
 *   - Step size: 100 mV  
 *   - Range: 3.0 V (0x00) to 4.2 V (0x12)  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pwroff_vsys_threshold(uint8_t *pData);

/**
 * @brief Sets VSYS voltage threshold for shutdown
 * 
 * [6:0] VSYS shutdown threshold  
 *   - Step size: 100 mV  
 *   - Range: 3.0 V (0x00) to 4.2 V (0x12)  
 * [7] Reserved  
 * 
 * @param data Value to write to threshold register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_pwroff_vsys_threshold(uint8_t data);

/**
 * @brief Gets power-on OK delay and sequence configuration
 * 
 * [7:4] Power-on delay time  
 *   - Step size: 1 ms  
 *   - Range: 0–15 ms  
 * [3:0] Power-on sequence time  
 *   - Step size: 0.5 ms  
 *   - Range: 0–7.5 ms  
 * 
 * @param pData Pointer to byte to retrieve timing
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pwrok_delay_seq(uint8_t *pData);

/**
 * @brief Sets power-on OK delay and sequence configuration
 * 
 * [7:4] Power-on delay time  
 *   - Step size: 1 ms  
 * [3:0] Power-on sequence time  
 *   - Step size: 0.5 ms  
 * 
 * @param data Value to write to delay configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_pwrok_delay_seq(uint8_t data);

/**
 * @brief Gets power-on status
 * 
 * [7:4] Reserved  
 * [3] Wake-up from short press  
 * [2] Wake-up from long press  
 * [1] Wake-up from RTC alarm  
 * [0] Wake-up from charger insert  
 * 
 * @param pData Pointer to byte to retrieve power-on source
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_pwron_status(uint8_t *pData);

/**
 * @brief Gets fuel gauge reset control
 * 
 * [7] Reset coulomb counter  
 * [6] Reset battery temperature measurement  
 * [5] Reset battery voltage measurement  
 * [4:0] Reserved  
 * 
 * Writing 1 to a bit triggers a reset of the corresponding measurement.
 * 
 * @param pData Pointer to byte to retrieve fuel gauge reset flags
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_reset_fuel_gauge(uint8_t *pData);

/**
 * @brief Sets fuel gauge reset control
 * 
 * [7] Reset coulomb counter  
 * [6] Reset battery temperature measurement  
 * [5] Reset battery voltage measurement  
 * [4:0] Reserved  
 * 
 * Writing 1 to a bit triggers a reset of the corresponding measurement.
 * 
 * @param data Bitmask to trigger fuel gauge resets
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_reset_fuel_gauge(uint8_t data);

/**
 * @brief Gets sleep/wake-up control settings
 * 
 * [7] Enable wake-up from charger insert  
 * [6] Enable wake-up from PWRON pin  
 * [5] Enable wake-up from timer  
 * [4] Enable sleep mode entry on command  
 * [3] Enable wake-up from IRQ  
 * [2] Enable VSYS low sleep entry  
 * [1:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve sleep/wakeup configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_sleep_wakeup_ctrl(uint8_t *pData);

/**
 * @brief Sets sleep/wake-up control settings
 * 
 * [7] Enable wake-up from charger insert  
 * [6] Enable wake-up from PWRON pin  
 * [5] Enable wake-up from timer  
 * [4] Enable sleep mode entry on command  
 * [3] Enable wake-up from IRQ  
 * [2] Enable VSYS low sleep entry  
 * [1:0] Reserved  
 * 
 * @param data Value to configure sleep/wakeup behavior
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_sleep_wakeup_ctrl(uint8_t data);

/**
 * @brief Gets temperature sensor control
 * 
 * [7] Enable TS pin input  
 * [6] Select TS mode (0 = linear NTC, 1 = comparator)  
 * [5:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve temperature sensor config
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_temp_ctrl(uint8_t *pData);

/**
 * @brief Sets temperature sensor control
 * 
 * [7] Enable TS pin input  
 * [6] Select TS mode (0 = linear NTC, 1 = comparator)  
 * [5:0] Reserved  
 * 
 * @param data Value to write to temperature control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_temp_ctrl(uint8_t data);

/**
 * @brief Gets charge termination current setting
 * 
 * [6:4] Termination current percentage  
 *   - 000: 5%  
 *   - 001: 7.5%  
 *   - 010: 10%  
 *   - 011: 12.5%  
 *   - 100: 15%  
 *   - 101: 17.5%  
 *   - 110: 20%  
 *   - 111: 22.5%  
 * [3:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve termination setting
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_term_curr_setting(uint8_t *pData);

/**
 * @brief Sets charge termination current setting
 * 
 * [6:4] Termination current percentage  
 *   - Each step ≈ 2.5%  
 *   - 000 = 5%, ..., 111 = 22.5%  
 * [3:0] Reserved  
 * 
 * @param data Value to write to termination current register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_term_curr_setting(uint8_t data);

/**
 * @brief Gets thermal regulation setting
 * 
 * [2:0] Thermal regulation threshold  
 *   - 000: 60°C  
 *   - 001: 70°C  
 *   - 010: 80°C  
 *   - 011: 90°C  
 *   - 100: 100°C  
 *   - 101: 110°C  
 *   - 110: 120°C  
 *   - 111: 130°C  
 * [7:3] Reserved  
 * 
 * @param pData Pointer to byte to retrieve thermal threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_thermal_regulation(uint8_t *pData);

/**
 * @brief Sets thermal regulation setting
 * 
 * [2:0] Thermal regulation threshold  
 *   - Same encoding as above (60–130 °C)  
 * [7:3] Reserved  
 * 
 * @param data Value to write to thermal regulation config
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_thermal_regulation(uint8_t data);

/**
 * @brief Gets TS (Temperature Sensor) control register
 * 
 * [7] Enable TS function  
 * [6] Select TS comparator mode  
 *     - 0: Analog mode  
 *     - 1: Comparator mode  
 * [5] Shutdown when battery over temp (JEITA high)  
 * [4] Shutdown when battery under temp (JEITA low)  
 * [3:0] Reserved  
 * 
 * @param pData Pointer to byte to retrieve TS configuration
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_ts_ctrl(uint8_t *pData);

/**
 * @brief Sets TS (Temperature Sensor) control register
 * 
 * [7] Enable TS function  
 * [6] Select TS comparator mode  
 *     - 0: Analog mode  
 *     - 1: Comparator mode  
 * [5] Shutdown when battery over temp (JEITA high)  
 * [4] Shutdown when battery under temp (JEITA low)  
 * [3:0] Reserved  
 * 
 * @param data Value to write to TS control register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_ts_ctrl(uint8_t data);

/**
 * @brief Gets TS high threshold (analog mode)
 * 
 * [7:0] Threshold value  
 *   - Used in analog mode  
 *   - Refer to thermistor characteristics and JEITA ranges  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_ts_h(uint8_t *pData);

/**
 * @brief Sets TS high threshold (analog mode)
 * 
 * [7:0] Threshold value  
 *   - Used in analog mode  
 *   - Refer to thermistor characteristics and JEITA ranges  
 * 
 * @param data Value to write as TS high threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_ts_h(uint8_t data);

/**
 * @brief Gets TS hysteresis from high to normal temperature
 * 
 * [7:0] Hysteresis value  
 *   - Unit: Approx. °C depending on sensor config  
 *   - Determines transition point from hot to normal zone  
 * 
 * @param pData Pointer to byte to retrieve hysteresis
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_ts_hyst_high_to_normal(uint8_t *pData);

/**
 * @brief Sets TS hysteresis from high to normal temperature
 * 
 * [7:0] Hysteresis value  
 *   - Unit: Approx. °C depending on sensor config  
 *   - Determines transition point from hot to normal zone  
 * 
 * @param data Value to write to hysteresis register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_ts_hyst_high_to_normal(uint8_t data);

/**
 * @brief Gets TS hysteresis from low to normal temperature
 * 
 * [7:0] Hysteresis value  
 *   - Unit: Approx. °C depending on sensor config  
 *   - Determines transition point from cold to normal zone  
 * 
 * @param pData Pointer to byte to retrieve hysteresis
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_ts_hyst_low_to_normal(uint8_t *pData);

/**
 * @brief Sets TS hysteresis from low to normal temperature
 * 
 * [7:0] Hysteresis value  
 *   - Unit: Approx. °C depending on sensor config  
 *   - Determines transition point from cold to normal zone  
 * 
 * @param data Value to write to hysteresis register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_ts_hyst_low_to_normal(uint8_t data);

/**
 * @brief Gets TS (temperature sensor) low byte
 * 
 * [7:0] Low 8 bits of TS ADC result  
 * - Combined with TS_H for full 12-bit value  
 * - Represents battery thermistor voltage  
 * 
 * @param pData Pointer to byte to retrieve TS low byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_ts_l(uint8_t *pData);

/**
 * @brief Gets internal die temperature high byte
 * 
 * [7:0] High 4 bits of TDIE ADC result (left-aligned)  
 * - Combined with TDIE_L for full 12-bit die temperature  
 * - Result in °C with specific offset/gain (see datasheet)
 * 
 * @param pData Pointer to byte to retrieve TDIE high byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_tdie_h(uint8_t *pData);

/**
 * @brief Gets internal die temperature low byte
 * 
 * [7:0] Low 8 bits of TDIE ADC result  
 * - Combined with TDIE_H for full 12-bit die temperature  
 * 
 * @param pData Pointer to byte to retrieve TDIE low byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_tdie_l(uint8_t *pData);

/**
 * @brief Gets battery voltage high byte
 * 
 * [7:0] High 4 bits of VBAT ADC result (left-aligned)  
 * - Combined with VBAT_L for full 12-bit battery voltage  
 * - 1 LSB ≈ 1.1 mV  
 * 
 * @param pData Pointer to byte to retrieve VBAT high byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vbat_h(uint8_t *pData);

/**
 * @brief Gets battery voltage low byte
 * 
 * [7:0] Low 8 bits of VBAT ADC result  
 * - Combined with VBAT_H for full 12-bit battery voltage  
 * 
 * @param pData Pointer to byte to retrieve VBAT low byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vbat_l(uint8_t *pData);

/**
 * @brief Gets VBUS input voltage high byte
 * 
 * [7:0] High 4 bits of VBUS ADC result (left-aligned)  
 * - Combined with VBUS_L for full 12-bit input voltage  
 * - 1 LSB ≈ 1.7 mV  
 * 
 * @param pData Pointer to byte to retrieve VBUS high byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vbus_h(uint8_t *pData);

/**
 * @brief Gets VBUS input voltage low byte
 * 
 * [7:0] Low 8 bits of VBUS ADC result  
 * - Combined with VBUS_H for full 12-bit input voltage  
 * 
 * @param pData Pointer to byte to retrieve VBUS low byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vbus_l(uint8_t *pData);

/**
 * @brief Gets VHTF (high temperature fault) threshold during charging
 * 
 * [7:0] JEITA high-temperature fault voltage threshold  
 * - Used to determine over-temp shutdown threshold while charging  
 * - Value corresponds to voltage across thermistor (typically NTC)
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vhtf_chg(uint8_t *pData);

/**
 * @brief Sets VHTF (high temperature fault) threshold during charging
 * 
 * [7:0] JEITA high-temperature fault voltage threshold  
 * - Used to determine over-temp shutdown threshold while charging  
 * 
 * @param data Value to write as threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_vhtf_chg(uint8_t data);

/**
 * @brief Gets VHTF (high temperature fault) threshold during working (not charging)
 * 
 * [7:0] JEITA high-temperature fault voltage threshold in working mode  
 * - Typically higher than charging thresholds  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vhtf_work(uint8_t *pData);

/**
 * @brief Sets VHTF (high temperature fault) threshold during working
 * 
 * [7:0] JEITA high-temperature fault voltage threshold in working mode  
 * 
 * @param data Value to write as threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_vhtf_work(uint8_t data);

/**
 * @brief Gets VLTF (low temperature fault) threshold during charging
 * 
 * [7:0] JEITA low-temperature fault voltage threshold  
 * - Used to disable charging below certain temp (e.g., 0°C)  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vltf_chg(uint8_t *pData);

/**
 * @brief Sets VLTF (low temperature fault) threshold during charging
 * 
 * [7:0] JEITA low-temperature fault voltage threshold  
 * - Used to disable charging below certain temp (e.g., 0°C)  
 * 
 * @param data Value to write as threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_vltf_chg(uint8_t data);

/**
 * @brief Gets VLTF (low temperature fault) threshold during working
 * 
 * [7:0] JEITA low-temperature fault voltage threshold in working mode  
 * - Usually more lenient than charging threshold  
 * 
 * @param pData Pointer to byte to retrieve threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vltf_work(uint8_t *pData);

/**
 * @brief Sets VLTF (low temperature fault) threshold during working
 * 
 * [7:0] JEITA low-temperature fault voltage threshold in working mode  
 * 
 * @param data Value to write as threshold
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_vltf_work(uint8_t data);

/**
 * @brief Gets system voltage high byte
 * 
 * [7:0] High 4 bits of VSYS ADC result (left-aligned)  
 * - Combined with VSYS_L for 12-bit system voltage measurement  
 * - 1 LSB ≈ 1.4 mV  
 * 
 * @param pData Pointer to byte to retrieve VSYS high byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vsys_h(uint8_t *pData);

/**
 * @brief Gets system voltage low byte
 * 
 * [7:0] Low 8 bits of VSYS ADC result  
 * - Combined with VSYS_H for full 12-bit VSYS value  
 * 
 * @param pData Pointer to byte to retrieve VSYS low byte
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_vsys_l(uint8_t *pData);

/**
 * @brief Gets DCDC2 output voltage setting
 * 
 * [6:0] Voltage setting (step = 10 mV)  
 *   - 0x00 = 0.5 V  
 *   - 0x46 = 1.2 V  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc2_voltage(uint8_t *pData);

/**
 * @brief Sets DCDC2 output voltage setting
 * 
 * [6:0] Voltage setting (step = 10 mV)  
 *   - 0x00 = 0.5 V  
 *   - 0x46 = 1.2 V  
 * [7] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc2_voltage(uint8_t data);

/**
 * @brief Gets DCDC3 output voltage setting
 * 
 * [6:0] Voltage setting  
 *   - 0x00–0x46: 0.5–1.2 V (10 mV step)  
 *   - 0x47–0x56: 1.22–1.54 V (20 mV step)  
 *   - 0x57–0x6F: 1.6–3.4 V (100 mV step)  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc3_voltage(uint8_t *pData);

/**
 * @brief Sets DCDC3 output voltage setting
 * 
 * [6:0] Voltage setting  
 *   - 0x00–0x46: 0.5–1.2 V (10 mV step)  
 *   - 0x47–0x56: 1.22–1.54 V (20 mV step)  
 *   - 0x57–0x6F: 1.6–3.4 V (100 mV step)  
 * [7] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc3_voltage(uint8_t data);

/**
 * @brief Gets DCDC4 output voltage setting
 * 
 * [6:0] Voltage setting  
 *   - 0x00–0x46: 0.5–1.2 V (10 mV step)  
 *   - 0x47–0x5F: 1.22–1.84 V (20 mV step)  
 * [7] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc4_voltage(uint8_t *pData);

/**
 * @brief Sets DCDC4 output voltage setting
 * 
 * [6:0] Voltage setting  
 *   - 0x00–0x46: 0.5–1.2 V (10 mV step)  
 *   - 0x47–0x5F: 1.22–1.84 V (20 mV step)  
 * [7] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc4_voltage(uint8_t data);

/**
 * @brief Gets DCDC5 output voltage setting
 * 
 * [4:0] Voltage setting  
 *   - 0x00 = 1.4 V  
 *   - 0x17 = 3.7 V  
 *   - Step = 100 mV  
 * [5] Slow down DCDC5 frequency compensation  
 *   - 0: Disable  
 *   - 1: Enable  
 * [7:6] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dcdc5_voltage(uint8_t *pData);

/**
 * @brief Sets DCDC5 output voltage setting
 * 
 * [4:0] Voltage setting  
 *   - 0x00 = 1.4 V  
 *   - 0x17 = 3.7 V  
 *   - Step = 100 mV  
 * [5] Slow down DCDC5 frequency compensation  
 *   - 0: Disable  
 *   - 1: Enable  
 * [7:6] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dcdc5_voltage(uint8_t data);

/**
 * @brief Gets ALDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_aldo1_voltage(uint8_t *pData);

/**
 * @brief Sets ALDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_aldo1_voltage(uint8_t data);

/**
 * @brief Gets ALDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_aldo2_voltage(uint8_t *pData);

/**
 * @brief Sets ALDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_aldo2_voltage(uint8_t data);

/**
 * @brief Gets ALDO3 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_aldo3_voltage(uint8_t *pData);

/**
 * @brief Sets ALDO3 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_aldo3_voltage(uint8_t data);

/**
 * @brief Gets ALDO4 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_aldo4_voltage(uint8_t *pData);

/**
 * @brief Sets ALDO4 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_aldo4_voltage(uint8_t data);

/**
 * @brief Gets BLDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_bldo1_voltage(uint8_t *pData);

/**
 * @brief Sets BLDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_bldo1_voltage(uint8_t data);

/**
 * @brief Gets BLDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_bldo2_voltage(uint8_t *pData);

/**
 * @brief Sets BLDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_bldo2_voltage(uint8_t data);

/**
 * @brief Gets CPUSLDO output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_cpusldo_voltage(uint8_t *pData);

/**
 * @brief Sets CPUSLDO output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_cpusldo_voltage(uint8_t data);

/**
 * @brief Gets DLDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dldo1_voltage(uint8_t *pData);

/**
 * @brief Sets DLDO1 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dldo1_voltage(uint8_t data);

/**
 * @brief Gets DLDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_dldo2_voltage(uint8_t *pData);

/**
 * @brief Sets DLDO2 output voltage setting
 * 
 * [4:0] Voltage setting (see datasheet for range)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_dldo2_voltage(uint8_t data);

/**
 * @brief Gets battery parameter config
 * 
 * [7:0] Battery parameter bits (see datasheet)  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_battery_param(uint8_t *pData);

/**
 * @brief Sets battery parameter config
 * 
 * [7:0] Battery parameter bits (see datasheet)  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_battery_param(uint8_t data);

/**
 * @brief Gets fuel gauge control
 * 
 * [7:0] Fuel gauge control bits (see datasheet)  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_fuel_gauge_ctrl(uint8_t *pData);

/**
 * @brief Sets fuel gauge control
 * 
 * [7:0] Fuel gauge control bits (see datasheet)  
 * 
 * @param data Value to write to register
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_set_fuel_gauge_ctrl(uint8_t data);

/**
 * @brief Gets battery state of charge
 * 
 * [7:0] Battery charge percentage (0–100%)  
 * 
 * @param pData Pointer to byte to retrieve value
 * @return esp_err_t ESP_OK when succesful
 */
esp_err_t axp2101_get_battery_percentage(uint8_t *pData);


/**
 * @brief Gets fast charge current setting
 * 
 * [4:0] Constant current charge current limit  
 * - 0x00 = 0 mA  
 * - 0x01–0x08: 25 * N mA (e.g. 0x02 = 50 mA, 0x08 = 200 mA)  
 * - 0x09–0x10: 200 + 100 * (N - 8) mA (e.g. 0x09 = 300 mA, ..., 0x10 = 1000 mA)  
 * [7:5] Reserved  
 * 
 * @param pData Pointer to byte to retrieve fast charge setting
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_get_fastcharge_setting(uint8_t *pData);

/**
 * @brief Sets fast charge current setting
 * 
 * [4:0] Constant current charge current limit  
 * - 0x00 = 0 mA  
 * - 0x01–0x08: 25 * N mA (e.g. 0x02 = 50 mA, 0x08 = 200 mA)  
 * - 0x09–0x10: 200 + 100 * (N - 8) mA (e.g. 0x09 = 300 mA, ..., 0x10 = 1000 mA)  
 * [7:5] Reserved  
 * 
 * @param data Value to write to the fast charge current register
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_set_fastcharge_setting(uint8_t data);

/**
 * @brief Gets LDO enable control register 0 (0x90)
 * 
 * Bitmask to control enable state of LDO0–LDO7  
 * - Each bit corresponds to one LDO  
 * - 1 = enabled, 0 = disabled
 * 
 * @param pData Pointer to byte to retrieve register value
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_get_ldo_en_ctrl_0(uint8_t *pData);

/**
 * @brief Sets LDO enable control register 0 (0x90)
 * 
 * Bitmask to control enable state of LDO0–LDO7  
 * - Each bit corresponds to one LDO  
 * - 1 = enabled, 0 = disabled
 * 
 * @param data Byte value to write to the register
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_set_ldo_en_ctrl_0(uint8_t data);

/**
 * @brief Gets LDO enable control register 1 (0x91)
 * 
 * Bitmask to control enable state of LDO8–LDO10  
 * - Each bit corresponds to one LDO  
 * - 1 = enabled, 0 = disabled
 * 
 * @param pData Pointer to byte to retrieve register value
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_get_ldo_en_ctrl_1(uint8_t *pData);

/**
 * @brief Sets LDO enable control register 1 (0x91)
 * 
 * Bitmask to control enable state of LDO8–LDO10  
 * - Each bit corresponds to one LDO  
 * - 1 = enabled, 0 = disabled
 * 
 * @param data Byte value to write to the register
 * @return esp_err_t ESP_OK when successful
 */
esp_err_t axp2101_set_ldo_en_ctrl_1(uint8_t data);

#endif