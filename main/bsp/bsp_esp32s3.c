#include "bsp_esp32s3.h"
#include <esp_wifi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bsp_nvs.h"
// 获取板子的mac地址函数
char *bsp_get_mac(void)
{
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

    char *mac_addr = heap_caps_malloc(18, MALLOC_CAP_SPIRAM);
    sprintf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x", eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);

    return mac_addr;
}

// 封装一个不使用第三方库的生成uuid的函数
void generate_uuid(char *buffer)
{
    const char *template = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    const char *hex_chars = "0123456789abcdef";
    static int initialized = 0;

    if (!initialized)
    {
        srand(time(NULL));
        initialized = 1;
    }

    for (int i = 0; i < 36; i++)
    {
        if (template[i] == 'x')
        {
            buffer[i] = hex_chars[rand() % 16];
        }
        else if (template[i] == 'y')
        {
            buffer[i] = hex_chars[8 + (rand() % 4)];
        }
        else
        {
            buffer[i] = template[i];
        }
    }

    buffer[36] = '\0';
}

// 获取板子的uuid函数
char *bsp_get_uuid(void)
{
    char *uuid = malloc(37);

    // 去NVS中查询UUID是否存在
    if (bsp_NVS_FindKey(UUID_KEY) == ESP_OK)
    {
        // 获取uuid
        size_t len = 37;
        bsp_NVS_ReadStr(UUID_KEY, uuid, &len);

        // 返回
        return uuid;
    }

    // 生成uuid
    generate_uuid(uuid);

    // 保存到NVS中
    bsp_NVS_WriteStr(UUID_KEY, uuid);
    return uuid;
}