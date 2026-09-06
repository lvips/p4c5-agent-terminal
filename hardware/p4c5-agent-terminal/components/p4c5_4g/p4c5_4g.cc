/**
 * @file p4c5_4g.cc
 * @brief P4C5 4G 模组实现 — ML307C via esp-ml307
 *
 * 基于 xiaozhi main/boards/common/ml307_board.cc 移植。
 * 使用 78/esp-ml307 v3.6.4 协议栈。
 *
 * 参考：
 *   - xiaozhi: main/boards/common/ml307_board.cc
 *   - xiaozhi: main/boards/kevin-p4c5-4g/ (Enable4GModule)
 *   - 78/esp-ml307: https://github.com/78/esp-ml307
 *
 * ⚠️ R2.1 风险：ALDO4=2.9V 供电 vs ML307C 标称 3.4-4.2V。
 *    酷世原理图确认为 2.9V，M1 阶段必须实测通信稳定性。
 */

#include "p4c5_4g.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "p4c5_4g";

/* ── 内部状态 ── */
static bool s_initialized = false;
static bool s_network_ready = false;

/* ── 内部函数 ── */

static esp_err_t power_on_module(void)
{
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = (1ULL << P4C5_4G_PWR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pwr_cfg), TAG, "POWER gpio_config failed");
    gpio_set_level(P4C5_4G_PWR_GPIO, 1);  /* 高电平使能 */
    ESP_LOGI(TAG, "ML307C power enabled (GPIO %d)", P4C5_4G_PWR_GPIO);

    /* 等待模组启动（ML307C 冷启动约 5-10s） */
    vTaskDelay(pdMS_TO_TICKS(3000));
    return ESP_OK;
}

static esp_err_t init_dtr(void)
{
    gpio_config_t dtr_cfg = {
        .pin_bit_mask = (1ULL << P4C5_4G_DTR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&dtr_cfg), TAG, "DTR gpio_config failed");
    gpio_set_level(P4C5_4G_DTR_GPIO, 0);  /* DTR=0: 正常工作模式 */
    return ESP_OK;
}

static esp_err_t init_uart(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = P4C5_4G_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(P4C5_4G_UART_NUM, 1024, 1024, 0, NULL, 0),
                        TAG, "UART install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(P4C5_4G_UART_NUM, &uart_cfg),
                        TAG, "UART param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(P4C5_4G_UART_NUM,
                                     P4C5_4G_TX_GPIO, P4C5_4G_RX_GPIO,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "UART set pin failed");

    ESP_LOGI(TAG, "UART%d initialized: TX=%d, RX=%d, baud=%d",
             P4C5_4G_UART_NUM, P4C5_4G_TX_GPIO, P4C5_4G_RX_GPIO, P4C5_4G_BAUD_RATE);
    return ESP_OK;
}

static esp_err_t check_at_response(void)
{
    /* 发送 AT 指令检查模组是否就绪 */
    const char* at_cmd = "AT\r\n";
    uart_write_bytes(P4C5_4G_UART_NUM, at_cmd, strlen(at_cmd));

    char resp[64] = {};
    int len = uart_read_bytes(P4C5_4G_UART_NUM, resp, sizeof(resp) - 1, pdMS_TO_TICKS(2000));
    if (len > 0) {
        resp[len] = '\0';
        ESP_LOGI(TAG, "AT response: %s", resp);
    } else {
        ESP_LOGW(TAG, "No AT response — modem may still be booting");
    }
    // TODO: 解析 AT 响应，确认模组就绪
    return ESP_OK;
}

/* ── 公开 API ── */

esp_err_t p4c5_4g_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing 4G modem: ML307C (UART%d, TX=%d, RX=%d, PWR=%d, DTR=%d)",
             P4C5_4G_UART_NUM, P4C5_4G_TX_GPIO, P4C5_4G_RX_GPIO,
             P4C5_4G_PWR_GPIO, P4C5_4G_DTR_GPIO);

    ESP_RETURN_ON_ERROR(power_on_module(), TAG, "Power on failed");
    ESP_RETURN_ON_ERROR(init_dtr(), TAG, "DTR init failed");
    ESP_RETURN_ON_ERROR(init_uart(), TAG, "UART init failed");
    ESP_RETURN_ON_ERROR(check_at_response(), TAG, "AT check failed");

    // TODO: 使用 esp-ml307 高层 API 进行网络注册
    // Ml307Board::InitializeMl307() 中完成 TCP/IP 协议栈初始化

    s_initialized = true;
    ESP_LOGI(TAG, "4G modem initialized (AT check pending)");
    return ESP_OK;
}

esp_err_t p4c5_4g_tcp_connect(const char* host, uint16_t port, int* sock_fd)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    // TODO: 使用 esp-ml307 TCP API 建立连接
    (void)host; (void)port; (void)sock_fd;
    ESP_LOGW(TAG, "tcp_connect(%s:%d) — TODO: implement via esp-ml307", host, port);
    return ESP_ERR_NOT_SUPPORTED;
}

int p4c5_4g_tcp_send(int sock_fd, const void* data, int len)
{
    // TODO: 使用 esp-ml307 TCP API 发送数据
    (void)sock_fd; (void)data; (void)len;
    ESP_LOGW(TAG, "tcp_send — TODO: implement");
    return -1;
}

int p4c5_4g_tcp_recv(int sock_fd, void* buf, int len, int timeout_ms)
{
    // TODO: 使用 esp-ml307 TCP API 接收数据
    (void)sock_fd; (void)buf; (void)len; (void)timeout_ms;
    ESP_LOGW(TAG, "tcp_recv — TODO: implement");
    return -1;
}

esp_err_t p4c5_4g_tcp_close(int sock_fd)
{
    // TODO: 使用 esp-ml307 TCP API 关闭连接
    (void)sock_fd;
    ESP_LOGW(TAG, "tcp_close — TODO: implement");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t p4c5_4g_get_rssi(int* rssi)
{
    if (!rssi) return ESP_ERR_INVALID_ARG;
    // TODO: 发送 AT+CSQ 查询信号强度
    *rssi = -1;  // 占位
    ESP_LOGW(TAG, "get_rssi — TODO: implement AT+CSQ");
    return ESP_OK;
}

esp_err_t p4c5_4g_sim_ready(bool* ready)
{
    if (!ready) return ESP_ERR_INVALID_ARG;
    // TODO: 发送 AT+CPIN? 查询 SIM 状态
    *ready = false;  // 占位
    ESP_LOGW(TAG, "sim_ready — TODO: implement AT+CPIN?");
    return ESP_OK;
}

esp_err_t p4c5_4g_sleep(void)
{
    gpio_set_level(P4C5_4G_DTR_GPIO, 1);  /* DTR=1: 进入 sleep */
    ESP_LOGI(TAG, "4G modem entering sleep mode");
    return ESP_OK;
}

esp_err_t p4c5_4g_wake(void)
{
    gpio_set_level(P4C5_4G_DTR_GPIO, 0);  /* DTR=0: 唤醒 */
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "4G modem woke from sleep");
    return ESP_OK;
}

void p4c5_4g_deinit(void)
{
    if (!s_initialized) return;
    gpio_set_level(P4C5_4G_PWR_GPIO, 0);  /* 断电 */
    uart_driver_delete(P4C5_4G_UART_NUM);
    ESP_LOGI(TAG, "4G modem deinitialized");
    s_initialized = false;
}
