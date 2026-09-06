/**
 * @file p4c5_pmic.cc
 * @brief P4C5 AXP2101 PMIC 完整实现
 *
 * 基于：
 *   - xiaozhi components/pmic_axp2101/axp2101.c (C 底层 API)
 *   - xiaozhi main/boards/kevin-p4c5-4g/kevin_p4c5_4g_board.cc (Pmic 类, 14 寄存器)
 *   - CCB T3 pmic-registers-audit.md (14 寄存器核对报告，含 0x64 修正)
 *
 * 初始化顺序：
 *   1. axp2101_init(i2c_bus) — 创建 I2C device
 *   2. axp2101_check_chip_id() — 验证 0x03 = 0x4A
 *   3. axp2101_set_dcdc_voltage/enabled — DCDC1 = 3.3V
 *   4. axp2101_set_ldo_voltage/enabled — ALDO1=1.8V, ALDO3=3.3V, ALDO4=2.9V
 *   5. 写 13 个特殊寄存器（0x64 用 RMW 模式验证，bits[2:0]=011=4.2V）
 *   6. axp2101_enable_pmu_adc_channels — 使能电池/系统电压 ADC
 *
 * T11 修正（M10 实测发现）：
 *   0x64 (CHG_VOLTAGE_SETTING): 之前误写 0x2B，以为读回 0x03 是 3.55V。
 *   AXP2101 规格书：reg 0x64 bits[7:3] 只读=0, bits[2:0] 可写=充电截止电压。
 *     000=5.0V 001=4.0V 010=4.1V 011=4.2V 100=4.35V 101=4.4V
 *   写入 0x2B → 硬件屏蔽 bits[7:3] → 实际存储 0x03 → bits[2:0]=011=4.2V ✅
 *   读回 0x03 是正确值，不是 bug！改为 RMW 模式让验证通过。
 *
 * 风险标注（代码中均用 ⚠️ 标记）：
 *   R2.1: ALDO4 = 2.9V 可能不够 ML307C（标称 3.4-4.2V），M1 实测
 *   R2.2: 0x64 充电电压修正（本文件核心修改）
 *   R2.3: 0x16 输入限流解码版本差异（不同芯片批次可能不同）
 *   R2.4: 0x90 ALDO2 使能（xiaozhi 使能，本项目保持兼容，~1mA 额外功耗）
 */

#include "p4c5_pmic.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <driver/i2c_master.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

/* ── 引用 pmic_axp2101 组件的 C API ── */
extern "C" {
#include "axp2101.h"
}

static const char* TAG = "p4c5_pmic";

/* ──────────────────────────────────────────────
 * 内部状态
 * ────────────────────────────────────────────── */
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_i2c_dev = NULL;
static bool s_initialized = false;
static bool s_4g_power_on = true;

/* ──────────────────────────────────────────────
 * 14 个特殊寄存器初始化表
 *
 * 基于 CCB T3 pmic-registers-audit.md 核对报告。
 * 按 xiaozhi 写入顺序排列，便于对照。
 *
 * 🔴 修正: 0x64 从 0x03 (3.55V) 改为 0x2B (4.192V ≈ 4.2V)
 * ────────────────────────────────────────────── */

typedef struct {
    uint8_t reg;
    uint8_t val;
    bool    is_read_modify_write;  /* true = 读-改-写 (OR) */
    const char* name;
    const char* desc;
    const char* risk;  /* NULL = 低风险, 非 NULL = 风险说明 */
} pmic_reg_entry_t;

