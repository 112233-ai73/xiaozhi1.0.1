#include "bsp_nvs.h"
/**
 * @brief 初始化
 */
// 创建命名空间 并获取该命名空间的操作句柄
nvs_handle_t my_handle;
void bsp_NVS_Init(void)
{
    // 初始化NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    nvs_open("storage", NVS_READWRITE, &my_handle);
}

/**
 * @brief 向flash写数据
 */
esp_err_t bsp_NVS_WriteStr(char *key, char *value)
{
    return nvs_set_str(my_handle, key, value);
}

/**
 * @brief 向flash读数据
 */
esp_err_t bsp_NVS_ReadStr(char *key, char *value, size_t *len)
{
    return nvs_get_str(my_handle, key, value, len);
}

/**
 * @brief 删除
 */
esp_err_t bsp_NVS_DeleteKey(char *key)
{
    return nvs_erase_key(my_handle, key);
}

/**
 * @brief 寻找KEY是否存在
 */
esp_err_t bsp_NVS_FindKey(char *key)
{
    // 第三个参数表示值的类型
    return nvs_find_key(my_handle, key, NULL);
}

/**
 * @brief 一键删除
 */
esp_err_t bsp_NVS_DeleteAllKey(void)
{
    return nvs_erase_all(my_handle);
}