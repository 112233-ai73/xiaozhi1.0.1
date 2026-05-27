#include "bsp_ws.h"
#define WS_TOKEN "test-token"
// ws客户端
static esp_websocket_client_handle_t client;

// 定义一个事件标志组变量
EventGroupHandle_t ws_event_group;

// 定义事件标志组的一个位 用来表示ws是否连接成功
static const EventBits_t WEBSOCKET_CONNECTED_BIT = (1 << 0);

int WEBSOCKET_SESSION_ID_BIT = (1 << 1);

char session_id[9];

// 定义两个回调函数
text_callback_t text_callback;
bin_callback_t bin_callback;

static void
websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    // ws启动
    case WEBSOCKET_EVENT_BEGIN:
        MY_LOGI("ws启动");
        break;
    // ws连接成功
    case WEBSOCKET_EVENT_CONNECTED:
        MY_LOGI("ws连接成功");
        // 连接成功,把事件标志组中的表示ws连接成功的位置1
        is_wsline = true;
        xEventGroupSetBits(ws_event_group, WEBSOCKET_CONNECTED_BIT);
        break;
    // ws断开连接
    case WEBSOCKET_EVENT_DISCONNECTED:
        is_wsline = false;
        MY_LOGI("ws断开连接");
        break;
    // ws收到数据
    case WEBSOCKET_EVENT_DATA:
        MY_LOGI("ws收到数据");
        MY_LOGI("Received opcode=%d", data->op_code);
        if (data->op_code == 0x2)
        {
            if (bin_callback != NULL)
            {
                bin_callback((char *)data->data_ptr, data->data_len);
            }
        }
        else if (data->op_code == 0x1)
        {
            if (text_callback != NULL)
            {
                text_callback((char *)data->data_ptr, data->data_len);
            }
        }
        break;
    // ws错误
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "ws错误");
        break;
    // ws完成
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "ws完成");
        // 两件事件:
    
        // 2. 把设备状态变成初始化IDLE
        is_wsline = false;
        com_status_change(IDLE);
        break;
    }
}

/**
 * @brief 初始化WS
 */
void bsp_ws_init(void)//(text_callback_t text_cb, bin_callback_t bin_cb)
{
    //text_callback = text_cb;
   // bin_callback = bin_cb;
    // 1. 创建ws配置对象
    // WS的配置项
    esp_websocket_client_config_t websocket_cfg = {
        .uri = "wss://api.tenclass.net/xiaozhi/v1/",
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    // 根据配置得到WS客户端
    client = esp_websocket_client_init(&websocket_cfg);

    // 注册事件函数
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    // 添加header
    esp_websocket_client_append_header(client, "Authorization", "Bearer " WS_TOKEN);
    esp_websocket_client_append_header(client, "Protocol-Version", "1");
    esp_websocket_client_append_header(client, "Device-Id", bsp_get_mac());
    esp_websocket_client_append_header(client, "Client-Id", bsp_get_uuid());

    // 4.  事件标志组
    ws_event_group = xEventGroupCreate();
}

void bsp_ws_set_callbacks(text_callback_t text_cb, bin_callback_t bin_cb)
{
    text_callback = text_cb;
    bin_callback = bin_cb;
}

bool bsp_ws_is_connected(void)
{
    return client != NULL && esp_websocket_client_is_connected(client);
}

/**
 * @brief WS开启
 */
void bsp_ws_start(void)
{
    if (client != NULL && !esp_websocket_client_is_connected(client))
    {
        if (ws_event_group != NULL)
        {
            xEventGroupClearBits(ws_event_group, WEBSOCKET_CONNECTED_BIT | WEBSOCKET_SESSION_ID_BIT);
        }
        memset(session_id, 0, sizeof(session_id));
        esp_websocket_client_start(client);
        // 并且要等待事件标志组置1
        xEventGroupWaitBits(ws_event_group, WEBSOCKET_CONNECTED_BIT, true, true, portMAX_DELAY);

        ESP_LOGI(TAG, "ws连接成功");
    }
}

/**
 * @brief WS关闭
 */
void bsp_ws_stop(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        esp_websocket_client_close(client, portMAX_DELAY);
        is_wsline = false;
    }
}

/**
 * @brief 发送hello消息
 */
void bsp_ws_send_hello(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        // 1. 整理要发送给小智后台的json字符串
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "hello");
        cJSON_AddNumberToObject(root, "version", 1);
        cJSON_AddStringToObject(root, "transport", "websocket");

        cJSON *audio_params = cJSON_CreateObject();
        cJSON_AddStringToObject(audio_params, "format", "opus");
        cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
        cJSON_AddNumberToObject(audio_params, "channels", 1);
        cJSON_AddNumberToObject(audio_params, "frame_duration", 60);
        cJSON_AddItemToObject(root, "audio_params", audio_params);

        char *json_str = cJSON_PrintUnformatted(root);

        // 2. 发送json字符串给小智后台
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 3. 释放内存
        cJSON_Delete(root);
        free(json_str);

        MY_LOGE("WS:session_id1: %s", session_id);

        // 4. 等待拿到session_id 要等 事件标志组中的WEBSOCKET_SESSION_ID_BIT位 置1
        xEventGroupWaitBits(ws_event_group, WEBSOCKET_SESSION_ID_BIT, true, true, portMAX_DELAY);

        MY_LOGE("WS:session_id2: %s", session_id);
    }
}

/**
 * @brief 发送唤醒词
 * 我们口头唤醒的时候,只是唤醒了sr组件,但是我们在说唤醒词的时候,sr组件并没有把我们的音频发送出去
 * 我们需要再口头唤醒的时候,使用ws发送一个唤醒词的请求给服务器 同步唤醒
 */
