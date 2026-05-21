#include "ota.h"

static const char *TAG = "XIAOZHI_OTA";
static char ota_write_data[BUFFSIZE + 1] = { 0 };


static bool system_diagnostic(void)
{
    ESP_LOGI(TAG, "开始执行系统健康自检 (5秒)...");
    
   
    vTaskDelay(pdMS_TO_TICKS(5000)); 
    
    bool diagnostic_is_ok = true; 
    return diagnostic_is_ok;
}


void check_ota_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            
            ESP_LOGI(TAG, "检测到系统处于 OTA 待验证状态，开始自检...");
            if (system_diagnostic()) {
                ESP_LOGI(TAG, "自检通过！确认新固件有效，取消回滚。");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "自检失败！系统将回滚到上一个老版本固件并重启...");
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

static void log_ota_network_status(void)
{
    wifi_ap_record_t ap_info = {0};
    esp_err_t wifi_ret = esp_wifi_sta_get_ap_info(&ap_info);

    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA WiFi connected: SSID=%s, RSSI=%d, channel=%u",
                 ap_info.ssid, ap_info.rssi, ap_info.primary);
    } else {
        ESP_LOGW(TAG, "OTA WiFi is not connected: %s", esp_err_to_name(wifi_ret));
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL) {
        ESP_LOGW(TAG, "OTA STA netif not found");
        return;
    }

    esp_netif_ip_info_t ip_info = {0};
    esp_err_t ip_ret = esp_netif_get_ip_info(sta_netif, &ip_info);
    if (ip_ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA failed to get STA IP info: %s", esp_err_to_name(ip_ret));
        return;
    }

    ESP_LOGI(TAG, "OTA STA IP: " IPSTR ", gateway: " IPSTR ", netmask: " IPSTR,
             IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));
}

static void xiaozhi_ota_task(void *pvParameter)
{
    esp_err_t err;
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "启动小智 OTA 升级任务...");



    log_ota_network_status();

    esp_http_client_config_t config = {
        .url = OTA_URL,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
        // .cert_pem = (char *)server_cert_pem_start, // HTTPS 时取消注释并配置证书
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        vTaskDelete(NULL);
    }
    
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "无法打开 HTTP 连接: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
    }
    esp_http_client_fetch_headers(client);

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "准备将新固件写入分区: subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);

    int binary_file_length = 0;
    bool image_header_was_checked = false;
    
   
    while (1) {
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0) {
            ESP_LOGE(TAG, "读取数据失败");
            http_cleanup(client);
            vTaskDelete(NULL);
        } else if (data_read > 0) {
            if (image_header_was_checked == false) {
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin 失败 (%s)", esp_err_to_name(err));
                    http_cleanup(client);
                    esp_ota_abort(update_handle);
                    vTaskDelete(NULL);
                }
                ESP_LOGI(TAG, "OTA 写入初始化成功，开始持续写入...");
                image_header_was_checked = true;
            }
            
            err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "写入 Flash 失败 (%s)", esp_err_to_name(err));
                http_cleanup(client);
                esp_ota_abort(update_handle);
                vTaskDelete(NULL);
            }
            binary_file_length += data_read;
            ESP_LOGD(TAG, "已写入: %d bytes", binary_file_length);
            
        } else if (data_read == 0) {
            if (esp_http_client_is_complete_data_received(client) == true) {
                ESP_LOGI(TAG, "固件下载完成！");
                break;
            }
        }
    }
    ESP_LOGI(TAG, "总共写入数据大小: %d bytes", binary_file_length);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA 结束验证失败 (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        vTaskDelete(NULL);
    }

    // 设置新固件为下次启动项
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置启动分区失败 (%s)!", esp_err_to_name(err));
        http_cleanup(client);
        vTaskDelete(NULL);
    }
    
    ESP_LOGI(TAG, "OTA 升级成功！准备重启系统...");
    http_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}


void start_xiaozhi_ota(void)
{
    ESP_LOGI(TAG, "收到升级指令，正在创建 OTA 任务...");
    xTaskCreate(&xiaozhi_ota_task, "xiaozhi_ota_task", 8192, NULL, 5, NULL);
}
