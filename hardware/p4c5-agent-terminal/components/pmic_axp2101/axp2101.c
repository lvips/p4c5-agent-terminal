/*
 * KSDIY - ESP32-P4 / ESP32-C5 development boards
 * Author: Kevincoooool - https://github.com/kevincoooool
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file axp2101.c
 * @author Kevin Assen (kevin@protoconcept.nl)
 * @brief Driver for the X-Powers AXP2101
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

#include "axp2101.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "AXP2101"; 

esp_err_t axp2101_irq_init(gpio_num_t irq_gpio, gpio_isr_t isr_handler, void *arg) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << irq_gpio,
        .pull_up_en = GPIO_PULLDOWN_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_install_isr_service(0);                // Safe to call multiple times
    return gpio_isr_handler_add(irq_gpio, isr_handler, arg);
}

esp_err_t axp2101_init(i2c_master_bus_handle_t i2c_bus) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_SLAVE_ADDRESS,
        .scl_speed_hz = 400000
    };
    return axp2101_init_i2c(i2c_bus, &dev_cfg);
}

esp_err_t axp2101_check_chip_id()
{
    uint8_t chipId = 0x00;
    if (axp2101_get_pmu_chip_id(&chipId) == ESP_OK)
    {
        if (chipId == AXP2101_CHIP_ID)
        {
            return ESP_OK;
        }
        else
        {
            ESP_LOGW(TAG, "Unknown chip id found: 0x%x. expected: 0x%x", chipId, AXP2101_CHIP_ID);
        }
    }
    return ESP_FAIL;
}

esp_err_t axp2101_enable_irq(axp2101_irq_t irq) {
    uint8_t reg = irq >> 8;
    uint8_t bit = irq & 0xFF;
    uint8_t current;

    esp_err_t err;

    switch (reg) {
        case 0x40:
            err = axp2101_get_irq_enable_0(&current);
            if (err != ESP_OK) return err;
            current |= bit;
            return axp2101_set_irq_enable_0(current);

        case 0x41:
            err = axp2101_get_irq_enable_1(&current);
            if (err != ESP_OK) return err;
            current |= bit;
            return axp2101_set_irq_enable_1(current);

        case 0x42:
            err = axp2101_get_irq_enable_2(&current);
            if (err != ESP_OK) return err;
            current |= bit;
            return axp2101_set_irq_enable_2(current);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

/* PMU_STATUS_2[2:0] charging phase (Zephyr / AXP2101 datasheet) */
#define AXP2101_CHG_PHASE_MASK      0x07U
#define AXP2101_CHG_PHASE_DONE      0x04U
#define AXP2101_CHG_PHASE_CV        0x03U
#define AXP2101_CHG_PHASE_NOT_CHG   0x05U