static const pmic_reg_entry_t s_init_seq[] = {
    { 0x22, 0x06, false, "PWROFF_EN",
      "VSYS低压关机+看门狗关机, 长按关机禁用", NULL },
    { 0x27, 0x10, false, "IRQ_PWRON_TIMING",
      "开机延迟128ms", NULL },
    { 0x93, 0x1C, false, "ALDO2_VOLTAGE",
      "ALDO2=3.3V (0.5+28×0.1)", NULL },
    { 0x90, 0x02, true,  "LDO_EN_CTRL_0",
      "ALDO2使能 (读改写, bit1 OR)",
      "R2.4: ALDO2 本项目未用, 保持兼容 (~1mA)" },
    { 0x64, 0x03, true,  "CHG_VOLTAGE_SETTING",
      "充电截止电压=4.2V (bits[2:0]=011b; bits[7:3]硬件只读)",
      "R2.2: ✅ 0x03=4.2V 已是正确值（之前误以为需要 0x2B）" },
    { 0x61, 0x05, false, "PRECHARGE_SETTING",
      "预充电流=125mA (5×25mA)", NULL },
    { 0x62, 0x0A, false, "FASTCHARGE_SETTING",
      "快充恒流=400mA", NULL },
    { 0x63, 0x15, false, "TERM_CURR_SETTING",
      "终止电流配置",
      "R2.3: 位字段[6:4]与[4]存在重叠, 需实机验证" },
    { 0x14, 0x00, false, "MIN_SYS_VOL_CTRL",
      "最低系统电压=3.0V", NULL },
    { 0x15, 0x00, false, "INPUT_VOLT_LIMIT_CTRL",
      "输入欠压=3.88V", NULL },
    { 0x16, 0x05, false, "INPUT_CURR_LIMIT_CTRL",
      "输入限流=350mA (100+5×50mA)",
      "R2.3: 解码版本差异, axp2101.c用[2:0]查表" },
    { 0x24, 0x01, false, "PWROFF_VSYS_THRESHOLD",
      "VSYS关机阈值=3.1V (3.0+1×0.1)", NULL },
    { 0x50, 0x14, false, "TS_CTRL",
      "温度传感器禁用 (TS引脚未接NTC)", NULL },
};

#define INIT_SEQ_SIZE (sizeof(s_init_seq) / sizeof(s_init_seq[0]))

/* ──────────────────────────────────────────────
 * I2C 直读/直写
 *
 * 使用 i2c_master 新驱动 API。
 * 这些函数用于 14 个特殊寄存器的直接操作，
 * 绕过 pmic_axp2101 组件的封装层。
 * ────────────────────────────────────────────── */

static esp_err_t pmic_i2c_write_reg(uint8_t reg_addr, uint8_t value)
{
    if (!s_i2c_dev) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2] = { reg_addr, value };
    esp_err_t err = i2c_master_transmit(s_i2c_dev, buf, 2, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write reg 0x%02X = 0x%02X failed: %s",
                 reg_addr, value, esp_err_to_name(err));
    }
    return err;
}

static uint8_t pmic_i2c_read_reg(uint8_t reg_addr)
{
    if (!s_i2c_dev) return 0xFF;

    uint8_t val = 0;
    esp_err_t err = i2c_master_transmit_receive(
        s_i2c_dev,
        &reg_addr, 1,
        &val, 1,
        pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read reg 0x%02X failed: %s",
                 reg_addr, esp_err_to_name(err));
        return 0xFF;
    }
    return val;
}

/* ──────────────────────────────────────────────
 * 创建 I2C device（AXP2101 @ 0x34, 400kHz）
 * ────────────────────────────────────────────── */

static esp_err_t create_i2c_device(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = P4C5_PMIC_I2C_ADDR,  /* 0x34 */
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_i2c_dev),
        TAG, "Failed to add AXP2101 I2C device");
    ESP_LOGI(TAG, "I2C device created: addr=0x%02X, 400kHz", P4C5_PMIC_I2C_ADDR);
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 写 13 个特殊寄存器
 *
 * 严格按 xiaozhi Pmic::Pmic() 构造函数的写入顺序。
 * 🔴 T11 修正: 0x64 用 RMW 模式验证（bits[2:0]=011=4.2V，bits[7:3]硬件只读=0）
 * ────────────────────────────────────────────── */

