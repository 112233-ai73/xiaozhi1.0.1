# sd_card_example_main.c 代码说明文档

## 背景

`sd_card_example_main.c` 是 ESP-IDF `examples/storage/sd_card/sdspi` 示例的主入口文件，用于演示通过 SPI 外设访问 SD 卡，并把 SD 卡挂载成 FAT 文件系统。

该示例覆盖了 SD 卡项目中最常见的一条完整链路：

| 阶段 | 说明 |
|---|---|
| 配置挂载参数 | 设置挂载失败是否格式化、最大打开文件数、分配单元大小 |
| 初始化 SDSPI Host | 使用 `SDSPI_HOST_DEFAULT()` 创建 SD 卡 SPI 主机配置 |
| 初始化 SPI 总线 | 设置 MOSI/MISO/CLK/CS 引脚并调用 `spi_bus_initialize()` |
| 挂载 FATFS | 调用 `esp_vfs_fat_sdspi_mount()` 初始化 SD 卡并挂载到 `/sdcard` |
| 文件操作 | 使用标准 C/POSIX 接口写文件、重命名、读文件、删除文件 |
| 可选格式化 | 通过配置宏控制是否执行 FATFS 格式化 |
| 资源释放 | 卸载文件系统、释放 SPI 总线、释放可选电源控制驱动 |

源码位置：

`D:\Development\esp_idf_5.5.3\Espressif\frameworks\esp-idf-v5.5.3\examples\storage\sd_card\sdspi\main\sd_card_example_main.c`

## 总体流程

```text
app_main()
  -> 配置 esp_vfs_fat_sdmmc_mount_config_t
  -> 创建 SDSPI host
  -> 可选初始化片上 LDO 电源控制
  -> 配置 SPI 总线引脚
  -> spi_bus_initialize()
  -> 配置 sdspi_device_config_t
  -> esp_vfs_fat_sdspi_mount()
      -> 初始化 SD 卡
      -> 注册 VFS
      -> 挂载 FAT 文件系统到 /sdcard
  -> sdmmc_card_print_info()
  -> 写入 /sdcard/hello.txt
  -> 如果 /sdcard/foo.txt 已存在则删除
  -> hello.txt 重命名为 foo.txt
  -> 读取 foo.txt
  -> 可选格式化 SD 卡
  -> 写入并读取 /sdcard/nihao.txt
  -> esp_vfs_fat_sdcard_unmount()
  -> spi_bus_free()
  -> 可选释放 LDO 电源控制驱动
```

## 依赖头文件

| 头文件 | 作用 |
|---|---|
| `string.h` | 使用 `strchr()`、`memset()` 等字符串/内存接口 |
| `sys/unistd.h` | POSIX 文件接口，例如 `unlink()` |
| `sys/stat.h` | 使用 `stat()` 判断文件是否存在 |
| `esp_vfs_fat.h` | ESP-IDF FATFS VFS 挂载、卸载和格式化接口 |
| `sdmmc_cmd.h` | SD/MMC 卡初始化、命令和卡信息结构 |
| `sd_test_io.h` | 示例用 SD 卡引脚连线检查工具 |
| `sd_pwr_ctrl_by_on_chip_ldo.h` | 部分 SoC 使用片上 LDO 给 SD 卡 IO 供电时需要 |

## 关键宏与配置项

| 宏 | 作用 |
|---|---|
| `EXAMPLE_MAX_CHAR_SIZE` | 示例读写文件内容缓冲区大小，当前为 64 字节 |
| `MOUNT_POINT` | SD 卡挂载点，当前为 `/sdcard` |
| `PIN_NUM_MISO` | SPI MISO 引脚，来自 `CONFIG_EXAMPLE_PIN_MISO` |
| `PIN_NUM_MOSI` | SPI MOSI 引脚，来自 `CONFIG_EXAMPLE_PIN_MOSI` |
| `PIN_NUM_CLK` | SPI CLK 引脚，来自 `CONFIG_EXAMPLE_PIN_CLK` |
| `PIN_NUM_CS` | SPI CS 引脚，来自 `CONFIG_EXAMPLE_PIN_CS` |

这些 `CONFIG_EXAMPLE_...` 宏来自 SD SPI 示例自己的 `Kconfig.projbuild`。如果把代码迁移到你的工程，需要把这些配置项也迁移过来，或者直接改成固定 GPIO 编号，例如：

```c
#define PIN_NUM_MISO  13
#define PIN_NUM_MOSI  11
#define PIN_NUM_CLK   12
#define PIN_NUM_CS    10
```

