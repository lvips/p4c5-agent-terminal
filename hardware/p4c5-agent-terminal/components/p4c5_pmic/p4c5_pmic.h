/**
 * @file p4c5_pmic.h
 * @brief P4C5 电源管理 — AXP2101 PMIC
 *
 * 基于 xiaozhi pmic_axp2101 组件 + kevin_p4c5_4g_board.cc Pmic 类。
 * AXP2101 通过 I2C0 (0x34) 控制，配置 4 路输出：
 *   - DCDC1 = 3.3V (主电源)
 *   - ALDO1 = 1.8V (辅助)
 *   - ALDO3 = 3.3V (音频 codec)
 *   - ALDO4 = 2.9V (4G 模组 VBAT)
 *
 * 另有 14 个特殊寄存器需要在初始化时写入（来自 xiaozhi Pmic 构造函数）。
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AXP2101 PMIC
 *
 * 必须在 I2C bus 创建之后调用。
 * 配置所有电源轨电压并使能。
 *
 * @param i2c_bus  I2C master bus handle（由 p4c5_board 创建）
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_init(void* i2c_bus);

/**
 * @brief 检查 AXP2101 芯片 ID
 * @return ESP_OK ID 匹配，其他 不匹配或通信失败
 */
esp_err_t p4c5_pmic_check_id(void);

/**
 * @brief 获取电池电量百分比 (0-100)
 * @param level 输出参数
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_get_battery_level(int* level);

/**
 * @brief 查询是否正在充电
 * @param charging 输出参数
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_is_charging(bool* charging);

/**
 * @brief 查询是否正在放电
 * @param discharging 输出参数
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_is_discharging(bool* discharging);

/**
 * @brief 反初始化 PMIC
 */
void p4c5_pmic_deinit(void);

#ifdef __cplusplus
}
#endif
