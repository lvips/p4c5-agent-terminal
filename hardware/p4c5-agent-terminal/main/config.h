/**
 * @file config.h
 * @brief P4C5 开发板引脚 / 功能宏定义
 *
 * 数据来源：docs/hw/p4c5-pins.csv + xiaozhi kevin-p4c5-4g/config.h
 * 所有引脚均为 ESP32-P4 GPIO 编号。
 */

#pragma once

#include "driver/gpio.h"

/* ── 版本号 ── */
#define P4C5_BOARD_VERSION  "0.1.0"

/* ── LCD (ST7102 MIPI-DSI 480×800) ── */
#define P4C5_LCD_RST_GPIO         GPIO_NUM_22
#define P4C5_LCD_BL_GPIO          GPIO_NUM_6    /* PWM 背光 */
#define P4C5_LCD_WIDTH             480
#define P4C5_LCD_HEIGHT            800
#define P4C5_LCD_MIPI_DSI_LANES    2
#define P4C5_LCD_MIPI_LANE_MBPS    820
#define P4C5_LCD_MIPI_PHY_LDO_CHAN 3
#define P4C5_LCD_MIPI_PHY_LDO_MV   2500
#define P4C5_LCD_DPI_CLK_MHZ       37.8

/* ── 触摸 (ST7123 I2C) ── */
#define P4C5_TP_INT_GPIO          GPIO_NUM_23
#define P4C5_TP_I2C_PORT          I2C_NUM_0
#define P4C5_TP_I2C_ADDR          0x5A

/* ── 共享 I2C 总线 (I2C0) ── */
#define P4C5_I2C_SDA_GPIO         GPIO_NUM_7
#define P4C5_I2C_SCL_GPIO         GPIO_NUM_8
#define P4C5_I2C_PORT             I2C_NUM_0
#define P4C5_I2C_FREQ_HZ          400000

/* ── 音频 I2S (ES8311 DAC + ES7210 ADC) ── */
#define P4C5_I2S_PORT             I2S_NUM_0
#define P4C5_I2S_MCLK_GPIO        GPIO_NUM_13
#define P4C5_I2S_BCLK_GPIO        GPIO_NUM_12
#define P4C5_I2S_WS_GPIO          GPIO_NUM_10
#define P4C5_I2S_DOUT_GPIO        GPIO_NUM_9     /* DAC 数据输出 */
#define P4C5_I2S_DIN_GPIO         GPIO_NUM_11    /* ADC 数据输入 */
#define P4C5_PA_EN_GPIO           GPIO_NUM_3     /* NS4150B PA 使能 */
#define P4C5_AUDIO_SAMPLE_RATE    24000
#define P4C5_AUDIO_INPUT_REF      true           /* AEC 参考通道 */

/* ── 音频 I2C 地址 ── */
#define P4C5_ES8311_I2C_ADDR      0x18
#define P4C5_ES7210_I2C_ADDR      0x40

/* ── PMIC (AXP2101) ── */
#define P4C5_PMIC_I2C_ADDR        0x34
/* 电压配置 */
#define P4C5_PMIC_DCDC1_MV        3300   /* 主电源 3.3V */
#define P4C5_PMIC_ALDO1_MV        1800   /* 辅助 1.8V */
#define P4C5_PMIC_ALDO3_MV        3300   /* 音频 codec 3.3V */
#define P4C5_PMIC_ALDO4_MV        2900   /* 4G 模组 VBAT */

/* ── 4G 模组 (ML307C Cat.1) ── */
#define P4C5_4G_UART_NUM          UART_NUM_1
#define P4C5_4G_TX_GPIO           GPIO_NUM_53
#define P4C5_4G_RX_GPIO           GPIO_NUM_52
#define P4C5_4G_PWR_GPIO          GPIO_NUM_4
#define P4C5_4G_DTR_GPIO            GPIO_NUM_51
#define P4C5_4G_BAUD_RATE         115200

/* ── IMU (LSM6DS3TR-C) ── */
#define P4C5_IMU_I2C_ADDR         0x6A
/* INT1/INT2 未连接到 GPIO（原理图确认） */

/* ── DAC (MCP4725) ── */
#define P4C5_DAC_I2C_ADDR         0x60

/* ── LED ── */
#define P4C5_LED_GPIO             GPIO_NUM_34   /* 单色状态 LED */
#define P4C5_WS2812_GPIO          GPIO_NUM_21   /* RGB LED (RMT) */

/* ── 按键 ── */
#define P4C5_BOOT_BUTTON_GPIO     GPIO_NUM_35
#define P4C5_USER_BUTTON_GPIO     GPIO_NUM_0

/* ── USB ── */
#define P4C5_USB_VBUS_GPIO        GPIO_NUM_50   /* VBUS 检测 */

/* ── SD 卡 (SDMMC) ── */
/* SD 引脚由 ESP-IDF SDMMC 驱动自动配置，此处不列 */

/* ── RS485 ── */
#define P4C5_RS485_TX_GPIO        GPIO_NUM_31
#define P4C5_RS485_RX_GPIO        GPIO_NUM_33
#define P4C5_RS485_DE_GPIO        GPIO_NUM_32

/* ── Camera MIPI-CSI ── */
/* CSI 引脚由 esp_video 组件管理，此处暂不定义 */

/* ── ESP-Hosted (P4↔C5 SDIO) ── */
/* SDIO 引脚由 esp_hosted 组件管理，此处暂不定义 */
