/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file axp2101_registers.c
 * @author Kevin Assen (kevin@protoconcept.nl)
 * @brief Getters and setters for each of the registers of the AXP2101
 * @version 0.1
 * @date 05-08-2025
 * 
 * @company Protoconcept
 * 
 * @license MIT
 * 
 * 
 * @copyright Copyright (c) 2025 Kevin Assen, Protoconcept
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "axp2101_registers.h"

static i2c_master_dev_handle_t axp2101_handle = NULL;

static esp_err_t axp2101_read_register(uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(axp2101_handle, &reg, 1, val, 1, -1);
}

static esp_err_t axp2101_write_register(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(axp2101_handle, buf, sizeof(buf), -1);
}

esp_err_t axp2101_init_i2c(i2c_master_bus_handle_t bus, const i2c_device_config_t *pCfg)
{
    if (axp2101_handle != NULL) {
        return ESP_OK;
    }
    return i2c_master_bus_add_device(bus, pCfg, &axp2101_handle);
}

esp_err_t axp2101_get_pmu_status_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PMU_STATUS_1, pData);
}

esp_err_t axp2101_get_pmu_status_2(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PMU_STATUS_2, pData);
}

esp_err_t axp2101_get_pmu_chip_id(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PMU_CHIP_ID, pData);
}

esp_err_t axp2101_get_data_buffer_0(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DATA_BUFFER_0, pData);
}

esp_err_t axp2101_set_data_buffer_0(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DATA_BUFFER_0, data);
}

esp_err_t axp2101_get_data_buffer_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DATA_BUFFER_1, pData);
}

esp_err_t axp2101_set_data_buffer_1(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DATA_BUFFER_1, data);
}

esp_err_t axp2101_get_data_buffer_2(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DATA_BUFFER_2, pData);
}

esp_err_t axp2101_set_data_buffer_2(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DATA_BUFFER_2, data);
}

esp_err_t axp2101_get_data_buffer_3(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DATA_BUFFER_3, pData);
}

esp_err_t axp2101_set_data_buffer_3(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DATA_BUFFER_3, data);
}

esp_err_t axp2101_get_pmu_common_cfg(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PMU_COMMON_CFG, pData);
}

esp_err_t axp2101_set_pmu_common_cfg(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_PMU_COMMON_CFG, data);
}

esp_err_t axp2101_get_batfet_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BATFET_CTRL, pData);
}

esp_err_t axp2101_set_batfet_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BATFET_CTRL, data);
}

esp_err_t axp2101_get_temp_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TEMP_CTRL, pData);
}

esp_err_t axp2101_set_temp_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TEMP_CTRL, data);
}

esp_err_t axp2101_get_min_sys_vol_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_MIN_SYS_VOL_CTRL, pData);
}

esp_err_t axp2101_set_min_sys_vol_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_MIN_SYS_VOL_CTRL, data);
}

esp_err_t axp2101_get_input_volt_limit_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_INPUT_VOLT_LIMIT_CTRL, pData);
}

esp_err_t axp2101_set_input_volt_limit_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_INPUT_VOLT_LIMIT_CTRL, data);
}

esp_err_t axp2101_get_input_curr_limit_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_INPUT_CURR_LIMIT_CTRL, pData);
}

esp_err_t axp2101_set_input_curr_limit_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_INPUT_CURR_LIMIT_CTRL, data);
}

esp_err_t axp2101_get_reset_fuel_gauge(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_RESET_FUEL_GAUGE, pData);
}

esp_err_t axp2101_set_reset_fuel_gauge(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_RESET_FUEL_GAUGE, data);
}

esp_err_t axp2101_get_charge_gauge_wdt_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_CHARGE_GAUGE_WDT_CTRL, pData);
}

esp_err_t axp2101_set_charge_gauge_wdt_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_CHARGE_GAUGE_WDT_CTRL, data);
}

esp_err_t axp2101_get_watchdog_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_WATCHDOG_CTRL, pData);
}

esp_err_t axp2101_set_watchdog_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_WATCHDOG_CTRL, data);
}

esp_err_t axp2101_get_low_batt_warning(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_LOW_BATT_WARNING, pData);
}

esp_err_t axp2101_set_low_batt_warning(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_LOW_BATT_WARNING, data);
}