具体引脚要以你的硬件原理图为准。

## 调试引脚检查配置

当启用 `CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS` 时，代码会建立一组用于检查 SD 卡连线的配置：

| 对象 | 说明 |
|---|---|
| `names[]` | 引脚名称：`CLK`、`MOSI`、`MISO`、`CS` |
| `pins[]` | 对应 GPIO 编号 |
| `adc_channels[]` | 可选 ADC 检查通道 |
| `pin_configuration_t config` | 传给 `check_sd_card_pins()` 的检查配置 |

挂载失败且错误不是单纯 FATFS 挂载失败时，示例会在错误分支调用：

```c
check_sd_card_pins(&config, pin_count);
```

这部分只用于示例调试。产品代码中通常不会依赖 ADC 方式检查连线，而是通过硬件设计、启动自检和错误日志定位。

## s_example_write_file()

`s_example_write_file()` 用于向指定路径写入字符串。

```c
static esp_err_t s_example_write_file(const char *path, char *data)
```

执行流程：

1. 调用 `fopen(path, "w")` 以写模式打开文件。
2. 如果打开失败，返回 `ESP_FAIL`。
3. 调用 `fprintf(f, data)` 写入数据。
4. 调用 `fclose(f)` 关闭文件。
5. 返回 `ESP_OK`。

注意点：

| 注意点 | 说明 |
|---|---|
| `"w"` 模式会覆盖原文件 | 如果目标文件已存在，原内容会被清空 |
| 示例使用 `fprintf(f, data)` | 产品代码更建议使用 `fprintf(f, "%s", data)`，避免格式化字符串风险 |
| 必须关闭文件 | 不关闭可能导致数据未刷入 FATFS 或文件句柄泄漏 |

## s_example_read_file()

`s_example_read_file()` 用于读取指定路径文件的第一行内容。

```c
static esp_err_t s_example_read_file(const char *path)
```

执行流程：

1. 调用 `fopen(path, "r")` 以读模式打开文件。
2. 使用 `fgets(line, sizeof(line), f)` 读取一行。
3. 关闭文件。
4. 查找换行符 `\n`，如果存在则替换为字符串结束符。
5. 打印读取到的内容。

该函数只读取最多 `EXAMPLE_MAX_CHAR_SIZE - 1` 个字符，不适合直接处理大文件。如果要读取音频、图片、模型或日志文件，应改成分块读取。

## app_main() 挂载配置

挂载参数由 `esp_vfs_fat_sdmmc_mount_config_t` 描述：

```c
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
```

| 字段 | 当前值 | 说明 |
|---|---:|---|
| `format_if_mount_failed` | 由 `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` 控制 | 挂载失败时是否自动分区并格式化 |
| `max_files` | 5 | FATFS 同时最多打开文件数量 |
| `allocation_unit_size` | 16 KB | FAT 文件系统分配单元大小 |

工程化建议：

| 场景 | 建议 |
|---|---|
| 用户 SD 卡可能有数据 | 不建议自动格式化 |
| 设备内置专用 SD 卡 | 可以在首次初始化失败时按产品策略格式化 |
| 小文件很多 | 分配单元不宜过大，否则空间浪费明显 |
| 大文件连续写入 | 较大的分配单元可能有利于连续写性能 |

## SDSPI Host 初始化

示例使用：

```c
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
```

`SDSPI_HOST_DEFAULT()` 会创建 SDSPI host 默认配置。注释中提到默认频率为 `SDMMC_FREQ_DEFAULT`，SDSPI 可设置范围通常为 400 kHz 到 20 MHz。

如果要固定 10 MHz，可以设置：

```c
host.max_freq_khz = 10000;
```

调试新硬件时，可以先降低频率，例如 400 kHz 或 5 MHz，确认连线、上拉和供电稳定后再提高。

## 可选 SD 卡电源控制

部分 SoC 的 SD 卡 IO 电源可能由内部 LDO 或外部供电提供。示例通过：

```c
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
```

控制是否创建片上 LDO 电源控制驱动。

流程如下：

1. 创建 `sd_pwr_ctrl_ldo_config_t`。
2. 调用 `sd_pwr_ctrl_new_on_chip_ldo()` 创建电源控制句柄。
3. 将句柄赋给 `host.pwr_ctrl_handle`。
4. 程序结束时调用 `sd_pwr_ctrl_del_on_chip_ldo()` 释放。

如果你的硬件是普通外部 3.3 V 给 SD 卡供电，一般不需要启用这段逻辑。

## SPI 总线配置

