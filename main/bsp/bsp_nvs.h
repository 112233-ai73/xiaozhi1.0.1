#ifndef __BSP_NVS_H__
#define __BSP_NVS_H__

#include "nvs_flash.h"
#include "nvs.h"

/**
 * @brief 初始化
 */
void bsp_NVS_Init(void);

/**
 * @brief 向flash写数据
 */
esp_err_t bsp_NVS_WriteStr(char *key, char *value);

/**
 * @brief 向flash读数据
 */
esp_err_t bsp_NVS_ReadStr(char *key, char *value, size_t *len);

/**
 * @brief 删除
 */
esp_err_t bsp_NVS_DeleteKey(char *key);

/**
 * @brief 寻找KEY是否存在
 */
esp_err_t bsp_NVS_FindKey(char *key);

/**
 * @brief 一键删除
 */
esp_err_t bsp_NVS_DeleteAllKey(void);

#endif /* __BSP_NVS_H__ */