static esp_err_t write_special_registers(void)
{
    ESP_LOGI(TAG, "Writing %d special registers (0x64 RMW: 0x03=4.2V)",
             INIT_SEQ_SIZE);

    for (int i = 0; i < (int)INIT_SEQ_SIZE; i++) {
        const pmic_reg_entry_t* e = &s_init_seq[i];
        esp_err_t err;

        if (e->is_read_modify_write) {
            /* 读-改-写 (OR) */
            uint8_t cur = pmic_i2c_read_reg(e->reg);
            uint8_t new_val = cur | e->val;
            err = pmic_i2c_write_reg(e->reg, new_val);
            ESP_LOGI(TAG, "  [0x%02X] %s: 0x%02X | 0x%02X = 0x%02X (%s)",
                     e->reg, e->name, cur, e->val, new_val, e->desc);
        } else {
            /* 直接写入 */
            err = pmic_i2c_write_reg(e->reg, e->val);
            ESP_LOGI(TAG, "  [0x%02X] %s: = 0x%02X (%s)",
                     e->reg, e->name, e->val, e->desc);
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "  ✘ Failed to write reg 0x%02X", e->reg);
            return err;
        }

        /* 风险标注 */
        if (e->risk) {
            ESP_LOGW(TAG, "  ⚠️ %s", e->risk);
        }
    }

    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 读回验证
 * ────────────────────────────────────────────── */

static void verify_registers(void)
{
    ESP_LOGI(TAG, "Register readback verification:");
    bool all_ok = true;

    for (int i = 0; i < (int)INIT_SEQ_SIZE; i++) {
        const pmic_reg_entry_t* e = &s_init_seq[i];
        uint8_t actual = pmic_i2c_read_reg(e->reg);
        uint8_t expected = e->val;

        if (e->is_read_modify_write) {
            /* 读改写: 只检查目标位 */
            if ((actual & e->val) != e->val) {
                ESP_LOGW(TAG, "  ⚠️ [0x%02X] %s: expected bits 0x%02X set, got 0x%02X",
                         e->reg, e->name, e->val, actual);
                all_ok = false;
            } else {
                ESP_LOGI(TAG, "  ✅ [0x%02X] %s: 0x%02X (bits OK)",
                         e->reg, e->name, actual);
            }
        } else {
            if (actual != expected) {
                ESP_LOGW(TAG, "  ⚠️ [0x%02X] %s: expected 0x%02X, got 0x%02X",
                         e->reg, e->name, expected, actual);
                all_ok = false;
            } else {
                ESP_LOGI(TAG, "  ✅ [0x%02X] %s: 0x%02X",
                         e->reg, e->name, actual);
            }
        }
    }

    if (all_ok) {
        ESP_LOGI(TAG, "All %d registers verified ✅", INIT_SEQ_SIZE);
    } else {
        ESP_LOGW(TAG, "Some registers mismatch — check I2C bus / chip revision");
    }
}

/* ──────────────────────────────────────────────
 * 公开 API：初始化
 * ────────────────────────────────────────────── */