示例通过 `spi_bus_config_t` 配置 SPI 引脚：

```c
spi_bus_config_t bus_cfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
};
```

| 字段 | 说明 |
|---|---|
| `mosi_io_num` | 主机输出、SD 卡输入 |
| `miso_io_num` | SD 卡输出、主机输入 |
| `sclk_io_num` | SPI 时钟 |
| `quadwp_io_num` | Quad SPI 写保护线，本例不用，设置为 `-1` |
| `quadhd_io_num` | Quad SPI 保持线，本例不用，设置为 `-1` |
| `max_transfer_sz` | 单次最大传输大小，当前为 4000 字节 |

之后调用：

```c
ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
```

如果初始化失败，说明 SPI 外设、GPIO 或 DMA 资源初始化没有成功，示例直接返回。

## SDSPI 设备配置

```c
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
slot_config.gpio_cs = PIN_NUM_CS;
slot_config.host_id = host.slot;
```

这里配置 SD 卡作为 SPI 总线上的一个设备：

| 字段 | 说明 |
|---|---|
| `gpio_cs` | SD 卡片选引脚 |
| `host_id` | 绑定到前面初始化的 SPI host |

示例没有使用 CD 和 WP 信号：

| 信号 | 含义 | 示例处理 |
|---|---|---|
| CD | Card Detect，检测卡是否插入 | 未配置 |
| WP | Write Protect，写保护检测 | 未配置 |

如果你的 SD 卡座有 CD/WP 引脚，可以补充 `slot_config.gpio_cd` 和 `slot_config.gpio_wp`。

## 挂载文件系统

核心挂载调用是：

```c
ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
```

这个函数是 ESP-IDF 提供的便利接口，内部会完成多件事：

| 步骤 | 说明 |
|---|---|
| 初始化 SD 卡 | 通过 SDSPI 发送 SD 卡初始化命令 |
| 读取卡信息 | 填充 `sdmmc_card_t` |
| 挂载 FATFS | 将 FAT 文件系统挂载到 `/sdcard` |
| 注册 VFS | 使 `fopen()`、`rename()`、`stat()` 等标准接口可访问 SD 卡 |

错误处理分两类：

| 错误 | 含义 | 示例行为 |
|---|---|---|
| `ESP_FAIL` | SD 卡初始化成功但 FATFS 挂载失败 | 提示可开启格式化配置 |
| 其他错误码 | SD 卡初始化失败、SPI 通信异常、供电/上拉/引脚问题 | 打印错误名，并可选检查引脚 |

成功后调用：

```c
sdmmc_card_print_info(stdout, card);
```

用于打印卡名称、容量、速度等信息。

## 文件操作流程

### 写入 hello.txt

```c
const char *file_hello = MOUNT_POINT"/hello.txt";
snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
s_example_write_file(file_hello, data);
```

写入内容包含 SD 卡 CID 中的 `name` 字段，例如：

```text
Hello SD16G!
```

### 删除已存在的 foo.txt

```c
if (stat(file_foo, &st) == 0) {
    unlink(file_foo);
}
```

`stat()` 返回 0 表示文件存在。示例在重命名前先删除目标文件，避免 `rename()` 因目标已存在而失败或行为不符合预期。

### 重命名文件

```c
rename(file_hello, file_foo)
```

将 `/sdcard/hello.txt` 改名为 `/sdcard/foo.txt`。

### 读取 foo.txt

```c
s_example_read_file(file_foo);
```

读取并打印第一行内容。

### 可选格式化 FATFS

如果启用 `CONFIG_EXAMPLE_FORMAT_SD_CARD`，示例会调用：

```c
esp_vfs_fat_sdcard_format(mount_point, card);
```

格式化后再检查 `foo.txt` 是否还存在。正常情况下格式化完成后旧文件应不存在。

### 写入并读取 nihao.txt

```c
const char *file_nihao = MOUNT_POINT"/nihao.txt";
snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name);
s_example_write_file(file_nihao, data);
s_example_read_file(file_nihao);
```

这一步用于验证格式化后或普通流程后，SD 卡仍可正常写入和读取。

## 资源释放

程序最后释放资源：

```c
esp_vfs_fat_sdcard_unmount(mount_point, card);
spi_bus_free(host.slot);
```

| 接口 | 作用 |
|---|---|
| `esp_vfs_fat_sdcard_unmount()` | 卸载 FATFS，释放 SD 卡和 VFS 相关资源 |
| `spi_bus_free()` | 释放 SPI 总线资源 |
| `sd_pwr_ctrl_del_on_chip_ldo()` | 可选释放片上 LDO 电源控制驱动 |

