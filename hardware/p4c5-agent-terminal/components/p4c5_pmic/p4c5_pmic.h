/**
 * @file p4c5_pmic.h
 * @brief P4C5 AXP2101 PMIC 完整实现
 *
 * 基于 xiaozhi pmic_axp2101 组件 + kevin_p4c5_4g_board.cc Pmic 类。
 * 包含 CCB T3 核对的 14 寄存器初始化（0x64 修正为 4.2V）。
 *
 * AXP2101 通过 I2C0 (0x34) 控制，配置 4 路输出：
 *   DCDC1 = 3.3V (主电源)
 *   ALDO1 = 1.8V (辅助)
 *   ALDO3 = 3.3V (音频 codec)
 *   ALDO4 = 2.9V (4G 模组 VBAT)
 *
 * 风险标注：
 *   R2.1: ALDO4 = 2.9V 可能不够 ML307C（标称 3.4-4.2V），M1 实测
 *   R2.2: 0x64 充电电压已从 xiaozhi 0x03 (3.55V) 修正为 0x2B (4.2V)
 *   R2.3: 0x16 输入限流解码版本差异
 *   R2.4: 0x90 ALDO2 使能（xiaozhi 使能，本项目未用，保持兼容）
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────────────────────────────────
 * 初始化 / 反初始化
 * ────────────────────────────────────────────── */

/**
 * @brief 初始化 AXP2101 PMIC
 *
 * 完整初始化序列：
 *   1. 创建 I2C device (addr=0x34, 400kHz)
 *   2. 检查芯片 ID (reg 0x03 = 0x4A)
 *   3. 调用 pmic_axp2101 配置 4 路电源轨电压 + 使能
 *   4. 写入 14 个特殊寄存器（含 0x64 修正）
 *   5. 读回验证所有寄存器写入正确
 *   6. 使能 ADC 通道（电池电压/电流监测）
 *
 * @param i2c_bus  I2C master bus handle（由 p4c5_board 创建）
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_init(void* i2c_bus);

/**
 * @brief 反初始化 PMIC，关闭所有输出
 */
void p4c5_pmic_deinit(void);

/* ──────────────────────────────────────────────
 * 电池状态
 * ────────────────────────────────────────────── */

/**
 * @brief 获取电池电量百分比 (0-100)
 *
 * 优先读取库仑计 (reg 0xA4)，
 * 回退到电压法估算（Vbat → 线性映射 3.0V=0%, 4.2V=100%）。
 *
 * @return 0-100 百分比，错误时返回 0
 */
uint8_t p4c5_pmic_get_battery_level(void);

/**
 * @brief 查询是否正在充电
 *
 * 读 PMU_STATUS_2 (reg 0x01) [2:0] 充电阶段。
 * 01 = Pre-charge, 10 = 恒流, 11 = 恒压 → 均在充电
 *
 * @return true=充电中, false=未充电
 */
bool p4c5_pmic_is_charging(void);

/**
 * @brief 查询是否正在放电
 *
 * 读 PMU_STATUS_2 (reg 0x01) [6:5] 电池电流方向。
 * 10 = 放电 → 正在使用电池供电
 *
 * @return true=放电中, false=非放电
 */
bool p4c5_pmic_is_discharging(void);

/**
 * @brief 获取电池电压 (mV)
 *
 * 读 ADC 寄存器 0x34/0x35。
 * 公式: Vbat(mV) = (reg_hi << 5 | reg_low >> 3) * 1.0
 *
 * @return 电池电压 mV，错误时返回 0
 */
uint16_t p4c5_pmic_get_vbat_mv(void);

/**
 * @brief 获取芯片温度 (°C)
 *
 * 读 ADC 寄存器 0x3C/0x3D。
 * 公式: Temp(°C) = (reg_hi << 5 | reg_low >> 3) - 144.7, 步进 0.1°C
 *
 * @return 芯片温度（°C × 10，如 452 = 45.2°C），错误时返回 0
 */
int16_t p4c5_pmic_get_die_temp_x10(void);

/* ──────────────────────────────────────────────
 * 4G 电源控制
 * ────────────────────────────────────────────── */

/**
 * @brief 控制 4G 模组电源 (ALDO4)
 *
 * on=true: ALDO4 输出 2.9V（ML307C VBAT）
 * on=false: ALDO4 关闭（节省 ~15mA 功耗）
 *
 * ⚠️ R2.1: 2.9V 可能不够 ML307C 标称 3.4-4.2V
 *
 * @param on true=上电, false=断电
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_set_4g_power(bool on);

/* ──────────────────────────────────────────────
 * 寄存器诊断
 * ────────────────────────────────────────────── */

/**
 * @brief 读取 AXP2101 寄存器
 *
 * @param reg_addr 寄存器地址 (0x00-0xFF)
 * @return 寄存器值，I2C 错误时返回 0xFF
 */
uint8_t p4c5_pmic_read_reg(uint8_t reg_addr);

/**
 * @brief 写入 AXP2101 寄存器
 *
 * @param reg_addr 寄存器地址
 * @param value 写入值
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_write_reg(uint8_t reg_addr, uint8_t value);

/**
 * @brief 打印所有 14 个特殊寄存器的当前值
 *
 * 输出格式：
 *   REG  ADDR  VALUE  DESCRIPTION
 *   0x22  0x06   VSYS low voltage shutdown + DCDC1 voltage
 *   ...
 * 用于 M1 实机验证，确认写入值与预期一致。
 */
void p4c5_pmic_dump_all_regs(void);

/**
 * @brief 检查 AXP2101 芯片 ID
 * @return ESP_OK ID=0x4A 匹配，其他=不匹配或通信失败
 */
esp_err_t p4c5_pmic_check_id(void);

#ifdef __cplusplus
}
#endif
