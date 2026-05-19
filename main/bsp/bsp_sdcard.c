#include "bsp_sdcard.h"


static const char *TAG = "SD_CARD";

// ---------------------------------------------------------
// 1. 引脚配置 
// ---------------------------------------------------------
#define SD_PIN_CLK  GPIO_NUM_33
#define SD_PIN_CMD  GPIO_NUM_32
#define SD_PIN_D0   GPIO_NUM_35
#define SD_PIN_D1   GPIO_NUM_34  
#define SD_PIN_D2   GPIO_NUM_30
#define SD_PIN_D3   GPIO_NUM_31

// ESP32-S3 的 VDD_SPI 通常对应内部 LDO_VO4 通道 (具体取决于你的 ESP-IDF 版本宏定义)
// 如果编译报错找不到该宏，可以直接填数字 4
#define SD_LDO_CHAN_ID 4 

// 全局的电源控制句柄
sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

// ---------------------------------------------------------
// 2. 文件系统写入函数 (专门用于 HTTP 下载 MP3 时的分块写入)
// ---------------------------------------------------------
esp_err_t sd_write_binary_chunk(const char *path, const uint8_t *data, size_t size)
{
    // 注意：使用 "ab" 模式 (Append Binary)。
    // 这样 HTTP 每次下载到 1KB 数据，就会自动接在文件末尾，不会覆盖之前的数据。
    FILE *f = fopen(path, "ab"); 
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件进行写入: %s", path);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "写入失败: 期望 %zu 字节, 实际写入 %zu 字节", size, written);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ---------------------------------------------------------
// 3. 文件系统读取函数 (用于测试或读取文本)
// ---------------------------------------------------------
esp_err_t sd_read_file_example(const char *path)
{
    ESP_LOGI(TAG, "正在读取文件: %s", path);
    // "rb" 模式 (Read Binary)
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件进行读取");
        return ESP_FAIL;
    }
    
    char buffer[128];
    // 仅演示读取前 127 个字节
    size_t read_bytes = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[read_bytes] = '\0'; // 添加字符串结束符便于打印
    
    fclose(f);
    ESP_LOGI(TAG, "文件内容读取成功:\n%s", buffer);
    
    return ESP_OK;
}

// ---------------------------------------------------------
// 4. SD 卡核心初始化函数 (含 VDD_SPI 供电控制)
// ---------------------------------------------------------
void init_sd_card(void)
{
    esp_err_t ret;
    sdmmc_card_t *card;

    ESP_LOGI(TAG, "开始初始化 SD 卡 (SDMMC 4线模式 + VDD_SPI供电)...");

    // 1. 初始化内部 LDO 电源控制 (VDD_SPI)
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = SD_LDO_CHAN_ID,
    };
    
    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建内部 LDO 电源控制失败!");
        return;
    }

    // 2. 配置 VFS 挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, 
        .max_files = 5,                  
        .allocation_unit_size = 16 * 1024
    };

    // 3. 配置 SDMMC 主机并绑定电源句柄
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = pwr_ctrl_handle; // 【关键】将供电权交给 SDMMC 主机驱动

    // 4. 配置引脚和模式
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4; 
    slot_config.clk = SD_PIN_CLK;
    slot_config.cmd = SD_PIN_CMD;
    slot_config.d0  = SD_PIN_D0;
    slot_config.d1  = SD_PIN_D1; 
    slot_config.d2  = SD_PIN_D2;
    slot_config.d3  = SD_PIN_D3;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // 5. 执行挂载
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD卡挂载失败 (%s)。", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SD 卡挂载成功！");
    sdmmc_card_print_info(stdout, card);
}