esp_err_t axp2101_get_pwron_status(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PWRON_STATUS, pData);
}

esp_err_t axp2101_get_pwroff_status(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PWROFF_STATUS, pData);
}

esp_err_t axp2101_get_pwroff_en(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PWROFF_EN, pData);
}

esp_err_t axp2101_set_pwroff_en(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_PWROFF_EN, data);
}

esp_err_t axp2101_get_dcdc_pwroff_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC_PWROFF_CTRL, pData);
}

esp_err_t axp2101_set_dcdc_pwroff_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC_PWROFF_CTRL, data);
}

esp_err_t axp2101_get_pwroff_vsys_threshold(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PWROFF_VSYS_THRESHOLD, pData);
}

esp_err_t axp2101_set_pwroff_vsys_threshold(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_PWROFF_VSYS_THRESHOLD, data);
}

esp_err_t axp2101_get_pwrok_delay_seq(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PWROK_DELAY_SEQ, pData);
}

esp_err_t axp2101_set_pwrok_delay_seq(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_PWROK_DELAY_SEQ, data);
}

esp_err_t axp2101_get_sleep_wakeup_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_SLEEP_WAKEUP_CTRL, pData);
}

esp_err_t axp2101_set_sleep_wakeup_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_SLEEP_WAKEUP_CTRL, data);
}

esp_err_t axp2101_get_irq_pwron_timing(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_PWRON_TIMING, pData);
}

esp_err_t axp2101_set_irq_pwron_timing(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_PWRON_TIMING, data);
}

esp_err_t axp2101_get_fast_pwron_0(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FAST_PWRON_0, pData);
}

esp_err_t axp2101_set_fast_pwron_0(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FAST_PWRON_0, data);
}

esp_err_t axp2101_get_fast_pwron_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FAST_PWRON_1, pData);
}

esp_err_t axp2101_set_fast_pwron_1(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FAST_PWRON_1, data);
}

esp_err_t axp2101_get_fast_pwron_2(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FAST_PWRON_2, pData);
}

esp_err_t axp2101_set_fast_pwron_2(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FAST_PWRON_2, data);
}

esp_err_t axp2101_get_fast_pwron_3(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FAST_PWRON_3, pData);
}

esp_err_t axp2101_set_fast_pwron_3(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FAST_PWRON_3, data);
}

esp_err_t axp2101_get_adc_enable(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_ADC_ENABLE, pData);
}

esp_err_t axp2101_set_adc_enable(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_ADC_ENABLE, data);
}

esp_err_t axp2101_get_vbat_h(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VBAT_H, pData);
}

esp_err_t axp2101_get_vbat_l(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VBAT_L, pData);
}

esp_err_t axp2101_get_ts_h(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TS_H, pData);
}

esp_err_t axp2101_set_ts_h(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TS_H, data);
}

esp_err_t axp2101_get_ts_l(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TS_L, pData);
}

esp_err_t axp2101_get_vbus_h(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VBUS_H, pData);
}

esp_err_t axp2101_get_vbus_l(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VBUS_L, pData);
}

esp_err_t axp2101_get_vsys_h(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VSYS_H, pData);
}

esp_err_t axp2101_get_vsys_l(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VSYS_L, pData);
}

esp_err_t axp2101_get_tdie_h(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TDIE_H, pData);
}

esp_err_t axp2101_get_tdie_l(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TDIE_L, pData);
}

esp_err_t axp2101_get_irq_enable_0(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_ENABLE_0, pData);
}

esp_err_t axp2101_set_irq_enable_0(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_ENABLE_0, data);
}

esp_err_t axp2101_get_irq_enable_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_ENABLE_1, pData);
}

esp_err_t axp2101_set_irq_enable_1(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_ENABLE_1, data);
}

esp_err_t axp2101_get_irq_enable_2(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_ENABLE_2, pData);
}

esp_err_t axp2101_set_irq_enable_2(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_ENABLE_2, data);
}

esp_err_t axp2101_get_irq_status_0(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_STATUS_0, pData);
}

esp_err_t axp2101_set_irq_status_0(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_STATUS_0, data);
}

esp_err_t axp2101_get_irq_status_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_STATUS_1, pData);
}

esp_err_t axp2101_set_irq_status_1(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_STATUS_1, data);
}

