/**
 * @file p4c5_4g.h
 * @brief P4C5 4G 模组 — ML307C Cat.1
 *
 * 使用 78/esp-ml307 v3.6.4 协议栈。
 * 提供 WebSocket / TCP / HTTP 通信能力。
 *
 * ⚠️ R2.1 风险：ALDO4=2.9V 可能不够 ML307C 标称 3.4-4.2V。
 *    ML307C-DC-CN 工作电压范围 3.4-4.2V（typ），
 *    但酷世原理图确认 ALDO4 输出 2.9V → M1 必须实测。
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 4G 模组
 *
 * 步骤：
 *   1. 拉高 POWER_EN (GPIO4) 上电
 *   2. 初始化 UART (TX=53, RX=52, 115200)
 *   3. 配置 DTR (GPIO51) 用于 sleep 控制
 *   4. 通过 esp-ml307 发送 AT 指令检查模组状态
 *   5. 等待网络注册
 *
 * @return ESP_OK 成功，其他 失败
 */
esp_err_t p4c5_4g_init(void);

/**
 * @brief 建立 TCP 连接
 * @param host 服务器地址
 * @param port 端口
 * @param sock_fd 输出 socket fd
 * @return ESP_OK 成功
 */
esp_err_t p4c5_4g_tcp_connect(const char* host, uint16_t port, int* sock_fd);

/**
 * @brief 发送 TCP 数据
 * @param sock_fd socket fd
 * @param data 数据 buffer
 * @param len 数据长度
 * @return 实际发送字节数，< 0 表示错误
 */
int p4c5_4g_tcp_send(int sock_fd, const void* data, int len);

/**
 * @brief 接收 TCP 数据
 * @param sock_fd socket fd
 * @param buf 接收 buffer
 * @param len buffer 大小
 * @param timeout_ms 超时毫秒
 * @return 实际接收字节数，0=超时，< 0=错误
 */
int p4c5_4g_tcp_recv(int sock_fd, void* buf, int len, int timeout_ms);

/**
 * @brief 关闭 TCP 连接
 */
esp_err_t p4c5_4g_tcp_close(int sock_fd);

/**
 * @brief 查询信号强度 (RSSI)
 * @param rssi 输出 RSSI 值 (dBm)
 * @return ESP_OK 成功
 */
esp_err_t p4c5_4g_get_rssi(int* rssi);

/**
 * @brief 查询 SIM 卡状态
 * @param ready 输出 true=SIM ready
 * @return ESP_OK 成功
 */
esp_err_t p4c5_4g_sim_ready(bool* ready);

/**
 * @brief 进入低功耗模式（DTR 控制）
 */
esp_err_t p4c5_4g_sleep(void);

/**
 * @brief 唤醒模组
 */
esp_err_t p4c5_4g_wake(void);

/**
 * @brief 反初始化 4G 模组
 */
void p4c5_4g_deinit(void);

#ifdef __cplusplus
}
#endif