esp_err_t p4c5_pmic_init(void* i2c_bus)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_bus = (i2c_master_bus_handle_t)i2c_bus;
    ESP_LOGI(TAG, "=== AXP2101 PMIC init (addr=0x%02X) ===", P4C5_PMIC_I2C_ADDR);

    /* Step 1: 创建 I2C device */
    ESP_RETURN_ON_ERROR(create_i2c_device(), TAG, "I2C device creation failed");

    /* Step 2: 调用 pmic_axp2101 初始化（创建内部 device handle） */
    ESP_RETURN_ON_ERROR(axp2101_init(s_i2c_bus), TAG, "axp2101_init failed");

    /* Step 3: 检查芯片 ID */
    ESP_RETURN_ON_ERROR(axp2101_check_chip_id(), TAG, "Chip ID check failed");
    ESP_LOGI(TAG, "AXP2101 chip ID verified (0x4A)");

    /* Step 4: 配置电源轨 */
    ESP_LOGI(TAG, "Configuring power rails:");

    /* DCDC1 = 3.3V (主电源) */
    ESP_RETURN_ON_ERROR(axp2101_set_dcdc_voltage(AXP2101_DCDC1, 3.3f),
                        TAG, "DCDC1 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_dcdc_enabled(AXP2101_DCDC1, true),
                        TAG, "DCDC1 enable failed");
    ESP_LOGI(TAG, "  DCDC1 = 3.3V ✅");

    /* ALDO1 = 1.8V (辅助) */
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO1, 1.8f),
                        TAG, "ALDO1 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO1, true),
                        TAG, "ALDO1 enable failed");
    ESP_LOGI(TAG, "  ALDO1 = 1.8V ✅");

    /* ALDO3 = 3.3V (音频 codec) */
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO3, 3.3f),
                        TAG, "ALDO3 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO3, true),
                        TAG, "ALDO3 enable failed");
    ESP_LOGI(TAG, "  ALDO3 = 3.3V ✅");

    /* ALDO4 = 2.9V (4G 模组 VBAT)
     * ⚠️ R2.1: 2.9V 可能不够 ML307C 标称 3.4-4.2V
     *    ML307C-DC-CN 工作电压范围 VBAT: 3.4V ~ 4.4V (typ)
     *    但酷世原理图确认 ALDO4 输出 2.9V
     *    → M1 必须实测 ML307C 在 2.9V 下能否正常拨号 */
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f),
                        TAG, "ALDO4 voltage failed");
    ESP_RETURN_ON_ERROR(axp2101_set_ldo_enabled(AXP2101_LDO_ALDO4, true),
                        TAG, "ALDO4 enable failed");
    ESP_LOGW(TAG, "  ALDO4 = 2.9V ✅ (⚠️ R2.1: 可能不够 ML307C 3.4V min)");

    /* 等待电源稳定 */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Step 5: 写入 14 个特殊寄存器（含 0x64 修正） */
    ESP_RETURN_ON_ERROR(write_special_registers(), TAG, "Special registers write failed");

    /* Step 6: 读回验证 */
    verify_registers();

    /* Step 7: 使能 ADC 通道 */
    axp2101_enable_pmu_adc_channels();
    axp2101_set_battery_voltage_measurement_enabled(true);
    axp2101_set_system_voltage_measurement_enabled(true);
    ESP_LOGI(TAG, "ADC channels enabled (Vbat, Vsys, die temp)");

    s_initialized = true;
    ESP_LOGI(TAG, "=== AXP2101 PMIC init complete ===");
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：反初始化
 * ────────────────────────────────────────────── */

void p4c5_pmic_deinit(void)
{
    if (!s_initialized) return;

    ESP_LOGI(TAG, "Shutting down PMIC outputs...");

    /* 关闭所有输出 */
    axp2101_set_dcdc_enabled(AXP2101_DCDC1, false);
    axp2101_set_ldo_enabled(AXP2101_LDO_ALDO1, false);
    axp2101_set_ldo_enabled(AXP2101_LDO_ALDO3, false);
    axp2101_set_ldo_enabled(AXP2101_LDO_ALDO4, false);

    if (s_i2c_dev) {
        i2c_master_bus_rm_device(s_i2c_dev);
        s_i2c_dev = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "PMIC deinitialized");
}

/* ──────────────────────────────────────────────
 * 公开 API：电池电量
 *
 * 方法：
 *   1. 优先读库仑计 (reg 0xA4, BATTERY_PERCENTAGE)
 *   2. 回退到电压法：Vbat → 线性映射 3.0V=0%, 4.2V=100%
 * ────────────────────────────────────────────── */

