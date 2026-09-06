/**
 * @file dsh_client_frames.h
 * @brief DSH 协议帧类型常量 + 帧构造/解析 API
 *
 * 基于 CCB T8 调研结果：JSON-over-WebSocket 协议
 * 参考文档：docs/hw/dsh-client-frame-protocol.md
 */

#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 客户端版本号 ── */
#define DSH_CLIENT_VERSION  "0.1.0"

/* ══════════════════════════════════════════════════════════
 * 下行帧类型（服务端 → 客户端，14 类）
 * ══════════════════════════════════════════════════════════ */
#define DSH_FRAME_SESSION_STATE         "session_state"
#define DSH_FRAME_THINKING              "thinking"
#define DSH_FRAME_ASSISTANT_TEXT        "assistant_text"
#define DSH_FRAME_ASSISTANT_DONE        "assistant_done"
#define DSH_FRAME_TOOL_CALL             "tool_call"
#define DSH_FRAME_TOOL_RESULT           "tool_result"       /* 服务端中继 */
#define DSH_FRAME_TOOL_PROGRESS         "tool_progress"
#define DSH_FRAME_FILE_CHANGE           "file_change"
#define DSH_FRAME_AUTH_REQUESTED        "authorization/requested"
#define DSH_FRAME_AUTH_RESOLVED         "authorization/resolved"
#define DSH_FRAME_QUESTION_REQUESTED    "question/requested"
#define DSH_FRAME_QUESTION_RESOLVED     "question/resolved"
#define DSH_FRAME_FILE_UPLOAD           "file_upload"
#define DSH_FRAME_SYSTEM_EVENT          "system_event"

/* ══════════════════════════════════════════════════════════
 * 上行帧类型（客户端 → 服务端，4 类）
 * ══════════════════════════════════════════════════════════ */
#define DSH_FRAME_CLIENT_HELLO          "client/hello"
#define DSH_FRAME_CLIENT_HEARTBEAT      "client/heartbeat"
#define DSH_FRAME_CLIENT_TOOL_RESULT    "client/tool_result"
#define DSH_FRAME_CLIENT_USER_INPUT     "client/user_input"

/* ══════════════════════════════════════════════════════════
 * 帧构造 API（上行帧）
 *
 * 所有函数返回 cJSON*，调用者负责 cJSON_Delete()
 * ══════════════════════════════════════════════════════════ */

/**
 * 构造 client/hello 帧
 *
 * @param device_id     设备标识（如 "p4c5-001"）
 * @param version       固件版本（如 "0.1.0"）
 * @param capabilities  JSON 数组 ["audio","display","touch","4g"]
 * @param auth_token    认证令牌（可为 NULL）
 * @return cJSON* 必须 cJSON_Delete()
 */
cJSON *dsh_frame_build_hello(const char *device_id, const char *version,
                              cJSON *capabilities, const char *auth_token);

/**
 * 构造 client/heartbeat 帧
 *
 * @param battery_pct   电池百分比 0-100（-1 表示未知）
 * @param rssi_dbm      信号强度 dBm（0 表示未知）
 * @return cJSON* 必须 cJSON_Delete()
 */
cJSON *dsh_frame_build_heartbeat(int battery_pct, int rssi_dbm);

/**
 * 构造 client/tool_result 帧
 *
 * @param tool_id       工具调用 ID（来自 tool_call 帧的 "id" 字段）
 * @param status        "success" / "error" / "partial"
 * @param result        执行结果（可为 NULL）
 * @param error_msg     错误信息（可为 NULL）
 * @return cJSON* 必须 cJSON_Delete()
 */
cJSON *dsh_frame_build_tool_result(const char *tool_id, const char *status,
                                    const char *result, const char *error_msg);

/**
 * 构造 client/user_input 帧
 *
 * @param text          用户输入文本
 * @return cJSON* 必须 cJSON_Delete()
 */
cJSON *dsh_frame_build_user_input(const char *text);

/**
 * 构造通用帧（用于自定义帧类型）
 *
 * @param frame_type    帧类型字符串
 * @param payload       附加字段（cJSON object，可为 NULL）
 * @return cJSON* 必须 cJSON_Delete()
 */
cJSON *dsh_frame_build_generic(const char *frame_type, cJSON *payload);

/* ══════════════════════════════════════════════════════════
 * 帧解析 API
 * ══════════════════════════════════════════════════════════ */

/**
 * 从 JSON 文本中提取帧类型
 *
 * @param json_text     JSON 字符串
 * @param[out] type     帧类型字符串指针（指向 json 内部，不要 free）
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 解析失败
 */
esp_err_t dsh_frame_parse_type(const char *json_text, const char **type);

/**
 * 将 cJSON 序列化为 JSON 字符串
 *
 * @param json          cJSON 对象
 * @param[out] out_text 输出字符串（调用者 free()）
 * @param[out] out_len  输出长度（可为 NULL）
 * @return ESP_OK 成功
 */
esp_err_t dsh_frame_serialize(cJSON *json, char **out_text, int *out_len);

/**
 * 判断帧类型是否为下行帧
 */
int dsh_frame_is_downstream(const char *type);

/**
 * 判断帧类型是否为上行帧
 */
int dsh_frame_is_upstream(const char *type);

#ifdef __cplusplus
}
#endif
