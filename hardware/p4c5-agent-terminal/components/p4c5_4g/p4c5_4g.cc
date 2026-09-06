/**
 * @file p4c5_4g.cc
 * @brief P4C5 4G 模组完整实现 — ML307C via esp-ml307
 *
 * 基于 xiaozhi main/boards/common/ml307_board.cc 移植。
 * 使用 78/esp-ml307 AtModem 进行模组检测、网络注册、状态查询。
 *
 * 初始化流程（后台任务）：
 *   1. PWR_GPIO 拉高上电
 *   2. DTR_GPIO 初始化
 *   3. AtModem::Detect(TX, RX, DTR, 921600)
 *   4. WaitForNetworkReady()（等待 SIM + 网络注册）
 *   5. 回调通知上层
 */

#include "p4c5_4g.h"
#include "config.h"

#include <esp_log.h>
#include <esp_check.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* esp-ml307 C++ API */
#include "at_modem.h"
#include "network_interface.h"

#include <cstring>
#include <string>

static const char *TAG = "p4c5_4g";

/* ── 内部状态 ── */
static bool s_initialized = false;
static std::unique_ptr<AtModem> s_modem;
static p4c5_4g_event_cb_t s_event_cb = nullptr;
static void *s_event_cb_data = nullptr;

/* 缓存字符串（避免重复 AT 查询） */
static char s_imei[32] = {0};
static char s_iccid[32] = {0};
static char s_carrier[32] = {0};
static char s_revision[32] = {0};

/* 最大重试 */
static constexpr int MODEM_DETECT_MAX_RETRIES = 10;
static constexpr int NETWORK_REG_MAX_RETRIES = 6;

/* ── 内部：事件通知 ── */
static void notify_event(p4c5_4g_event_t event, const char *data = nullptr)
{
    if (s_event_cb) {
        s_event_cb(event, data, s_event_cb_data);
    }
}

/* ── 内部：硬件初始化 ── */
static esp_err_t power_on(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << P4C5_4G_PWR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "PWR gpio_config failed");
    gpio_set_level(P4C5_4G_PWR_GPIO, 1);
    ESP_LOGI(TAG, "ML307C power ON (GPIO%d)", P4C5_4G_PWR_GPIO);

    /* ML307C 冷启动约 3-5s */
    vTaskDelay(pdMS_TO_TICKS(3000));
    return ESP_OK;
}

static esp_err_t init_dtr(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << P4C5_4G_DTR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "DTR gpio_config failed");
    gpio_set_level(P4C5_4G_DTR_GPIO, 0);  /* DTR=0: 正常工作 */
    return ESP_OK;
}

