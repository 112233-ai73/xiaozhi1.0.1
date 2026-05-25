#include "bsp_http.h"

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{

    // 拿到evt中保存的缓冲区
    mutable_buffer_t *buffer = (mutable_buffer_t *)evt->user_data;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        MY_LOGD("HTTP请求失败");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        MY_LOGD("HTTP建立连接");
        break;
    case HTTP_EVENT_HEADER_SENT:
        MY_LOGD("HTTP头开始发送");
        break;
    case HTTP_EVENT_ON_HEADER:
        MY_LOGD("HTTP头开始接收");
        break;
    case HTTP_EVENT_ON_DATA:
        MY_LOGD("HTTP接收数据, len=%d", evt->data_len);
        bsp_mutable_buffer_add_data(buffer, evt->data, evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        MY_LOGD("HTTP数据接收完成");
        char *data = bsp_mutable_buffer_get_data(buffer);
        MY_LOGE("HTTP接收到的数据:\r\n%s", data);
        break;
    case HTTP_EVENT_DISCONNECTED:
        MY_LOGI("HTTP断开连接");

        break;
    case HTTP_EVENT_REDIRECT:
        MY_LOGD("HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

/**
 * @brief 创建请求体
 */
// 封装获取body的函数
static char *protocol_http_get_body(void)
{
    // 1. 创建cJSON根对象
    cJSON *root = cJSON_CreateObject();

    // 2. 创建一个application json对象
    cJSON *application_json = cJSON_CreateObject();
    // 给app_json添加属性version和elf_sha256
    const esp_app_desc_t *app_desc = esp_app_get_description();
    // 2.1 给application对象 添加version字段
    cJSON_AddStringToObject(application_json, "version", app_desc->version);
    // 2.2 给application对象 添加elf_sha256字段
    cJSON_AddStringToObject(application_json, "elf_sha256", (char *)esp_app_get_elf_sha256_str());

    // 3. 创建一个board json对象
    cJSON *board = cJSON_CreateObject();
    // 3.1 给board对象 添加type字段
    cJSON_AddStringToObject(board, "type", "esp32s3");
    // 3.2 给board对象 添加name字段
    cJSON_AddStringToObject(board, "name", "esp32s3");
    // 3.3 给board对象 添加ssid字段
    cJSON_AddStringToObject(board, "ssid", "nowLetsgo");

    // 4. 把application和board对象添加到根对象中
    cJSON_AddItemToObject(root, "application", application_json);
    cJSON_AddItemToObject(root, "board", board);

    // 5. 把根对象转换成字符串
    char *body = cJSON_PrintUnformatted(root);

    // 6. 释放
    cJSON_Delete(root);

    return body;
}

/**
 * @brief 发送http请求 请求激活码
 */
void http_request_active_code(void)
{

    // 创建一个可变缓冲区的结构体实例
    mutable_buffer_t *buffer = bsp_mutable_buffer_create();
    esp_http_client_config_t config = {
        .url = "https://api.tenclass.net/xiaozhi/ota/",
        .event_handler = _http_event_handler,
        .user_data = buffer,
        .disable_auto_redirect = true,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 添加请求头
    esp_http_client_set_header(client, "Device-Id", bsp_get_mac());
    esp_http_client_set_header(client, "Client-Id", bsp_get_uuid());
    esp_http_client_set_header(client, "User-Agent", "esp32/1.0.0");
    esp_http_client_set_header(client, "Accept-Language", "zh-CN");

    // 添加请求体
    char *body = protocol_http_get_body();
    esp_http_client_set_post_field(client, body, strlen(body));

    // 发送请求
    esp_http_client_perform(client);

    // 释放缓冲区
    bsp_mutable_buffer_free(buffer);
}