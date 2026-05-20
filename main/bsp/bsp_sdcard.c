#include "bsp_sdcard.h"
#include "com/com_debug.h"




static const char *TAG = "SD_SPI";

#define MOUNT_POINT "/sdcard"

// ---------------------------------------------------------
// 1. 在这里写死你的 SPI 引脚 (请根据 ESP32-S3 实际接线修改!)
// ---------------------------------------------------------
#define PIN_NUM_MISO  13  // 主设备输入，从设备输出 (SD 卡的 DO)
#define PIN_NUM_MOSI  11  // 主设备输出，从设备输入 (SD 卡的 DI)
#define PIN_NUM_CLK   12  // 时钟线 (SD 卡的 SCK)
#define PIN_NUM_CS    10  // 片选线 (SD 卡的 CS)
// ---------------------------------------------------------

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

    // --- 测试读写功能 ---
    const char *test_file = MOUNT_POINT "/test.txt";
    sd_write_text_file(test_file, "Hello ESP32-S3 SPI SD Card!");
    sd_read_text_file(test_file);
}
