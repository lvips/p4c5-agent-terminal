/**
 * @file p4c5_4g.h
 * @brief P4C5 4G 模组 — ML307C Cat.1（完整实现）
 *
 * 基于 78/esp-ml307 v3.6.4 协议栈。
 * 内部使用 AtModem::Detect() + WaitForNetworkReady()。
 * 提供网络状态回调 + esp-ml307 NetworkInterface 访问。
 *
 * ⚠️ ALDO4=2.9V vs ML307C 标称 3.4-4.2V — 需实测确认。
 */

#pragma once

#include "esp_err.h"
#include "dsh_client_transport.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 网络事件 ── */
typedef enum {
    P4C5_4G_EVENT_MODEM_DETECTING,    /* 正在检测模组 */
    P4C5_4G_EVENT_MODEM_FOUND,        /* 模组已检测到 */
    P4C5_4G_EVENT_REGISTERING,        /* 正在注册网络 */
    P4C5_4G_EVENT_NETWORK_READY,      /* 网络就绪 */
    P4C5_4G_EVENT_DISCONNECTED,       /* 网络断开 */
    P4C5_4G_EVENT_NO_SIM,             /* 无 SIM 卡 */
    P4C5_4G_EVENT_REG_DENIED,         /* 注册被拒绝 */
    P4C5_4G_ERROR_INIT_FAILED,        /* 模组初始化失败 */
    P4C5_4G_ERROR_TIMEOUT,            /* 超时 */
} p4c5_4g_event_t;

/**
 * 网络事件回调
 * @param event  事件类型
 * @param data   附加数据（如运营商名、IMSI 等，可为 NULL）
 * @param user_data 用户数据
 */
typedef void (*p4c5_4g_event_cb_t)(p4c5_4g_event_t event, const char *data, void *user_data);

/* ══════════════════════════════════════════════════════════
 * 生命周期
 * ══════════════════════════════════════════════════════════ */

/**
 * 初始化 4G 模组（非阻塞）
 *
 * 步骤：
 *   1. 拉高 PWR_GPIO 上电
 *   2. 配置 DTR_GPIO
 *   3. 启动后台任务：AtModem::Detect → WaitForNetworkReady
 *
 * @return ESP_OK 启动成功（网络注册在后台进行）
 */
esp_err_t p4c5_4g_init(void);

/**
 * 反初始化（断电 + 释放资源）
 */
void p4c5_4g_deinit(void);

/* ══════════════════════════════════════════════════════════
 * 状态查询
 * ══════════════════════════════════════════════════════════ */

/** 模组是否已检测到 */
bool p4c5_4g_is_modem_detected(void);

/** 网络是否就绪（已注册 + PDP 可用） */
bool p4c5_4g_is_network_ready(void);

/** 获取信号强度 CSQ（0-31, -1=无效） */
int p4c5_4g_get_csq(void);

/** 获取 CSQ 转 RSSI（dBm），无信号返回 0 */
int p4c5_4g_get_rssi_dbm(void);

/** 获取 IMEI（静态缓冲区，不需要 free） */
const char *p4c5_4g_get_imei(void);

/** 获取 ICCID */
const char *p4c5_4g_get_iccid(void);

/** 获取运营商名称 */
const char *p4c5_4g_get_carrier(void);

/** 获取模组固件版本 */
const char *p4c5_4g_get_module_revision(void);

/* ══════════════════════════════════════════════════════════
 * 回调
 * ══════════════════════════════════════════════════════════ */

/**
 * 注册网络事件回调
 * @param cb   回调函数
 * @param user_data 用户数据
 */
void p4c5_4g_set_event_callback(p4c5_4g_event_cb_t cb, void *user_data);

/* ══════════════════════════════════════════════════════════
 * 电源管理
 * ══════════════════════════════════════════════════════════ */

/** 进入低功耗模式（DTR=1） */
esp_err_t p4c5_4g_sleep(void);

/** 唤醒模组（DTR=0） */
esp_err_t p4c5_4g_wake(void);

/* ══════════════════════════════════════════════════════════
 * 传输层适配器（供 dsh_client 使用）
 * ══════════════════════════════════════════════════════════ */

/**
 * 获取 ML307 WebSocket 传输层
 *
 * 返回的 dsh_transport_t 可直接传给 dsh_client_set_transport()。
 * 仅在模组已检测到时返回有效指针。
 */
const dsh_transport_t *p4c5_4g_get_transport(void);

#ifdef __cplusplus
}
#endif

/* ══════════════════════════════════════════════════════════
 * C++ 内部接口 — 仅供 ML307 transport 使用
 * ══════════════════════════════════════════════════════════ */
#ifdef __cplusplus
#include <memory>

class AtModem;
class NetworkInterface;

namespace p4c5_4g_internal {
    /** 获取 AtModem 实例（init 后有效） */
    AtModem *get_modem();
    /** 获取 NetworkInterface（用于 CreateWebSocket 等） */
    NetworkInterface *get_network();
}
#endif
