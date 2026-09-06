/**
 * @file dsh_client_heartbeat.h
 * @brief 心跳任务内部接口
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 状态查询回调 — 心跳帧需要电池和信号数据
 *
 * @param[out] battery_pct  电池百分比 0-100，-1 表示未知
 * @param[out] rssi_dbm     信号强度 dBm，0 表示未知
 */
typedef void (*dsh_status_provider_t)(int *battery_pct, int *rssi_dbm, void *user_data);

/**
 * 启动心跳任务
 *
 * @param interval_ms 心跳间隔（毫秒），建议 30000
 * @param provider    状态查询回调（可为 NULL，则 battery=-1, rssi=0）
 * @param user_data   回调用户数据
 * @return ESP_OK 成功
 */
esp_err_t dsh_heartbeat_start(uint32_t interval_ms,
                               dsh_status_provider_t provider,
                               void *user_data);

/**
 * 停止心跳任务
 */
esp_err_t dsh_heartbeat_stop(void);

/**
 * 心跳任务是否运行中
 */
int dsh_heartbeat_is_running(void);

#ifdef __cplusplus
}
#endif