esp_err_t axp2101_get_battery_status(axp2101_batt_status_t *pStatus)
{
    if (pStatus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t st1 = 0;
    uint8_t st2 = 0;
    esp_err_t err = axp2101_get_pmu_status_1(&st1);
    if (err != ESP_OK) {
        return err;
    }
    err = axp2101_get_pmu_status_2(&st2);
    if (err != ESP_OK) {
        return err;
    }

    const bool present = (st1 & BIT3) != 0;
    const uint8_t phase = st2 & AXP2101_CHG_PHASE_MASK;

    if (!present) {
        *pStatus = AXP2101_BATT_NOT_PRESENT;
    } else if (phase == AXP2101_CHG_PHASE_DONE) {
        *pStatus = AXP2101_BATT_CHARGED;
    } else if (phase <= AXP2101_CHG_PHASE_CV) {
        *pStatus = AXP2101_BATT_CHARGING;
    } else {
        *pStatus = AXP2101_BATT_DISCHARGING;
    }

    return ESP_OK;
}

esp_err_t axp2101_set_dcdc_enabled(axp2101_dcdc_t dcdc, bool enable) {
    if (dcdc > AXP2101_DCDC5) return ESP_ERR_INVALID_ARG;

    uint8_t reg;
    esp_err_t err = axp2101_get_dcdc_en_ctrl(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= (1 << dcdc);
    else
        reg &= ~(1 << dcdc);

    return axp2101_set_dcdc_en_ctrl(reg);
}

esp_err_t axp2101_set_ldo_enabled(axp2101_ldo_t ldo, bool enable) {
    uint8_t reg, bit;
    esp_err_t err;

    if (ldo < AXP2101_LDO_DLDO2) {
        err = axp2101_get_ldo_en_ctrl_0(&reg);
        if (err != ESP_OK) return err;

        bit = ldo;
        if (enable)
            reg |= (1 << bit);
        else
            reg &= ~(1 << bit);

        return axp2101_set_ldo_en_ctrl_0(reg);
    } else if (ldo == AXP2101_LDO_DLDO2) {
        err = axp2101_get_ldo_en_ctrl_1(&reg);
        if (err != ESP_OK) return err;

        if (enable)
            reg |= BIT0;
        else
            reg &= ~BIT0;

        return axp2101_set_ldo_en_ctrl_1(reg);
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t axp2101_set_dcdc_voltage(axp2101_dcdc_t dcdc, float voltage) {
    uint8_t reg_val = 0;

    switch (dcdc) {
        case AXP2101_DCDC1:
            if (voltage < AXP2101_DCDC1_MIN_VOLTAGE || voltage > AXP2101_DCDC1_MAX_VOLTAGE)
                return ESP_ERR_INVALID_ARG;
            reg_val = (uint8_t)((voltage - AXP2101_DCDC1_MIN_VOLTAGE) / AXP2101_DCDC1_STEP_VOLTAGE);
            return axp2101_set_dcdc1_voltage(reg_val);

        case AXP2101_DCDC2:
            if (voltage >= AXP2101_DCDC2_R1_MIN && voltage <= AXP2101_DCDC2_R1_MAX) {
                reg_val = (uint8_t)((voltage - AXP2101_DCDC2_R1_MIN) / AXP2101_DCDC2_R1_STEP);
            } else if (voltage >= AXP2101_DCDC2_R2_MIN && voltage <= AXP2101_DCDC2_R2_MAX) {
                reg_val = AXP2101_DCDC2_R2_OFFSET + (uint8_t)((voltage - AXP2101_DCDC2_R2_MIN) / AXP2101_DCDC2_R2_STEP);
            } else {
                return ESP_ERR_INVALID_ARG;
            }
            return axp2101_set_dcdc2_voltage(reg_val);

        case AXP2101_DCDC3:
            if (voltage >= AXP2101_DCDC3_R1_MIN && voltage <= AXP2101_DCDC3_R1_MAX) {
                reg_val = (uint8_t)((voltage - AXP2101_DCDC3_R1_MIN) / AXP2101_DCDC3_R1_STEP);
            } else if (voltage >= AXP2101_DCDC3_R2_MIN && voltage <= AXP2101_DCDC3_R2_MAX) {
                reg_val = AXP2101_DCDC3_R2_OFFSET + (uint8_t)((voltage - AXP2101_DCDC3_R2_MIN) / AXP2101_DCDC3_R2_STEP);
            } else if (voltage >= AXP2101_DCDC3_R3_MIN && voltage <= AXP2101_DCDC3_R3_MAX) {
                reg_val = AXP2101_DCDC3_R3_OFFSET + (uint8_t)((voltage - AXP2101_DCDC3_R3_MIN) / AXP2101_DCDC3_R3_STEP);
            } else {
                return ESP_ERR_INVALID_ARG;
            }
            return axp2101_set_dcdc3_voltage(reg_val);

        case AXP2101_DCDC4:
            if (voltage >= AXP2101_DCDC4_R1_MIN && voltage <= AXP2101_DCDC4_R1_MAX) {
                reg_val = (uint8_t)((voltage - AXP2101_DCDC4_R1_MIN) / AXP2101_DCDC4_R1_STEP);
            } else if (voltage >= AXP2101_DCDC4_R2_MIN && voltage <= AXP2101_DCDC4_R2_MAX) {
                reg_val = AXP2101_DCDC4_R2_OFFSET + (uint8_t)((voltage - AXP2101_DCDC4_R2_MIN) / AXP2101_DCDC4_R2_STEP);
            } else {
                return ESP_ERR_INVALID_ARG;
            }
            return axp2101_set_dcdc4_voltage(reg_val);

        case AXP2101_DCDC5:
            if (voltage < AXP2101_DCDC5_MIN_VOLTAGE || voltage > AXP2101_DCDC5_MAX_VOLTAGE)
                return ESP_ERR_INVALID_ARG;
            reg_val = (uint8_t)((voltage - AXP2101_DCDC5_MIN_VOLTAGE) / AXP2101_DCDC5_STEP_VOLTAGE);
            return axp2101_set_dcdc5_voltage(reg_val);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t axp2101_set_battery_detection_enabled(bool enable) {
    uint8_t reg;
    esp_err_t err = axp2101_get_batt_detect_ctrl(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= BIT7;
    else
        reg &= ~BIT7;

    return axp2101_set_batt_detect_ctrl(reg);
}

esp_err_t axp2101_set_vbus_measurement_enabled(bool enable) {
    uint8_t reg;
    esp_err_t err = axp2101_get_adc_enable(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= BIT6;
    else
        reg &= ~BIT6;

    return axp2101_set_adc_enable(reg);
}

esp_err_t axp2101_set_battery_voltage_measurement_enabled(bool enable) {
    uint8_t reg;
    esp_err_t err = axp2101_get_adc_enable(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= BIT5;
    else
        reg &= ~BIT5;

    return axp2101_set_adc_enable(reg);
}

esp_err_t axp2101_set_system_voltage_measurement_enabled(bool enable) {
    uint8_t reg;
    esp_err_t err = axp2101_get_adc_enable(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= BIT4;
    else
        reg &= ~BIT4;

    return axp2101_set_adc_enable(reg);
}

static uint16_t axp2101_adc_combine_h6l8(uint8_t hi, uint8_t lo)
{
    /* XPowersLib readRegisterH6L8: 高字节低 6 位 + 低字节 8 位，结果为 mV */
    return (uint16_t)(((hi & 0x3FU) << 8) | lo);
}

typedef esp_err_t (*axp2101_adc_get_byte_fn)(uint8_t *pData);

static esp_err_t axp2101_read_adc_pair_mv(axp2101_adc_get_byte_fn get_hi,
                                          axp2101_adc_get_byte_fn get_lo,
                                          uint16_t *mv)
{
    if (mv == NULL || get_hi == NULL || get_lo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t hi = 0;
    uint8_t lo = 0;
    esp_err_t err = get_hi(&hi);
    if (err != ESP_OK) {
        return err;
    }
    err = get_lo(&lo);
    if (err != ESP_OK) {
        return err;
    }

    *mv = axp2101_adc_combine_h6l8(hi, lo);
    return ESP_OK;
}

esp_err_t axp2101_enable_pmu_adc_channels(void)
{
    esp_err_t err = axp2101_set_battery_voltage_measurement_enabled(true);
    if (err != ESP_OK) {
        return err;
    }
    err = axp2101_set_vbus_measurement_enabled(true);
    if (err != ESP_OK) {
        return err;
    }
    err = axp2101_set_system_voltage_measurement_enabled(true);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t adc_en = 0;
    err = axp2101_get_adc_enable(&adc_en);
    if (err != ESP_OK) {
        return err;
    }
    adc_en |= BIT3; /* TDIE */
    err = axp2101_set_adc_enable(adc_en);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t gauge = 0;
    err = axp2101_get_charge_gauge_wdt_ctrl(&gauge);
    if (err != ESP_OK) {
        return err;
    }
    gauge |= BIT3; /* 内部电流检测 */
    gauge |= BIT7; /* 电量计 */
    return axp2101_set_charge_gauge_wdt_ctrl(gauge);
}

uint16_t axp2101_decode_precharge_ma(uint8_t reg)
{
    const uint8_t n = reg & 0x0FU;
    if (n > 8U) {
        return 0;
    }
    return (uint16_t)n * 25U;
}

uint16_t axp2101_decode_fastcharge_ma(uint8_t reg)
{
    const uint8_t n = reg & 0x1FU;
    if (n == 0U) {
        return 0;
    }
    if (n <= 8U) {
        return (uint16_t)n * 25U;
    }
    if (n <= 0x10U) {
        return (uint16_t)(200U + (n - 8U) * 100U);
    }
    return 0;
}

uint16_t axp2101_decode_input_limit_ma(uint8_t reg)
{
    switch (reg & 0x07U) {
    case 0:
        return 100;
    case 1:
        return 500;
    case 2:
        return 900;
    case 3:
        return 1000;
    case 4:
        return 1500;
    case 5:
        return 2000;
    default:
        return 0;
    }
}

static uint16_t decode_term_curr_ma(uint8_t reg)
{
    const uint8_t n = reg & 0x0FU;
    if (n > 8U) {
        return 200;
    }
    return (uint16_t)n * 25U;
}

esp_err_t axp2101_get_battery_current_direction(axp2101_bat_current_dir_t *dir)
{
    if (dir == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t st2 = 0;
    esp_err_t err = axp2101_get_pmu_status_2(&st2);
    if (err != ESP_OK) {
        return err;
    }

    switch ((st2 >> 5) & 0x03U) {
    case 1:
        *dir = AXP2101_BAT_CUR_CHARGE;
        break;
    case 2:
        *dir = AXP2101_BAT_CUR_DISCHARGE;
        break;
    default:
        *dir = AXP2101_BAT_CUR_STANDBY;
        break;
    }
    return ESP_OK;
}

esp_err_t axp2101_get_work_current_info(uint16_t vbat_mv, bool vbus_ok,
                                        axp2101_work_current_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(*info));
    strncpy(info->phase, "待机", sizeof(info->phase) - 1);

    uint8_t st1 = 0;
    uint8_t st2 = 0;
    esp_err_t err = axp2101_get_pmu_status_1(&st1);
    if (err != ESP_OK) {
        return err;
    }
    err = axp2101_get_pmu_status_2(&st2);
    if (err != ESP_OK) {
        return err;
    }

    info->thermal_regulation = (st1 & BIT1) != 0;

    switch ((st2 >> 5) & 0x03U) {
    case 1:
        info->dir = AXP2101_BAT_CUR_CHARGE;
        break;
    case 2:
        info->dir = AXP2101_BAT_CUR_DISCHARGE;
        break;
    default:
        info->dir = AXP2101_BAT_CUR_STANDBY;
        break;
    }

    if (vbus_ok) {
        uint8_t iin = 0;
        if (axp2101_get_input_curr_limit_ctrl(&iin) == ESP_OK) {
            info->input_limit_ma = axp2101_decode_input_limit_ma(iin);
        }
    }

    const uint8_t chg_phase = st2 & AXP2101_CHG_PHASE_MASK;

    if (info->dir == AXP2101_BAT_CUR_CHARGE || chg_phase <= AXP2101_CHG_PHASE_CV) {
        uint8_t pre = 0;
        uint8_t icc = 0;
        uint8_t iterm = 0;
        (void)axp2101_get_precharge_setting(&pre);
        (void)axp2101_get_fastcharge_setting(&icc);
        (void)axp2101_get_term_curr_setting(&iterm);

        switch (chg_phase) {
        case 0:
            info->work_ma = 10;
            strncpy(info->phase, "涓流", sizeof(info->phase) - 1);
            break;
        case 1:
            info->work_ma = axp2101_decode_precharge_ma(pre);
            strncpy(info->phase, "预充", sizeof(info->phase) - 1);
            break;
        case 2:
            info->work_ma = axp2101_decode_fastcharge_ma(icc);
            strncpy(info->phase, "恒流", sizeof(info->phase) - 1);
            break;
        case 3:
            info->work_ma = decode_term_curr_ma(iterm);
            strncpy(info->phase, "恒压", sizeof(info->phase) - 1);
            break;
        case 4:
            info->work_ma = 0;
            strncpy(info->phase, "充满", sizeof(info->phase) - 1);
            break;
        default:
            info->work_ma = 0;
            strncpy(info->phase, "未充电", sizeof(info->phase) - 1);
            break;
        }
        info->is_estimated = false;
        return ESP_OK;
    }

    if (info->dir == AXP2101_BAT_CUR_DISCHARGE) {
        strncpy(info->phase, "放电", sizeof(info->phase) - 1);
        info->is_estimated = true;
        if (vbat_mv > 2500U) {
            /* AXP2101 无 IBAT ADC：按典型系统功耗 ~1.2W 估算电池输出电流 */
            info->work_ma = (uint16_t)((1200U * 1000U) / vbat_mv);
        }
        return ESP_OK;
    }

    info->work_ma = 0;
    return ESP_OK;
}

esp_err_t axp2101_get_vbat_voltage_mv(uint16_t *mv)
{
    return axp2101_read_adc_pair_mv(axp2101_get_vbat_h, axp2101_get_vbat_l, mv);
}

esp_err_t axp2101_get_vbus_voltage_mv(uint16_t *mv)
{
    return axp2101_read_adc_pair_mv(axp2101_get_vbus_h, axp2101_get_vbus_l, mv);
}

esp_err_t axp2101_get_vsys_voltage_mv(uint16_t *mv)
{
    return axp2101_read_adc_pair_mv(axp2101_get_vsys_h, axp2101_get_vsys_l, mv);
}

esp_err_t axp2101_get_die_temperature_c(float *temp_c)
{
    if (temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t hi = 0;
    uint8_t lo = 0;
    esp_err_t err = axp2101_get_tdie_h(&hi);
    if (err != ESP_OK) {
        return err;
    }
    err = axp2101_get_tdie_l(&lo);
    if (err != ESP_OK) {
        return err;
    }

    const uint16_t raw = axp2101_adc_combine_h6l8(hi, lo);
    *temp_c = 22.0f + (7274.0f - (float)raw) / 20.0f;
    return ESP_OK;
}

esp_err_t axp2101_set_ts_enabled(bool enable) {
    uint8_t reg;
    esp_err_t err = axp2101_get_ts_ctrl(&reg);
    if (err != ESP_OK) return err;

    if (enable)
        reg |= BIT7;
    else
        reg &= ~BIT7;

    return axp2101_set_ts_ctrl(reg);
}

esp_err_t axp2101_set_precharge_current(axp2101_precharge_current_t current) {
    return axp2101_set_precharge_setting((uint8_t)current);
}

esp_err_t axp2101_set_charge_voltage(axp2101_charge_voltage_t voltage) {
    if (voltage < 0x01 || voltage > 0x05) return ESP_ERR_INVALID_ARG;

    uint8_t reg = 0;
    esp_err_t err = axp2101_get_chg_voltage_setting(&reg);
    if (err != ESP_OK) return err;

    reg = (reg & 0xF8) | (uint8_t)voltage;
    return axp2101_set_chg_voltage_setting(reg);
}

esp_err_t axp2101_set_charge_current(axp2101_charge_current_t current) {
    if (current > 0x10) return ESP_ERR_INVALID_ARG;
    return axp2101_set_fastcharge_setting((uint8_t)current);
}

esp_err_t axp2101_set_termination_current(axp2101_term_current_ma_t current, bool enable) {
    if (current > AXP2101_TERM_CURR_200MA) return ESP_ERR_INVALID_ARG;

    uint8_t value = (enable ? BIT4 : 0x00) | (uint8_t)current;
    return axp2101_set_term_curr_setting(value);
}

esp_err_t axp2101_set_ldo_voltage(axp2101_ldo_t ldo, float voltage) {
    // ALDO/BLDO voltage range: 0.5V - 3.5V, step = 0.1V
    // Register value = (voltage - 0.5) / 0.1
    #define LDO_MIN_VOLTAGE 0.5f
    #define LDO_MAX_VOLTAGE 3.5f
    #define LDO_STEP_VOLTAGE 0.1f
    
    if (voltage < LDO_MIN_VOLTAGE || voltage > LDO_MAX_VOLTAGE) {
        ESP_LOGE(TAG, "LDO voltage %.2fV out of range (0.5V - 3.5V)", voltage);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t reg_val = (uint8_t)((voltage - LDO_MIN_VOLTAGE) / LDO_STEP_VOLTAGE);
    
    switch (ldo) {
        case AXP2101_LDO_ALDO1:
            return axp2101_set_aldo1_voltage(reg_val);
        case AXP2101_LDO_ALDO2:
            return axp2101_set_aldo2_voltage(reg_val);
        case AXP2101_LDO_ALDO3:
            ESP_LOGI(TAG, "Setting ALDO3 voltage to %.2fV (reg=0x%02X)", voltage, reg_val);
            return axp2101_set_aldo3_voltage(reg_val);
        case AXP2101_LDO_ALDO4:
            return axp2101_set_aldo4_voltage(reg_val);
        case AXP2101_LDO_BLDO1:
            return axp2101_set_bldo1_voltage(reg_val);
        case AXP2101_LDO_BLDO2:
            return axp2101_set_bldo2_voltage(reg_val);
        case AXP2101_LDO_DLDO1:
            return axp2101_set_dldo1_voltage(reg_val);
        case AXP2101_LDO_DLDO2:
            return axp2101_set_dldo2_voltage(reg_val);
        case AXP2101_LDO_CPUSLDO:
            return axp2101_set_cpusldo_voltage(reg_val);
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

void axp2101_print_pmu_status(void) {
    uint8_t reg1 = 0, reg2 = 0;

    if (axp2101_get_pmu_status_1(&reg1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PMU_STATUS_1");
        return;
    }

    if (axp2101_get_pmu_status_2(&reg2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PMU_STATUS_2");
        return;
    }

    ESP_LOGI(TAG, "=== PMU STATUS 1 (0x00) ==");
    ESP_LOGI(TAG, "[5] VBUS good:              %s", (reg1 & BIT5) ? "yes" : "no");
    ESP_LOGI(TAG, "[4] BATFET state:           %s", (reg1 & BIT4) ? "open" : "closed");
    ESP_LOGI(TAG, "[3] Battery present:        %s", (reg1 & BIT3) ? "yes" : "no");
    ESP_LOGI(TAG, "[2] Battery active mode:    %s", (reg1 & BIT2) ? "yes" : "no");
    ESP_LOGI(TAG, "[1] Thermal regulation:     %s", (reg1 & BIT1) ? "active" : "normal");
    ESP_LOGI(TAG, "[0] Current limit active:   %s", (reg1 & BIT0) ? "yes" : "no");

    static const char *const chg_phase[] = {
        "涓流", "预充", "恒流", "恒压", "充满", "未充电", "?", "?",
    };
    const uint8_t phase = reg2 & AXP2101_CHG_PHASE_MASK;

    ESP_LOGI(TAG, "=== PMU STATUS 2 (0x01) ==");
    ESP_LOGI(TAG, "[2:0] Charge phase:         %s (0x%X)", chg_phase[phase], phase);
    ESP_LOGI(TAG, "[7:3] Upper status bits:    0x%02X", (unsigned)((reg2 >> 3) & 0x1FU));
}

void axp2101_print_irq_status_0(uint8_t status) {
    ESP_LOGI(TAG, "=== IRQ STATUS 0 (0x48) ===");
    ESP_LOGI(TAG, "[7] SOC warning level:             %s", (status & BIT7) ? "yes" : "no");
    ESP_LOGI(TAG, "[6] SOC shutdown level:            %s", (status & BIT6) ? "yes" : "no");
    ESP_LOGI(TAG, "[5] Gauge watchdog timeout:        %s", (status & BIT5) ? "yes" : "no");
    ESP_LOGI(TAG, "[4] New SOC value available:       %s", (status & BIT4) ? "yes" : "no");
    ESP_LOGI(TAG, "[3] Overtemp in charge mode:       %s", (status & BIT3) ? "yes" : "no");
    ESP_LOGI(TAG, "[2] Undertemp in charge mode:      %s", (status & BIT2) ? "yes" : "no");
    ESP_LOGI(TAG, "[1] Overtemp in work mode:         %s", (status & BIT1) ? "yes" : "no");
    ESP_LOGI(TAG, "[0] Undertemp in work mode:        %s", (status & BIT0) ? "yes" : "no");
}

void axp2101_print_irq_status_1(uint8_t status) {
    ESP_LOGI(TAG, "=== IRQ STATUS 1 (0x49) ===");
    ESP_LOGI(TAG, "[7] VBUS inserted:                 %s", (status & BIT7) ? "yes" : "no");
    ESP_LOGI(TAG, "[6] VBUS removed:                  %s", (status & BIT6) ? "yes" : "no");
    ESP_LOGI(TAG, "[5] Battery inserted:              %s", (status & BIT5) ? "yes" : "no");
    ESP_LOGI(TAG, "[4] Battery removed:               %s", (status & BIT4) ? "yes" : "no");
    ESP_LOGI(TAG, "[3] Short press PWR key:           %s", (status & BIT3) ? "yes" : "no");
    ESP_LOGI(TAG, "[2] Long press PWR key:            %s", (status & BIT2) ? "yes" : "no");
    ESP_LOGI(TAG, "[1] PWR key neg. edge:             %s", (status & BIT1) ? "yes" : "no");
    ESP_LOGI(TAG, "[0] PWR key pos. edge:             %s", (status & BIT0) ? "yes" : "no");
}

void axp2101_print_irq_status_2(uint8_t status) {
    ESP_LOGI(TAG, "=== IRQ STATUS 2 (0x4A) ===");
    ESP_LOGI(TAG, "[7] Watchdog expired:              %s", (status & BIT7) ? "yes" : "no");
    ESP_LOGI(TAG, "[6] LDO overcurrent:               %s", (status & BIT6) ? "yes" : "no");
    ESP_LOGI(TAG, "[5] BATFET overcurrent:            %s", (status & BIT5) ? "yes" : "no");
    ESP_LOGI(TAG, "[4] Charge done:                   %s", (status & BIT4) ? "yes" : "no");
    ESP_LOGI(TAG, "[3] Charge started:                %s", (status & BIT3) ? "yes" : "no");
    ESP_LOGI(TAG, "[2] Die over temp level 1:         %s", (status & BIT2) ? "yes" : "no");
    ESP_LOGI(TAG, "[1] Safety timer expired:          %s", (status & BIT1) ? "yes" : "no");
    ESP_LOGI(TAG, "[0] Battery overvoltage:           %s", (status & BIT0) ? "yes" : "no");
}