#include "ota.h"
#include "audio_sr.h"

static char ota_write_data[BUFFSIZE + 1] = {0};

static bool system_diagnostic(void)
{
    MY_LOGI("开始执行系统健康自检 (5秒)...");

    vTaskDelay(pdMS_TO_TICKS(5000));

    bool diagnostic_is_ok = true;
    return diagnostic_is_ok;
}

static void get_local_version(char *local_version_str, size_t max_len)
{
    esp_app_desc_t running_app_info;
    const esp_partition_t *running_partition = esp_ota_get_running_partition();

    if (esp_ota_get_partition_description(running_partition, &running_app_info) == ESP_OK)
    {
        strncpy(local_version_str, running_app_info.version, max_len);
        ESP_LOGI(TAG, "当前小智运行版本: %s", local_version_str);
    }
    else
    {
        strncpy(local_version_str, "0.0.0", max_len);
        ESP_LOGW(TAG, "无法获取当前版本，默认使用 0.0.0");
    }
}

void check_ota_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {

            MY_LOGI("检测到系统处于 OTA 待验证状态，开始自检...");
            if (system_diagnostic())
            {
                MY_LOGI("自检通过！确认新固件有效，取消回滚。");
                esp_ota_mark_app_valid_cancel_rollback();
            }
            else
            {
                MY_LOGE("自检失败！系统将回滚到上一个老版本固件并重启...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
}

static void http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void ota_fail_and_delete_task(esp_http_client_handle_t client, esp_ota_handle_t update_handle)
{
    if (client != NULL)
    {
        http_cleanup(client);
    }

    if (update_handle != 0)
    {
        esp_ota_abort(update_handle);
    }

    app_sr_resume_tasks();
    vTaskDelete(NULL);
}

static void log_ota_network_status(void)
{
    wifi_ap_record_t ap_info = {0};
    esp_err_t wifi_ret = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_ret == ESP_OK)
    {
        MY_LOGI("OTA WiFi connected: SSID=%s, RSSI=%d, channel=%u",
                ap_info.ssid, ap_info.rssi, ap_info.primary);
    }
    else
    {
        MY_LOGW("OTA WiFi is not connected: %s", esp_err_to_name(wifi_ret));
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL)
    {
        MY_LOGW("OTA STA netif not found");
        return;
    }

    esp_netif_ip_info_t ip_info = {0};
    esp_err_t ip_ret = esp_netif_get_ip_info(sta_netif, &ip_info);
    if (ip_ret != ESP_OK)
    {
        MY_LOGW("OTA failed to get STA IP info: %s", esp_err_to_name(ip_ret));
        return;
    }

    MY_LOGI("OTA STA IP: " IPSTR ", gateway: " IPSTR ", netmask: " IPSTR,
            IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));
}

static int is_new_version_available(const char *local_ver, const char *server_ver)
{
    int v1[3] = {0}, v2[3] = {0};
    sscanf(local_ver, "%d.%d.%d", &v1[0], &v1[1], &v1[2]);
    sscanf(server_ver, "%d.%d.%d", &v2[0], &v2[1], &v2[2]);

    for (int i = 0; i < 3; i++)
    {
        if (v2[i] > v1[i])
            return 1;
        if (v2[i] < v1[i])
            return 0;
    }
    return 0;
}

