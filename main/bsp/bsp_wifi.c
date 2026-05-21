#include "bsp_wifi.h"
#include "com/com_debug.h"
#include <stdbool.h>

#define ESP_MAXIMUM_RETRY 5
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;
static bool s_uart_provisioning = false;

static esp_err_t wifi_set_storage(wifi_storage_t storage)
{
    esp_err_t ret = esp_wifi_set_storage(storage);
    if (ret != ESP_OK)
    {
        MY_LOGE("esp_wifi_set_storage failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static void wifi_clear_connection_bits(void)
{
    if (s_wifi_event_group != NULL)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }
}

static void wifi_retry_or_fail(void)
{
    if (s_uart_provisioning)
    {
        MY_LOGI("UART provisioning is updating WiFi config, skip reconnect");
        return;
    }

    if (s_retry_num < ESP_MAXIMUM_RETRY)
    {
        esp_wifi_connect();
        s_retry_num++;
        MY_LOGI("retry to connect to the AP");
    }
    else
    {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }

    MY_LOGI("connect to the AP fail");
}

static void wifi_handle_got_ip(void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    MY_LOGI("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        MY_LOGI("WiFi STA Started. Waiting for UART credentials...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_retry_or_fail();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_handle_got_ip(event_data);
    }
}

static void wifi_register_event_handlers(void)
{
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
}

static void wifi_init_driver(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_register_event_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(wifi_set_storage(WIFI_STORAGE_FLASH));
}

static esp_err_t wifi_apply_flash_config(wifi_config_t *wifi_config)
{
    esp_err_t ret = wifi_set_storage(WIFI_STORAGE_FLASH);

    if (ret == ESP_OK)
    {
        ret = esp_wifi_set_config(WIFI_IF_STA, wifi_config);
    }

    if (ret != ESP_OK)
    {
        MY_LOGE("esp_wifi_set_config failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static void wifi_auto_connect_saved_config(wifi_config_t *saved_config)
{
    if (saved_config->sta.ssid[0] == '\0')
    {
        MY_LOGI("NVS 中无保存的 WiFi 信息,跳过自动连接，等待串口配网...");
        return;
    }

    MY_LOGI("NVS 中检测到历史 WiFi,正在尝试自动连接 SSID: %s", saved_config->sta.ssid);
    s_retry_num = 0;
    if (wifi_apply_flash_config(saved_config) == ESP_OK)
    {
        esp_wifi_connect();
        MY_LOGI("历史 WiFi 已开始后台自动重连，系统继续启动");
    }
}

static esp_err_t wifi_disconnect_if_needed(void)
{
    esp_err_t ret = esp_wifi_disconnect();

    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_CONNECT)
    {
        MY_LOGW("esp_wifi_disconnect failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

static esp_err_t wifi_stop_if_needed(void)
{
    esp_err_t ret = esp_wifi_stop();

    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED)
    {
        MY_LOGW("esp_wifi_stop failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static esp_err_t wifi_start_and_connect(void)
{
    esp_err_t ret = esp_wifi_start();

    if (ret == ESP_OK)
    {
        ret = esp_wifi_connect();
    }

    if (ret != ESP_OK)
    {
        MY_LOGE("WiFi start/connect failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static void wifi_init_sta(void)
{
    wifi_config_t saved_config = {0};

    s_wifi_event_group = xEventGroupCreate();
    wifi_init_driver();
    esp_wifi_get_config(WIFI_IF_STA, &saved_config);
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_auto_connect_saved_config(&saved_config);
}

void bsp_wifi_start_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    MY_LOGI("收到串口配网请求，准备连接 SSID: %s, PWD: %s", ssid, password);

    s_retry_num = 0;
    s_uart_provisioning = true;
    wifi_clear_connection_bits();

    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    if (wifi_disconnect_if_needed() == ESP_OK &&
        wifi_stop_if_needed() == ESP_OK &&
        wifi_apply_flash_config(&wifi_config) == ESP_OK)
    {
        wifi_start_and_connect();
    }

    s_uart_provisioning = false;
}

void bsp_wifi_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL)
    {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    MY_LOGI("ESP_WIFI_MODE_STA");
    wifi_init_sta();
}