/* ── 后台网络任务 ── */
static void network_task(void *arg)
{
    /* 阶段 1: 检测模组 */
    notify_event(P4C5_4G_EVENT_MODEM_DETECTING);

    int retries = 0;
    while (retries < MODEM_DETECT_MAX_RETRIES) {
        ESP_LOGI(TAG, "Detecting modem (attempt %d/%d)...", retries + 1, MODEM_DETECT_MAX_RETRIES);
        s_modem = AtModem::Detect(
            P4C5_4G_TX_GPIO,
            P4C5_4G_RX_GPIO,
            P4C5_4G_DTR_GPIO,
            P4C5_4G_BAUD_RATE
        );
        if (s_modem) {
            ESP_LOGI(TAG, "✅ Modem detected!");
            break;
        }
        retries++;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (!s_modem) {
        ESP_LOGE(TAG, "❌ Modem detection failed after %d retries", MODEM_DETECT_MAX_RETRIES);
        notify_event(P4C5_4G_ERROR_INIT_FAILED);
        vTaskDelete(nullptr);
        return;
    }

    notify_event(P4C5_4G_EVENT_MODEM_FOUND);

    /* 缓存模组信息 */
    {
        std::string rev = s_modem->GetModuleRevision();
        std::string imei = s_modem->GetImei();
        std::string iccid = s_modem->GetIccid();
        strncpy(s_revision, rev.c_str(), sizeof(s_revision) - 1);
        strncpy(s_imei, imei.c_str(), sizeof(s_imei) - 1);
        strncpy(s_iccid, iccid.c_str(), sizeof(s_iccid) - 1);

        ESP_LOGI(TAG, "ML307 Rev: %s", s_revision);
        ESP_LOGI(TAG, "IMEI: %s", s_imei);
        ESP_LOGI(TAG, "ICCID: %s", s_iccid);
    }

    /* 注册网络状态回调 */
    s_modem->OnNetworkStateChanged([](bool ready) {
        if (ready) {
            ESP_LOGI(TAG, "🌐 Network ready!");
            /* 获取运营商名称 */
            std::string carrier = s_modem->GetCarrierName();
            strncpy(s_carrier, carrier.c_str(), sizeof(s_carrier) - 1);
            notify_event(P4C5_4G_EVENT_NETWORK_READY, s_carrier);
        } else {
            ESP_LOGW(TAG, "Network disconnected");
            notify_event(P4C5_4G_EVENT_DISCONNECTED);
        }
    });

    /* 阶段 2: 等待网络注册 */
    notify_event(P4C5_4G_EVENT_REGISTERING);

    int reg_retries = 0;
    while (reg_retries < NETWORK_REG_MAX_RETRIES) {
        auto result = s_modem->WaitForNetworkReady(30000);
        if (result == NetworkStatus::Ready) {
            break;
        } else if (result == NetworkStatus::ErrorInsertPin) {
            ESP_LOGE(TAG, "❌ No SIM card!");
            notify_event(P4C5_4G_EVENT_NO_SIM);
        } else if (result == NetworkStatus::ErrorRegistrationDenied) {
            ESP_LOGE(TAG, "❌ Registration denied!");
            notify_event(P4C5_4G_EVENT_REG_DENIED);
        } else if (result == NetworkStatus::ErrorTimeout) {
            ESP_LOGW(TAG, "⏱ Network registration timeout (retry %d)", reg_retries + 1);
            notify_event(P4C5_4G_ERROR_TIMEOUT);
        }
        reg_retries++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    if (!s_modem->network_ready()) {
        ESP_LOGE(TAG, "❌ Network registration failed after %d retries", NETWORK_REG_MAX_RETRIES);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "✅ 4G network ready! Carrier: %s, CSQ: %d",
             s_modem->GetCarrierName().c_str(),
             s_modem->GetCsq());

    vTaskDelete(nullptr);
}

/* ══════════════════════════════════════════════════════════
 * 公开 C API
 * ══════════════════════════════════════════════════════════ */

extern "C" {

esp_err_t p4c5_4g_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Init ML307C (UART%d TX=%d RX=%d PWR=%d DTR=%d baud=%d)",
             P4C5_4G_UART_NUM, P4C5_4G_TX_GPIO, P4C5_4G_RX_GPIO,
             P4C5_4G_PWR_GPIO, P4C5_4G_DTR_GPIO, P4C5_4G_BAUD_RATE);

    ESP_RETURN_ON_ERROR(power_on(), TAG, "Power on failed");
    ESP_RETURN_ON_ERROR(init_dtr(), TAG, "DTR init failed");

    /* 启动后台网络任务 */
    BaseType_t ret = xTaskCreate(network_task, "4g_net", 8192, nullptr, 5, nullptr);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "4G init started (network registration in background)");
    return ESP_OK;
}

void p4c5_4g_deinit(void)
{
    if (!s_initialized) return;

    s_modem.reset();
    gpio_set_level(P4C5_4G_PWR_GPIO, 0);
    s_initialized = false;
    ESP_LOGI(TAG, "4G deinitialized");
}

bool p4c5_4g_is_modem_detected(void)
{
    return s_modem != nullptr;
}

bool p4c5_4g_is_network_ready(void)
{
    return s_modem && s_modem->network_ready();
}

int p4c5_4g_get_csq(void)
{
    if (!s_modem) return -1;
    return s_modem->GetCsq();
}

int p4c5_4g_get_rssi_dbm(void)
{
    int csq = p4c5_4g_get_csq();
    if (csq < 0 || csq > 31) return 0;
    /* CSQ → dBm: rssi = -113 + 2 * csq */
    return -113 + 2 * csq;
}

const char *p4c5_4g_get_imei(void)
{
    return s_imei;
}

const char *p4c5_4g_get_iccid(void)
{
    return s_iccid;
}

const char *p4c5_4g_get_carrier(void)
{
    return s_carrier;
}

const char *p4c5_4g_get_module_revision(void)
{
    return s_revision;
}

void p4c5_4g_set_event_callback(p4c5_4g_event_cb_t cb, void *user_data)
{
    s_event_cb = cb;
    s_event_cb_data = user_data;
}

esp_err_t p4c5_4g_sleep(void)
{
    gpio_set_level(P4C5_4G_DTR_GPIO, 1);
    ESP_LOGI(TAG, "Modem sleep (DTR=1)");
    return ESP_OK;
}

esp_err_t p4c5_4g_wake(void)
{
    gpio_set_level(P4C5_4G_DTR_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Modem wake (DTR=0)");
    return ESP_OK;
}

}  /* extern "C" */

/* ══════════════════════════════════════════════════════════
 * C++ 内部接口（供 ML307 transport 使用）
 * ══════════════════════════════════════════════════════════ */

namespace p4c5_4g_internal {

AtModem *get_modem()
{
    return s_modem.get();
}

NetworkInterface *get_network()
{
    return s_modem.get();
}

}  /* namespace p4c5_4g_internal */