uint8_t p4c5_pmic_get_battery_level(void)
{
    if (!s_initialized) return 0;

    /* 方法 1：库仑计 */
    uint8_t soc = pmic_i2c_read_reg(0xA4);
    if (soc != 0xFF && soc <= 100) {
        return soc;
    }

    /* 方法 2：电压法估算 */
    uint16_t vbat = p4c5_pmic_get_vbat_mv();
    if (vbat == 0) return 0;

    /* 线性映射: 3000mV=0%, 4200mV=100% */
    if (vbat <= 3000) return 0;
    if (vbat >= 4200) return 100;
    return (uint8_t)((vbat - 3000) * 100 / 1200);
}

/* ──────────────────────────────────────────────
 * 公开 API：充电状态
 *
 * PMU_STATUS_2 (reg 0x01):
 *   [2:0] 充电阶段:
 *     000 = 未充电/待机
 *     001 = 预充电 (Pre-charge)
 *     010 = 恒流充电 (Constant Current)
 *     011 = 恒压充电 (Constant Voltage)
 *     100 = 充电完成 (Charge Done)
 *     101 = 过温降额
 * ────────────────────────────────────────────── */

bool p4c5_pmic_is_charging(void)
{
    if (!s_initialized) return false;

    uint8_t status2 = pmic_i2c_read_reg(0x01);
    uint8_t phase = status2 & 0x07;

    /* 001(预充) / 010(恒流) / 011(恒压) → 充电中 */
    return (phase >= 1 && phase <= 3);
}

/* ──────────────────────────────────────────────
 * 公开 API：放电状态
 *
 * PMU_STATUS_2 (reg 0x01):
 *   [6:5] 电池电流方向:
 *     00 = 待机
 *     01 = 充电
 *     10 = 放电
 *     11 = 保留
 * ────────────────────────────────────────────── */

bool p4c5_pmic_is_discharging(void)
{
    if (!s_initialized) return false;

    uint8_t status2 = pmic_i2c_read_reg(0x01);
    uint8_t cur_dir = (status2 >> 5) & 0x03;

    /* 10 = 放电 */
    return (cur_dir == 2);
}

/* ──────────────────────────────────────────────
 * 公开 API：电池电压 (mV)
 *
 * ADC 寄存器:
 *   0x34 = VBAT 高字节 [7:0]
 *   0x35 = VBAT 低字节 [7:5], [4:0] = 0
 * 公式: Vbat(mV) = (0x34 << 5) | (0x35 >> 3)
 *   单位: 1mV/LSB
 * ────────────────────────────────────────────── */

uint16_t p4c5_pmic_get_vbat_mv(void)
{
    if (!s_initialized) return 0;

    uint8_t hi = pmic_i2c_read_reg(0x34);
    uint8_t lo = pmic_i2c_read_reg(0x35);

    /* Vbat(mV) = (hi << 5) | (lo >> 3) */
    uint16_t vbat = ((uint16_t)hi << 5) | (lo >> 3);
    return vbat;
}

/* ──────────────────────────────────────────────
 * 公开 API：芯片温度 (°C × 10)
 *
 * ADC 寄存器:
 *   0x3C = die temp 高字节
 *   0x3D = die temp 低字节
 * 公式: Temp(°C) = ((0x3C << 5) | (0x3D >> 3)) - 144.7
 *   单位: 0.1°C/LSB
 * ────────────────────────────────────────────── */

int16_t p4c5_pmic_get_die_temp_x10(void)
{
    if (!s_initialized) return 0;

    uint8_t hi = pmic_i2c_read_reg(0x3C);
    uint8_t lo = pmic_i2c_read_reg(0x3D);

    int16_t raw = ((int16_t)hi << 5) | (lo >> 3);
    /* Temp(°C) = (raw - 144.7) * 0.1, 返回 °C × 10 */
    int16_t temp_x10 = raw - 1447;
    return temp_x10;
}

/* ──────────────────────────────────────────────
 * 公开 API：4G 电源控制 (ALDO4)
 * ────────────────────────────────────────────── */

