/**
 * @file p4c5_board.h
 * @brief P4C5 板级统一初始化入口
 *
 * 统一调度所有子系统的初始化：
 *   PMIC → Display → Audio → 4G
 *
 * 初始化顺序至关重要：
 *   1. PMIC (先供电)
 *   2. Display (屏幕需要 AXP2101 的 LDO 输出)
 *   3. Audio (codec 需要 ALDO3=3.3V)
 *   4. 4G (ML307C 需要 ALDO4=2.9V)
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 板级统一初始化
 *
 * 按顺序初始化：PMIC → Display → Audio → 4G
 * 创建共享 I2C bus，供所有 I2C 设备使用。
 *
 * @return ESP_OK 全部成功，其他 首个失败的子系统错误码
 */
esp_err_t p4c5_board_init(void);

/**
 * @brief 获取共享 I2C bus handle
 * @return I2C master bus handle，未初始化返回 NULL
 */
void* p4c5_board_get_i2c_bus(void);

/**
 * @brief 板级反初始化
 */
void p4c5_board_deinit(void);

#ifdef __cplusplus
}
#endif
