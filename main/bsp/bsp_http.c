#include "bsp_http.h"

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    mutable_buffer_t *buffer = (mutable_buffer_t *)evt->user_data;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        MY_LOGD("HTTP request error");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        MY_LOGD("HTTP connected");
        break;
    case HTTP_EVENT_HEADER_SENT:
        MY_LOGD("HTTP header sent");
        break;
    case HTTP_EVENT_ON_HEADER:
        MY_LOGD("HTTP header received");
        break;
    case HTTP_EVENT_ON_DATA:
        MY_LOGD("HTTP received data, len=%d", evt->data_len);
        bsp_mutable_buffer_add_data(buffer, evt->data, evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
    {
        MY_LOGD("HTTP data received");
        char *data = (char *)bsp_mutable_buffer_get_data(buffer);
        MY_LOGE("HTTP received data:\r\n%s", data != NULL ? data : "");
        break;
    }
    case HTTP_EVENT_DISCONNECTED:
        MY_LOGI("HTTP disconnected");
        break;
    case HTTP_EVENT_REDIRECT:
        MY_LOGD("HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static char *protocol_http_get_body(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *application_json = cJSON_CreateObject();
    cJSON *board = cJSON_CreateObject();
    if (root == NULL || application_json == NULL || board == NULL)
    {
        cJSON_Delete(root);
        cJSON_Delete(application_json);
        cJSON_Delete(board);
        return NULL;
    }

    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON_AddStringToObject(application_json, "version", app_desc->version);
    cJSON_AddStringToObject(application_json, "elf_sha256", (char *)esp_app_get_elf_sha256_str());

    cJSON_AddStringToObject(board, "type", "esp32s3");
    cJSON_AddStringToObject(board, "name", "esp32s3");
    cJSON_AddStringToObject(board, "ssid", "nowLetsgo");

    cJSON_AddItemToObject(root, "application", application_json);
    cJSON_AddItemToObject(root, "board", board);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return body;
}

void http_request_active_code(void)
{
    mutable_buffer_t *buffer = bsp_mutable_buffer_create();
    if (buffer == NULL)
    {
        MY_LOGE("create HTTP response buffer failed");
        return;
    }

    esp_http_client_config_t config = {
        .url = "https://api.tenclass.net/xiaozhi/ota/",
        .event_handler = _http_event_handler,
        .user_data = buffer,
        .disable_auto_redirect = true,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        MY_LOGE("init HTTP client failed");
        bsp_mutable_buffer_free(buffer);
        return;
    }

    esp_http_client_set_header(client, "Device-Id", bsp_get_mac());
    esp_http_client_set_header(client, "Client-Id", bsp_get_uuid());
    esp_http_client_set_header(client, "User-Agent", "esp32/1.0.0");
    esp_http_client_set_header(client, "Accept-Language", "zh-CN");

    char *body = protocol_http_get_body();
    if (body == NULL)
    {
        MY_LOGE("create HTTP request body failed");
        esp_http_client_cleanup(client);
        bsp_mutable_buffer_free(buffer);
        return;
    }
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK)
    {
        MY_LOGE("HTTP request failed: %s", esp_err_to_name(ret));
    }

    free(body);
    esp_http_client_cleanup(client);
    bsp_mutable_buffer_free(buffer);
}