如果中途 `return`，示例没有统一清理已经初始化的资源。作为官方演示可以接受；产品代码建议改成统一 `goto cleanup` 结构，避免挂载失败后 SPI 总线或电源控制句柄泄漏。

## 关键 API 说明

| API | 作用 |
|---|---|
| `SDSPI_HOST_DEFAULT()` | 创建 SDSPI host 默认配置 |
| `spi_bus_initialize()` | 初始化 SPI 总线 |
| `SDSPI_DEVICE_CONFIG_DEFAULT()` | 创建 SDSPI 设备默认配置 |
| `esp_vfs_fat_sdspi_mount()` | 初始化 SD 卡并挂载 FATFS |
| `sdmmc_card_print_info()` | 打印 SD 卡信息 |
| `fopen()` | 打开文件 |
| `fprintf()` | 写文件 |
| `fgets()` | 读文件一行 |
| `stat()` | 查询文件状态 |
| `unlink()` | 删除文件 |
| `rename()` | 重命名文件 |
| `esp_vfs_fat_sdcard_format()` | 格式化 FATFS |
| `esp_vfs_fat_sdcard_unmount()` | 卸载 SD 卡 FATFS |
| `spi_bus_free()` | 释放 SPI 总线 |

## 与项目迁移相关的注意点

| 注意点 | 说明 |
|---|---|
| `CONFIG_EXAMPLE_PIN_...` 需要来源 | 搬到自己工程时要迁移 Kconfig，或改成固定 GPIO |
| SD 卡需要上拉电阻 | SD 卡 SPI 模式下 CMD/DAT 相关线通常需要合适上拉 |
| CS 引脚必须正确 | 多 SPI 设备共总线时，每个设备要有独立 CS |
| 挂载失败不要盲目格式化 | 用户卡上可能有重要数据 |
| 文件写完要关闭 | `fclose()` 后数据才更可靠地落盘 |
| 长期运行项目要避免提前 return 泄漏资源 | 建议使用统一清理路径 |
| 示例只演示文本小文件 | 大文件应使用分块读写和错误恢复 |

## 推荐产品化改造

如果要把这个示例封装到你的 `main/bsp` 目录，建议拆成：

| 文件 | 职责 |
|---|---|
| `main/bsp/bsp_sdcard.h` | 声明 `bsp_sdcard_mount()`、`bsp_sdcard_unmount()` 等接口 |
| `main/bsp/bsp_sdcard.c` | 管理 SPI 总线、SD 卡挂载、卸载和状态 |
| `main/Kconfig.projbuild` | 可选配置 SD 卡 GPIO、是否自动格式化、SPI 频率 |

建议接口形态：

```c
esp_err_t bsp_sdcard_mount(void);
void bsp_sdcard_unmount(void);
bool bsp_sdcard_is_mounted(void);
```

如果只是当前板子固定硬件测试，也可以先直接写死 GPIO，不引入 Kconfig：

```c
#define BSP_SDCARD_PIN_MISO  13
#define BSP_SDCARD_PIN_MOSI  11
#define BSP_SDCARD_PIN_CLK   12
#define BSP_SDCARD_PIN_CS    10
```

## 验证方式

| 测试场景 | 预期结果 |
|---|---|
| SD 卡插入且引脚正确 | 打印 `Filesystem mounted` 和 SD 卡信息 |
| 写入 `hello.txt` | 日志打印 `File written` |
| 重命名为 `foo.txt` | 日志打印 `Renaming file /sdcard/hello.txt to /sdcard/foo.txt` |
| 读取 `foo.txt` | 打印 `Read from file: 'Hello ...'` |
| 写入并读取 `nihao.txt` | 打印 `Read from file: 'Nihao ...'` |
| 未插卡或接线错误 | 挂载失败，并提示检查上拉电阻或引脚 |
| 文件系统损坏 | 可能返回 `ESP_FAIL`，按配置决定是否格式化 |

## 总结

该文件是一个标准的 ESP-IDF SDSPI SD 卡入门模板。它用 `esp_vfs_fat_sdspi_mount()` 把 SD 卡初始化、FATFS 挂载和 VFS 注册封装成一步，然后通过标准文件接口验证读写、删除和重命名能力。

迁移到自己的项目时，最需要关注三件事：GPIO 引脚来源、挂载失败处理策略、资源释放路径。示例代码适合理解流程，产品代码建议进一步封装成独立 SD 卡 BSP 模块。
