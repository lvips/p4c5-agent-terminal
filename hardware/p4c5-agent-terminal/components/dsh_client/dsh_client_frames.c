/**
 * @file dsh_client_frames.c
 * @brief DSH 协议帧构造 + 解析
 *
 * 所有帧均为 JSON TEXT 格式：{"type":"xxx", ...}
 */

#include "dsh_client_frames.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════
 * 上行帧构造
 * ══════════════════════════════════════════════════════════ */

cJSON *dsh_frame_build_hello(const char *device_id, const char *version,
                              cJSON *capabilities, const char *auth_token)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", DSH_FRAME_CLIENT_HELLO);
    cJSON_AddStringToObject(json, "device_id", device_id ? device_id : "unknown");
    cJSON_AddStringToObject(json, "version", version ? version : DSH_CLIENT_VERSION);

    if (capabilities && cJSON_IsArray(capabilities)) {
        cJSON_AddItemToObject(json, "capabilities", cJSON_Duplicate(capabilities, 1));
    } else {
        /* 默认能力集 */
        cJSON *caps = cJSON_CreateArray();
        if (caps) {
            cJSON_AddItemToArray(caps, cJSON_CreateString("audio"));
            cJSON_AddItemToArray(caps, cJSON_CreateString("display"));
            cJSON_AddItemToArray(caps, cJSON_CreateString("touch"));
            cJSON_AddItemToArray(caps, cJSON_CreateString("4g"));
            cJSON_AddItemToObject(json, "capabilities", caps);
        }
    }

    if (auth_token) {
        cJSON_AddStringToObject(json, "auth_token", auth_token);
    }

    return json;
}

cJSON *dsh_frame_build_heartbeat(int battery_pct, int rssi_dbm)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", DSH_FRAME_CLIENT_HEARTBEAT);
    cJSON_AddNumberToObject(json, "timestamp", (double)time(NULL));
    cJSON_AddNumberToObject(json, "battery", battery_pct);
    cJSON_AddNumberToObject(json, "rssi", rssi_dbm);

    return json;
}

cJSON *dsh_frame_build_tool_result(const char *tool_id, const char *status,
                                    const char *result, const char *error_msg)
{
    if (!tool_id) {
        ESP_LOGE("dsh_frames", "tool_id is NULL");
        return NULL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", DSH_FRAME_CLIENT_TOOL_RESULT);
    cJSON_AddStringToObject(json, "id", tool_id);
    cJSON_AddStringToObject(json, "status", status ? status : "success");

    if (result) {
        cJSON_AddStringToObject(json, "result", result);
    }
    if (error_msg) {
        cJSON_AddStringToObject(json, "error", error_msg);
    }

    return json;
}

cJSON *dsh_frame_build_user_input(const char *text)
{
    if (!text) {
        ESP_LOGE("dsh_frames", "text is NULL");
        return NULL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", DSH_FRAME_CLIENT_USER_INPUT);
    cJSON_AddStringToObject(json, "text", text);

    return json;
}

cJSON *dsh_frame_build_generic(const char *frame_type, cJSON *payload)
{
    if (!frame_type) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", frame_type);

    /* 将 payload 中的所有字段合并到 json 中 */
    if (payload && cJSON_IsObject(payload)) {
        cJSON *child = payload->child;
        while (child) {
            cJSON *dup = cJSON_Duplicate(child, 1);
            if (dup) {
                cJSON_AddItemToObject(json, child->string, dup);
            }
            child = child->next;
        }
    }

    return json;
}

/* ══════════════════════════════════════════════════════════
 * 帧解析
 * ══════════════════════════════════════════════════════════ */

esp_err_t dsh_frame_parse_type(const char *json_text, const char **type)
{
    if (!json_text || !type) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *json = cJSON_Parse(json_text);
    if (!json) {
        ESP_LOGW("dsh_frames", "JSON parse failed");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsString(type_item) || type_item->valuestring == NULL) {
        ESP_LOGW("dsh_frames", "Missing 'type' field");
        cJSON_Delete(json);
        return ESP_ERR_NOT_FOUND;
    }

    /* 注意：返回的 type 指针指向 json 内部，调用者不要 free json
     * 直到不再需要 type 字符串 */
    *type = type_item->valuestring;
    /* 不 delete json — 调用者负责管理生命周期 */
    return ESP_OK;
}

esp_err_t dsh_frame_serialize(cJSON *json, char **out_text, int *out_len)
{
    if (!json || !out_text) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_text = cJSON_PrintUnformatted(json);
    if (!*out_text) {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (int)strlen(*out_text);
    }

    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════
 * 帧类型判断
 * ══════════════════════════════════════════════════════════ */

/* 下行帧类型列表 */
static const char *s_downstream_types[] = {
    DSH_FRAME_SESSION_STATE,
    DSH_FRAME_THINKING,
    DSH_FRAME_ASSISTANT_TEXT,
    DSH_FRAME_ASSISTANT_DONE,
    DSH_FRAME_TOOL_CALL,
    DSH_FRAME_TOOL_RESULT,        /* 服务端中继 */
    DSH_FRAME_TOOL_PROGRESS,
    DSH_FRAME_FILE_CHANGE,
    DSH_FRAME_AUTH_REQUESTED,
    DSH_FRAME_AUTH_RESOLVED,
    DSH_FRAME_QUESTION_REQUESTED,
    DSH_FRAME_QUESTION_RESOLVED,
    DSH_FRAME_FILE_UPLOAD,
    DSH_FRAME_SYSTEM_EVENT,
    NULL,
};

/* 上行帧类型列表 */
static const char *s_upstream_types[] = {
    DSH_FRAME_CLIENT_HELLO,
    DSH_FRAME_CLIENT_HEARTBEAT,
    DSH_FRAME_CLIENT_TOOL_RESULT,
    DSH_FRAME_CLIENT_USER_INPUT,
    NULL,
};

static int match_type(const char *type, const char **list)
{
    if (!type) return 0;
    for (int i = 0; list[i]; i++) {
        if (strcmp(type, list[i]) == 0) return 1;
    }
    return 0;
}

int dsh_frame_is_downstream(const char *type)
{
    return match_type(type, s_downstream_types);
}

int dsh_frame_is_upstream(const char *type)
{
    return match_type(type, s_upstream_types);
}