static void xiaozhi_ota_task(void *pvParameter)
{
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    MY_LOGI("启动小智 OTA 升级任务...");

    log_ota_network_status();

    MY_LOGI("获取当前版本号");
    char local_version[32] = {0};
    get_local_version(local_version, sizeof(local_version));

    MY_LOGI("获取服务器最新版本号");
    esp_http_client_config_t config_ver = {
        .url = OTA_VERSION_URL,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client_ver = esp_http_client_init(&config_ver);
    if (client_ver == NULL)
    {
        ESP_LOGE(TAG, "HTTP client init failed");
        ota_fail_and_delete_task(NULL, 0);
        return;
    }

    MY_LOGI("创建数组用于存放下载的 JSON 字符串");
    char version_buffer[OTA_JSON_BUFFER_SIZE] = {0};
    err = esp_http_client_open(client_ver, 0);
    if (err == ESP_OK)
    {
        esp_http_client_fetch_headers(client_ver);
        int total_read = 0;
        while (total_read < (int)sizeof(version_buffer) - 1)
        {
            int data_read = esp_http_client_read(client_ver,
                                                version_buffer + total_read,
                                                sizeof(version_buffer) - 1 - total_read);
            if (data_read < 0)
            {
                MY_LOGE("read OTA JSON failed");
                break;
            }
            if (data_read == 0)
            {
                if (esp_http_client_is_complete_data_received(client_ver))
                {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            total_read += data_read;
        }
    }
    else
    {
        MY_LOGE("open OTA JSON connection failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client_ver);

    if (strlen(version_buffer) == 0)
    {
        ESP_LOGE(TAG, "获取云端版本信息失败");
        ota_fail_and_delete_task(NULL, 0);
        return;
    }

    MY_LOGI("解析 JSON 字符串");
    cJSON *json = cJSON_Parse(version_buffer);
    if (json == NULL)
    {
        MY_LOGE("JSON 解析失败");
        ota_fail_and_delete_task(NULL, 0);
        return;
    }

    cJSON *server_ver_obj = cJSON_GetObjectItem(json, "version");
    if (!server_ver_obj || !cJSON_IsString(server_ver_obj))
    {
        MY_LOGE("无法从 JSON 获取版本号");
        cJSON_Delete(json);
        ota_fail_and_delete_task(NULL, 0);
        return;
    }

    cJSON *ota_url_obj = cJSON_GetObjectItem(json, "url");
    if (!ota_url_obj || !cJSON_IsString(ota_url_obj))
    {
        MY_LOGE("cannot get OTA URL from JSON");
        cJSON_Delete(json);
        ota_fail_and_delete_task(NULL, 0);
        return;
    }

    char server_version[32] = {0};
    char ota_url[OTA_URL_MAX_LEN] = {0};
    strncpy(server_version, server_ver_obj->valuestring, sizeof(server_version) - 1);
    strncpy(ota_url, ota_url_obj->valuestring, sizeof(ota_url) - 1);
    cJSON_Delete(json);
    MY_LOGI("OTA URL: %s", ota_url);

    MY_LOGI("云端最新版本: %s", server_version);

    MY_LOGI("比较版本号");
    if (!is_new_version_available(local_version, server_version))
    {
        MY_LOGI("当前已是最新版本 (%s)，无需升级", local_version);
        app_sr_resume_tasks();
        vTaskDelete(NULL);
        return;
    }

    MY_LOGI("开始下载新固件");
    esp_http_client_config_t config = {
        .url = ota_url,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
        // .cert_pem = (char *)server_cert_pem_start, // HTTPS 时取消注释并配置证书
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        ota_fail_and_delete_task(NULL, update_handle);
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        MY_LOGE("无法打开 HTTP 连接: %s", esp_err_to_name(err));
        ota_fail_and_delete_task(client, update_handle);
    }
    esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    MY_LOGI("准备将新固件写入分区: subtype %d at offset 0x%lx",
            update_partition->subtype, update_partition->address);

    int binary_file_length = 0;
    bool image_header_was_checked = false;

    while (1)
    {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0)
        {
            MY_LOGE("读取数据失败");
            ota_fail_and_delete_task(client, update_handle);
        }
        else if (data_read > 0)
        {
            if (image_header_was_checked == false)
            {
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK)
                {
                    MY_LOGE("esp_ota_begin 失败 (%s)", esp_err_to_name(err));
                    ota_fail_and_delete_task(client, update_handle);
                }
                MY_LOGI("OTA 写入初始化成功，开始持续写入...");
                image_header_was_checked = true;
            }

            err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK)
            {
                MY_LOGE("写入 Flash 失败 (%s)", esp_err_to_name(err));
                ota_fail_and_delete_task(client, update_handle);
            }
            binary_file_length += data_read;
            MY_LOGD("已写入: %d bytes", binary_file_length);
        }
        else if (data_read == 0)
        {
            if (esp_http_client_is_complete_data_received(client) == true)
            {
                MY_LOGI("固件下载完成！");
                break;
            }
        }
    }
    MY_LOGI("总共写入数据大小: %d bytes", binary_file_length);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        MY_LOGE("OTA 结束验证失败 (%s)!", esp_err_to_name(err));
        ota_fail_and_delete_task(client, 0);
    }

    // 设置新固件为下次启动项
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        MY_LOGE("设置启动分区失败 (%s)!", esp_err_to_name(err));
        ota_fail_and_delete_task(client, 0);
    }

    MY_LOGI("OTA 升级成功！准备重启系统...");
    http_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void start_xiaozhi_ota(void)
{
    MY_LOGI("收到升级指令，正在创建 OTA 任务...");
    BaseType_t ret = xTaskCreate(&xiaozhi_ota_task, "xiaozhi_ota_task", 8192, NULL, 5, NULL);
    if (ret != pdPASS)
    {
        MY_LOGE("OTA task create failed");
        app_sr_resume_tasks();
    }
}