esp_err_t axp2101_get_irq_status_2(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_IRQ_STATUS_2, pData);
}

esp_err_t axp2101_set_irq_status_2(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_IRQ_STATUS_2, data);
}

esp_err_t axp2101_get_ts_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TS_CTRL, pData);
}

esp_err_t axp2101_set_ts_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TS_CTRL, data);
}

esp_err_t axp2101_get_ts_hyst_low_to_normal(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TS_HYST_LOW_TO_NORMAL, pData);
}

esp_err_t axp2101_set_ts_hyst_low_to_normal(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TS_HYST_LOW_TO_NORMAL, data);
}

esp_err_t axp2101_get_ts_hyst_high_to_normal(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TS_HYST_HIGH_TO_NORMAL, pData);
}

esp_err_t axp2101_set_ts_hyst_high_to_normal(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TS_HYST_HIGH_TO_NORMAL, data);
}

esp_err_t axp2101_get_vltf_chg(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VLTF_CHG, pData);
}

esp_err_t axp2101_set_vltf_chg(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_VLTF_CHG, data);
}

esp_err_t axp2101_get_vhtf_chg(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VHTF_CHG, pData);
}

esp_err_t axp2101_set_vhtf_chg(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_VHTF_CHG, data);
}

esp_err_t axp2101_get_vltf_work(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VLTF_WORK, pData);
}

esp_err_t axp2101_set_vltf_work(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_VLTF_WORK, data);
}

esp_err_t axp2101_get_vhtf_work(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_VHTF_WORK, pData);
}

esp_err_t axp2101_set_vhtf_work(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_VHTF_WORK, data);
}

esp_err_t axp2101_get_jeita_enable(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_JEITA_ENABLE, pData);
}

esp_err_t axp2101_set_jeita_enable(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_JEITA_ENABLE, data);
}

esp_err_t axp2101_get_jeita_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_JEITA_SETTING, pData);
}

esp_err_t axp2101_set_jeita_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_JEITA_SETTING, data);
}

esp_err_t axp2101_get_jeita_cool(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_JEITA_COOL, pData);
}

esp_err_t axp2101_set_jeita_cool(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_JEITA_COOL, data);
}

esp_err_t axp2101_get_jeita_warm(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_JEITA_WARM, pData);
}

esp_err_t axp2101_set_jeita_warm(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_JEITA_WARM, data);
}

esp_err_t axp2101_get_precharge_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_PRECHARGE_SETTING, pData);
}

esp_err_t axp2101_set_precharge_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_PRECHARGE_SETTING, data);
}

esp_err_t axp2101_get_fastcharge_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FASTCHARGE_SETTING, pData);
}

esp_err_t axp2101_set_fastcharge_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FASTCHARGE_SETTING, data);
}

esp_err_t axp2101_get_term_curr_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_TERM_CURR_SETTING, pData);
}

esp_err_t axp2101_set_term_curr_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_TERM_CURR_SETTING, data);
}

esp_err_t axp2101_get_chg_voltage_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_CHG_VOLTAGE_SETTING, pData);
}

esp_err_t axp2101_set_chg_voltage_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_CHG_VOLTAGE_SETTING, data);
}

esp_err_t axp2101_get_thermal_regulation(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_THERMAL_REGULATION, pData);
}

esp_err_t axp2101_set_thermal_regulation(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_THERMAL_REGULATION, data);
}

esp_err_t axp2101_get_chg_timeout_setting(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_CHG_TIMEOUT_SETTING, pData);
}

esp_err_t axp2101_set_chg_timeout_setting(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_CHG_TIMEOUT_SETTING, data);
}

esp_err_t axp2101_get_batt_detect_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BATT_DETECT_CTRL, pData);
}

esp_err_t axp2101_set_batt_detect_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BATT_DETECT_CTRL, data);
}

esp_err_t axp2101_get_chgled_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_CHGLED_CTRL, pData);
}

esp_err_t axp2101_set_chgled_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_CHGLED_CTRL, data);
}

esp_err_t axp2101_get_backup_chg_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BACKUP_CHG_VOLTAGE, pData);
}

esp_err_t axp2101_set_backup_chg_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BACKUP_CHG_VOLTAGE, data);
}

