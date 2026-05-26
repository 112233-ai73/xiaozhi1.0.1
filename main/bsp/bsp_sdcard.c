#include "bsp_sdcard.h"
#include "com/com_debug.h"

//static const char *TAG = "SD_SPI";

#define MOUNT_POINT "/sdcard"

#define PIN_NUM_MISO  GPIO_NUM_15  
#define PIN_NUM_MOSI  GPIO_NUM_6  
#define PIN_NUM_CLK   GPIO_NUM_7  
#define PIN_NUM_CS    GPIO_NUM_5  

char sd_file_names[MAX_FILES_TO_LIST][MAX_FILENAME_LEN];
int sd_file_count = 0;

// --- 写入文本文件 ---
esp_err_t sd_write_text_file(const char *path, const char *data)
{
    ESP_LOGI(TAG, "正在打开文件准备写入: %s", path);
    FILE *f = fopen(path, "w"); // "w" 模式会覆盖原文件
    if (f == NULL) {
        ESP_LOGE(TAG, "打开文件写入失败!");
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI(TAG, "文件写入成功");
    return ESP_OK;
}

// --- 读取文本文件 ---
esp_err_t sd_read_text_file(const char *path)
{
    ESP_LOGI(TAG, "正在读取文件: %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "打开文件读取失败!");
        return ESP_FAIL;
    }
    
    char line[128]; // 设定读取缓冲区大小
    fgets(line, sizeof(line), f);
    fclose(f);

    // 去除末尾换行符便于打印
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "文件内容为: '%s'", line);
    return ESP_OK;
}

// --- 初始化 SD 卡 (SPI 模式) ---
void init_sd_card_spi(void)
{
    esp_err_t ret;
    sdmmc_card_t *card;

    ESP_LOGI(TAG, "开始初始化 SD 卡 (SPI 模式)...");

    // 1. 配置文件系统挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 建议设为 false，防止接线错误时意外格式化你的卡
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // 2. 配置 SPI 主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // 3. 配置 SPI 总线引脚
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1, // 不使用
        .quadhd_io_num = -1, // 不使用
        .max_transfer_sz = 4000,
    };

    // 初始化 SPI 总线
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败!");
        return;
    }

    // 4. 配置 SD SPI 设备 (片选引脚)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // 5. 挂载文件系统
    ESP_LOGI(TAG, "正在挂载文件系统...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "文件系统挂载失败。SD卡可能未格式化为 FAT32。");
        } else {
            ESP_LOGE(TAG, "SD 卡初始化失败 (%s)。请检查连线、引脚号和供电。", esp_err_to_name(ret));
        }
        return;
    }
    
    ESP_LOGI(TAG, "SD 卡挂载成功!");
    
    // 打印 SD 卡信息
    sdmmc_card_print_info(stdout, card);
}

esp_err_t sd_list_files(const char *dir_path)
{
    MY_LOGI("开始读取目录并筛选 MP3 文件: %s", dir_path);
    
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        MY_LOGE("无法打开目录，请检查路径: %s", dir_path);
        return ESP_FAIL;
    }

    struct dirent *entry;
    sd_file_count = 0; 

    while ((entry = readdir(dir)) != NULL) {
        
        // 1. 忽略目录
        if (entry->d_type == DT_DIR) {
            continue;
        }

        // 2. 检查后缀是否为 .mp3
        char *ext = strrchr(entry->d_name, '.');
        if (ext != NULL && strcasecmp(ext, ".mp3") == 0) {
            
            // 3. 检查数组是否已满
            if (sd_file_count < MAX_FILES_TO_LIST) {
                
                // 4. 使用 snprintf 拼接路径和文件名
                // 如果传入的 dir_path 是 "/sdcard"，格式化后就是 "/sdcard/文件名.mp3"
                // snprintf 会自动在末尾补 '\0'，非常安全
                snprintf(sd_file_names[sd_file_count], MAX_FILENAME_LEN, "%s/%s", dir_path, entry->d_name);
                
                MY_LOGI("提取到完整路径: %s", sd_file_names[sd_file_count]);
                sd_file_count++;
            } else {
                MY_LOGW("MP3 文件数量已达到设定上限 (%d)，停止记录", MAX_FILES_TO_LIST);
                break; 
            }
        }
    }

    closedir(dir);
    MY_LOGI("目录读取完成，共保存了 %d 个带路径的 MP3 文件名。", sd_file_count);
    return ESP_OK;
}