void bsp_ws_send_wakeup_word(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {

        // 1. 整理要发送给小智后台的json字符串
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "session_id", session_id);
        cJSON_AddStringToObject(root, "type", "listen");
        cJSON_AddStringToObject(root, "state", "detect");
        cJSON_AddStringToObject(root, "text", "你好小智");

        char *json_str = cJSON_PrintUnformatted(root);

        // 2. 发送唤醒词请求
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 3. 释放内存
        cJSON_Delete(root);
        free(json_str);
    }
}

/**
 * @brief 发送开始监听请求
 */
void bsp_ws_send_start_listen(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        /**
         * 使用cJSON 参考下边json数据生成json字符串
         * {
                "session_id": "<会话ID>",
                "type": "listen",
                "state": "start",
                "mode": "<监听模式>"
            }
         */
        cJSON *root = cJSON_CreateObject();

        cJSON_AddStringToObject(root, "session_id", session_id);
        cJSON_AddStringToObject(root, "type", "listen");
        cJSON_AddStringToObject(root, "state", "start");
        cJSON_AddStringToObject(root, "mode", "auto");

        char *json_str = cJSON_PrintUnformatted(root);

        // 发送文本数据
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 释放
        cJSON_Delete(root);
        free(json_str);
    }
}

/**
 * @brief 发送结束监听请求
 */
void bsp_ws_send_stop_listen(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        /**
         * 使用cJSON 参考下边json数据生成json字符串
         * {
                "session_id": "<会话ID>",
                "type": "listen",
                "state": "stop"
            }
         */
        cJSON *root = cJSON_CreateObject();

        cJSON_AddStringToObject(root, "session_id", session_id);
        cJSON_AddStringToObject(root, "type", "listen");
        cJSON_AddStringToObject(root, "state", "stop");

        char *json_str = cJSON_PrintUnformatted(root);

        // 发送文本数据
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 释放
        cJSON_Delete(root);
        free(json_str);
    }
}

/**
 * @brief 发送中断请求
 */
void bsp_ws_send_abort(void)
{

    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        /**
         * 使用cJSON 参考下边json数据生成json字符串
         * {
                "session_id": "<会话ID>",
                "type": "abort",
                "reason": "wake_word_detected" // 可选
            }
         */
        cJSON *root = cJSON_CreateObject();

        cJSON_AddStringToObject(root, "session_id", session_id);
        cJSON_AddStringToObject(root, "type", "abort");
        cJSON_AddStringToObject(root, "reason", "wake_word_detected");

        char *json_str = cJSON_PrintUnformatted(root);

        // 发送文本数据
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 释放
        cJSON_Delete(root);
        free(json_str);
    }
}

/**
 * @brief 发送opus数据
 */
void bsp_ws_send_opus(char *data, int len)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        esp_websocket_client_send_bin(client, data, len, portMAX_DELAY);
    }
}

/**
 * @brief 发送IOT控制学习
 */
void bsp_ws_send_iot_learn(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        // 准备数据
        char *data1 = "{\"descriptors\":[{\"description\":\"Speaker\",\"methods\":{\"SetMute\":{\"description\":\"Set mute status\",\"parameters\":{\"mute\":{\"description\":\"Mute status\",\"type\":\"boolean\"}}},\"SetVolume\":{\"description\":\"Set volume level\",\"parameters\":{\"volume\":{\"description\":\"Volume level[0-100]\",\"type\":\"number\"}}}},\"name\":\"Speaker\",\"properties\":{\"mute\":{\"description\":\"Mute status\",\"type\":\"boolean\"},\"volume\":{\"description\":\"Volume level[0-100]\",\"type\":\"number\"}}}],\"session_id\":\"";
        char *data2 = "\",\"type\":\"iot\",\"update\":true}";

        // 使用heap_caps_malloc申请空间
        char *json_str = heap_caps_malloc(strlen(data1) + strlen(data2) + 8, MALLOC_CAP_SPIRAM);
        // 拼接字符串和session_id
        memcpy(json_str, data1, strlen(data1));
        memcpy(json_str + strlen(data1), session_id, 8);
        memcpy(json_str + strlen(data1) + 8, data2, strlen(data2));

        // 发送数据
        esp_websocket_client_send_text(client, json_str, strlen(data2) + strlen(data1) + 8, portMAX_DELAY);
        // esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);

        // 释放
        free(json_str);
    }
}

/**
 * @brief 发送IOT状态
 */
void bsp_ws_send_iot_status(void)
{
    if (client != NULL && esp_websocket_client_is_connected(client))
    {
        // 准备数据
        char *data1 = "{\"session_id\": \"";
        char *data2 = "\",\"states\": [{\"name\": \"Speaker\",\"state\": {\"mute\": false,\"volume\": 60}}],\"type\": \"iot\",\"update\": true}";

        // 使用heap_caps_malloc申请空间
        char *json_str = heap_caps_malloc(strlen(data1) + strlen(data2) + 8, MALLOC_CAP_SPIRAM);
        // 拼接字符串和session_id
        memcpy(json_str, data1, strlen(data1));
        memcpy(json_str + strlen(data1), session_id, 8);
        memcpy(json_str + strlen(data1) + 8, data2, strlen(data2));

        // 发送数据
        esp_websocket_client_send_text(client, json_str, strlen(data2) + strlen(data1) + 8, portMAX_DELAY);

        // 释放
        free(json_str);
    }
}