esp_err_t axp2101_get_dcdc_en_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC_EN_CTRL, pData);
}

esp_err_t axp2101_set_dcdc_en_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC_EN_CTRL, data);
}

esp_err_t axp2101_get_dcdc_pwm_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC_PWM_CTRL, pData);
}

esp_err_t axp2101_set_dcdc_pwm_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC_PWM_CTRL, data);
}

esp_err_t axp2101_get_dcdc1_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC1_VOLTAGE, pData);
}

esp_err_t axp2101_set_dcdc1_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC1_VOLTAGE, data);
}

esp_err_t axp2101_get_dcdc2_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC2_VOLTAGE, pData);
}

esp_err_t axp2101_set_dcdc2_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC2_VOLTAGE, data);
}

esp_err_t axp2101_get_dcdc3_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC3_VOLTAGE, pData);
}

esp_err_t axp2101_set_dcdc3_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC3_VOLTAGE, data);
}

esp_err_t axp2101_get_dcdc4_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC4_VOLTAGE, pData);
}

esp_err_t axp2101_set_dcdc4_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC4_VOLTAGE, data);
}

esp_err_t axp2101_get_dcdc5_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DCDC5_VOLTAGE, pData);
}

esp_err_t axp2101_set_dcdc5_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DCDC5_VOLTAGE, data);
}

esp_err_t axp2101_get_ldo_en_ctrl_0(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_LDO_EN_CTRL_0, pData);
}

esp_err_t axp2101_set_ldo_en_ctrl_0(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_LDO_EN_CTRL_0, data);
}

esp_err_t axp2101_get_ldo_en_ctrl_1(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_LDO_EN_CTRL_1, pData);
}

esp_err_t axp2101_set_ldo_en_ctrl_1(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_LDO_EN_CTRL_1, data);
}

esp_err_t axp2101_get_aldo1_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_ALDO1_VOLTAGE, pData);
}

esp_err_t axp2101_set_aldo1_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_ALDO1_VOLTAGE, data);
}

esp_err_t axp2101_get_aldo2_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_ALDO2_VOLTAGE, pData);
}

esp_err_t axp2101_set_aldo2_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_ALDO2_VOLTAGE, data);
}

esp_err_t axp2101_get_aldo3_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_ALDO3_VOLTAGE, pData);
}

esp_err_t axp2101_set_aldo3_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_ALDO3_VOLTAGE, data);
}

esp_err_t axp2101_get_aldo4_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_ALDO4_VOLTAGE, pData);
}

esp_err_t axp2101_set_aldo4_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_ALDO4_VOLTAGE, data);
}

esp_err_t axp2101_get_bldo1_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BLDO1_VOLTAGE, pData);
}

esp_err_t axp2101_set_bldo1_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BLDO1_VOLTAGE, data);
}

esp_err_t axp2101_get_bldo2_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BLDO2_VOLTAGE, pData);
}

esp_err_t axp2101_set_bldo2_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BLDO2_VOLTAGE, data);
}

esp_err_t axp2101_get_cpusldo_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_CPUSLDO_VOLTAGE, pData);
}

esp_err_t axp2101_set_cpusldo_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_CPUSLDO_VOLTAGE, data);
}

esp_err_t axp2101_get_dldo1_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DLDO1_VOLTAGE, pData);
}

esp_err_t axp2101_set_dldo1_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DLDO1_VOLTAGE, data);
}

esp_err_t axp2101_get_dldo2_voltage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_DLDO2_VOLTAGE, pData);
}

esp_err_t axp2101_set_dldo2_voltage(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_DLDO2_VOLTAGE, data);
}

esp_err_t axp2101_get_battery_param(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BATTERY_PARAM, pData);
}

esp_err_t axp2101_set_battery_param(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_BATTERY_PARAM, data);
}

esp_err_t axp2101_get_fuel_gauge_ctrl(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_FUEL_GAUGE_CTRL, pData);
}

esp_err_t axp2101_set_fuel_gauge_ctrl(uint8_t data) {
    return axp2101_write_register(AXP2101_REG_FUEL_GAUGE_CTRL, data);
}

esp_err_t axp2101_get_battery_percentage(uint8_t *pData) {
    return axp2101_read_register(AXP2101_REG_BATTERY_PERCENTAGE, pData);
}