esp_err_t p4c5_pmic_set_4g_power(bool on)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    esp_err_t err;
    if (on) {
        /* 确保 ALDO4 = 2.9V 且使能 */
        err = axp2101_set_ldo_voltage(AXP2101_LDO_ALDO4, 2.9f);
        if (err != ESP_OK) return err;
        err = axp2101_set_ldo_enabled(AXP2101_LDO_ALDO4, true);
        if (err != ESP_OK) return err;
        s_4g_power_on = true;
        ESP_LOGI(TAG, "4G power ON (ALDO4=2.9V)");
    } else {
        err = axp2101_set_ldo_enabled(AXP2101_LDO_ALDO4, false);
        if (err != ESP_OK) return err;
        s_4g_power_on = false;
        ESP_LOGI(TAG, "4G power OFF (ALDO4 disabled, ~15mA saved)");
    }
    return ESP_OK;
}

/* ──────────────────────────────────────────────
 * 公开 API：寄存器读/写
 * ────────────────────────────────────────────── */

uint8_t p4c5_pmic_read_reg(uint8_t reg_addr)
{
    return pmic_i2c_read_reg(reg_addr);
}

esp_err_t p4c5_pmic_write_reg(uint8_t reg_addr, uint8_t value)
{
    return pmic_i2c_write_reg(reg_addr, value);
}

/* ──────────────────────────────────────────────
 * 公开 API：芯片 ID 检查
 * ────────────────────────────────────────────── */

esp_err_t p4c5_pmic_check_id(void)
{
    return axp2101_check_chip_id();
}

/* ──────────────────────────────────────────────
 * 公开 API：打印所有 14 个特殊寄存器
 * ────────────────────────────────────────────── */

void p4c5_pmic_dump_all_regs(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "PMIC not initialized");
        return;
    }

    ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    ESP_LOGI(TAG, " AXP2101 Register Dump (%d special registers)", INIT_SEQ_SIZE);
    ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    ESP_LOGI(TAG, " %-4s %-4s %-6s %s", "ADDR", "VAL", "STATUS", "DESCRIPTION");
    ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    for (int i = 0; i < (int)INIT_SEQ_SIZE; i++) {
        const pmic_reg_entry_t* e = &s_init_seq[i];
        uint8_t actual = pmic_i2c_read_reg(e->reg);

        const char* status;
        if (e->is_read_modify_write) {
            status = ((actual & e->val) == e->val) ? "✅" : "⚠️";
        } else {
            status = (actual == e->val) ? "✅" : "⚠️";
        }

        ESP_LOGI(TAG, " 0x%02X 0x%02X  %s    %s — %s",
                 e->reg, actual, status, e->name, e->desc);

        if (e->risk) {
            ESP_LOGW(TAG, "        ⚠️ %s", e->risk);
        }
    }

    /* 额外打印关键状态寄存器 */
    ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    ESP_LOGI(TAG, " Status registers:");

    uint8_t pmu1 = pmic_i2c_read_reg(0x00);
    uint8_t pmu2 = pmic_i2c_read_reg(0x01);
    uint8_t chip_id = pmic_i2c_read_reg(0x03);

    ESP_LOGI(TAG, "  0x00 PMU_STATUS_1 = 0x%02X  (battery_present=%d)",
             pmu1, (pmu1 >> 3) & 1);
    ESP_LOGI(TAG, "  0x01 PMU_STATUS_2 = 0x%02X  (charge_phase=%d, cur_dir=%d)",
             pmu2, pmu2 & 0x07, (pmu2 >> 5) & 0x03);
    ESP_LOGI(TAG, "  0x03 CHIP_ID      = 0x%02X  (expected 0x4A)", chip_id);

    uint16_t vbat = p4c5_pmic_get_vbat_mv();
    int16_t temp = p4c5_pmic_get_die_temp_x10();
    uint8_t soc = p4c5_pmic_get_battery_level();

    ESP_LOGI(TAG, "  Vbat = %d mV, Die temp = %d.%d°C, SOC = %d%%",
             vbat, temp / 10, (temp >= 0 ? temp % 10 : (-temp) % 10), soc);

    ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}
