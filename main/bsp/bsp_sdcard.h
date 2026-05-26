#ifndef __BSP_SDCARD_H__
#define __BSP_SDCARD_H__

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <dirent.h>

#define MAX_FILES_TO_LIST 30
#define MAX_FILENAME_LEN  300

extern char sd_file_names[MAX_FILES_TO_LIST][MAX_FILENAME_LEN];
extern int sd_file_count;

void init_sd_card_spi(void);

esp_err_t sd_list_files(const char *dir_path);

#endif /* __BSP_SDCARD_H__ */
