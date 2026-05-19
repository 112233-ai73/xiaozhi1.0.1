# ESP-IDF SDMMC SD 卡示例代码说明

## 背景

本文分析 ESP-IDF `examples/storage/sd_card/sdmmc/main/sd_card_example_main.c`。该示例使用 SoC 的 `SDMMC` 外设访问 SD 卡，通过 `esp_vfs_fat_sdmmc_mount()` 将 SD 卡上的 FAT 文件系统挂载到 VFS 路径 `/sdcard`，再用标准 C/POSIX 文件接口演示写入、重命名、读取、可选格式化和卸载。

源码位置：

`d:\Development\esp_idf_5.5.3\Espressif\frameworks\esp-idf-v5.5.3\examples\storage\sd_card\sdmmc\main\sd_card_example_main.c`

## 结论概览

这个文件不是完整的产品级 SD 卡管理模块，而是一条最小可运行的验证链路。它把 SDMMC host 配置、slot 配置、可选电源控制、FATFS 挂载和文件操作集中写在 `app_main()` 中，方便确认硬件连线、上拉电阻、供电、menuconfig 和 SD 卡文件系统是否正常。

整体流程如下：

```text
app_main()
  -> 配置 esp_vfs_fat_sdmmc_mount_config_t
  -> 创建 SDMMC host，按配置选择默认/HS/UHS-I 速度
  -> 可选创建片上 LDO 电源控制句柄
  -> 创建 SDMMC slot，配置 UHS-I flag、总线宽度、GPIO 和内部上拉
  -> esp_vfs_fat_sdmmc_mount()
      -> 初始化 SDMMC 外设和 SD 卡
      -> 注册 FATFS 到 VFS
      -> 将文件系统挂载到 /sdcard
  -> sdmmc_card_print_info()
  -> 写入 /sdcard/hello.txt
  -> 删除旧 /sdcard/foo.txt 并将 hello.txt 重命名为 foo.txt
  -> 读取 /sdcard/foo.txt
  -> 可选格式化 FATFS
  -> 写入并读取 /sdcard/nihao.txt
  -> esp_vfs_fat_sdcard_unmount()
  -> 可选释放片上 LDO 电源控制句柄
```

## 文件结构

| 区域 | 行号 | 作用 |
|---|---:|---|
| 头文件与宏定义 | `11-27` | 引入 VFS/FATFS、SDMMC、引脚检测、电源控制相关接口，定义挂载点和缓冲区大小 |
| 调试引脚配置 | `29-62` | 打开 `CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS` 后，记录 CLK/CMD/D0-D3 及可选 ADC 通道，用于失败诊断 |
| `s_example_write_file()` | `64-77` | 用 `fopen()`、`fprintf()`、`fclose()` 写入文本 |
| `s_example_read_file()` | `79-99` | 读取文件第一行，去掉换行后打印 |
| `app_main()` | `101-286` | 完成 SD 卡初始化、挂载、文件读写、格式化、卸载和资源释放 |

## 依赖头文件

| 头文件 | 作用 |
|---|---|
| `string.h` | 使用 `strchr()`、`memset()` 等字符串/内存函数 |
| `sys/unistd.h` | 提供 `unlink()` 等 POSIX 文件接口 |
| `sys/stat.h` | 使用 `stat()` 判断文件是否存在 |
| `esp_vfs_fat.h` | FATFS 挂载、卸载、格式化以及 VFS 适配 |
| `sdmmc_cmd.h` | SD/MMC 卡命令、卡信息结构和辅助函数 |
| `driver/sdmmc_host.h` | SDMMC host 和 slot 驱动配置 |
| `sd_test_io.h` | 示例中的 SD 卡连线检测工具 |
| `sd_pwr_ctrl_by_on_chip_ldo.h` | 部分 SoC 使用片上 LDO 给 SD IO 供电时需要 |

## 关键宏与配置项

| 宏/配置项 | 行号 | 说明 |
|---|---:|---|
| `EXAMPLE_MAX_CHAR_SIZE` | `22` | 文本读写缓冲区大小，当前为 64 字节 |
| `MOUNT_POINT` | `26` | SD 卡挂载路径，当前为 `/sdcard` |
| `EXAMPLE_IS_UHS1` | `27` | 判断是否启用 UHS-I SDR50 或 DDR50 |
| `CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED` | `109-113` | 挂载失败时是否允许自动格式化 |
| `CONFIG_EXAMPLE_SDMMC_SPEED_HS` | `132-133` | 使用 SDMMC High Speed 频率 |
| `CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50` | `134-137` | 使用 UHS-I SDR50，slot 切到 `SDMMC_HOST_SLOT_0` 并关闭 DDR flag |
| `CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50` | `138-140` | 使用 UHS-I DDR50，slot 切到 `SDMMC_HOST_SLOT_0` |
| `CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO` | `146-158`、`279-285` | 创建并释放片上 LDO 电源控制驱动 |
| `CONFIG_EXAMPLE_SDMMC_BUS_WIDTH_4` | `168-172`、`180-184` | 控制 SDMMC 使用 4-bit 或 1-bit 总线 |
| `CONFIG_SOC_SDMMC_USE_GPIO_MATRIX` | `176-185` | 支持 GPIO Matrix 的芯片上显式配置 CLK/CMD/D0-D3 |
| `CONFIG_EXAMPLE_FORMAT_SD_CARD` | `245-258` | 主动格式化 SD 卡并验证旧文件是否消失 |
| `CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS` | `29-62`、`202-204` | 初始化失败时执行 SD 卡引脚诊断 |

## 详细设计

### FATFS 挂载配置

`app_main()` 首先构造 `esp_vfs_fat_sdmmc_mount_config_t mount_config`：

```c
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
```

| 字段 | 当前取值 | 说明 |
|---|---:|---|
| `format_if_mount_failed` | 由配置决定 | `true` 时挂载失败会尝试分区并格式化 SD 卡 |
| `max_files` | `5` | FATFS 同时打开文件数量上限 |
| `allocation_unit_size` | `16 KB` | FAT 文件系统分配单元大小 |

自动格式化会清空卡内原有数据。测试工程可以打开，产品代码应由用户确认、量产策略或明确的初始化状态来决定是否格式化。

### SDMMC host 配置

代码使用 `SDMMC_HOST_DEFAULT()` 创建默认 host：

```c
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
```

随后根据配置选择速度：

| 配置 | 行号 | 行为 |
|---|---:|---|
| 默认 | `131` | 使用默认 SDMMC 频率，注释说明默认初始化为 20 MHz |
| `CONFIG_EXAMPLE_SDMMC_SPEED_HS` | `132-133` | 设置 `host.max_freq_khz = SDMMC_FREQ_HIGHSPEED` |
| `CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50` | `134-137` | 使用 slot 0，设置 `SDMMC_FREQ_SDR50`，并清除 `SDMMC_HOST_FLAG_DDR` |
| `CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50` | `138-140` | 使用 slot 0，设置 `SDMMC_FREQ_DDR50` |

UHS-I 模式对供电、电平、走线、卡座和 SD 卡本身都有更高要求。如果普通接线或转接板不稳定，建议先用默认速度或 HS 模式验证。

### 可选片上 LDO 电源控制

当 `CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO` 打开时，代码会创建 SD IO 的片上 LDO 电源控制驱动：

```text
sd_pwr_ctrl_new_on_chip_ldo()
  -> 获取 pwr_ctrl_handle
  -> host.pwr_ctrl_handle = pwr_ctrl_handle
  -> 挂载流程通过 host 控制 SD IO 电源
```

对应代码位于 `146-158`。流程末尾在 `279-285` 调用 `sd_pwr_ctrl_del_on_chip_ldo()` 释放。普通外部 3.3 V 供电的 SD 卡设计通常不需要启用这一分支。

### SDMMC slot 配置

slot 使用 `SDMMC_SLOT_CONFIG_DEFAULT()` 创建，然后按配置补充：

| 配置点 | 行号 | 说明 |
|---|---:|---|
| UHS-I flag | `163-165` | `EXAMPLE_IS_UHS1` 为真时设置 `SDMMC_SLOT_FLAG_UHS1` |
| 总线宽度 | `168-172` | 4-bit 模式使用 D0-D3，否则只使用 D0 |
| GPIO Matrix 引脚 | `176-185` | 设置 `clk`、`cmd`、`d0`，4-bit 下再设置 `d1`、`d2`、`d3` |
| 内部上拉 | `187-190` | 设置 `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` |

示例注释明确说明：内部上拉只适合调试和示例用途，实际硬件应在 SDMMC 总线上连接 10k 外部上拉电阻。

### 调试引脚检测

打开 `CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS` 后，代码会构造 `pin_configuration_t config`：

| 对象 | 行号 | 说明 |
|---|---:|---|
| `names[]` | `30` | 记录 `CLK`、`CMD`、`D0` 到 `D3` 名称 |
| `pins[]` | `31-39` | 记录对应 GPIO 编号 |
| `pin_count` | `41` | 根据 `pins[]` 自动计算引脚数量 |
| `adc_channels[]` | `43-53` | 可选 ADC 检测通道 |
| `config` | `55-61` | 传给 `check_sd_card_pins()` 的配置 |

当挂载失败且返回值不是 `ESP_FAIL` 时，代码会调用：

```c
check_sd_card_pins(&config, pin_count);
```

这适合示例调试。产品代码通常会把连线问题交给硬件验证、启动自检、错误码和日志定位，不会依赖 ADC 式引脚检测。

### 挂载与错误处理

核心挂载调用是：

```c
ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
```

这个 API 是 ESP-IDF 的便利封装，通常会完成以下工作：

| 步骤 | 说明 |
|---|---|
| 初始化 SDMMC 外设 | 按 `host` 和 `slot_config` 配置外设和引脚 |
| 初始化 SD 卡 | 发送 SD/MMC 初始化命令，识别卡类型和能力 |
| 挂载 FATFS | 将卡上的 FAT 文件系统挂载到指定路径 |
| 注册 VFS | 让 `fopen()`、`stat()`、`rename()` 等标准接口可访问 `/sdcard` |
| 返回 `card` | 后续可用于打印卡信息、格式化和卸载 |

错误处理分两类：

| 返回情况 | 行号 | 示例行为 |
|---|---:|---|
| `ESP_FAIL` | `196-198` | 认为主要是 FATFS 挂载失败，提示可打开自动格式化配置 |
| 其他错误 | `199-205` | 认为 SD 卡初始化或硬件通信失败，提示检查上拉，并可选检查引脚 |

示例失败后直接 `return`。生产代码通常还需要降频重试、重新上电、卡检测、明确错误上报和资源清理。

### SD 卡信息输出

挂载成功后调用：

```c
sdmmc_card_print_info(stdout, card);
```

该函数会打印 SD 卡类型、容量、总线宽度、频率、CID/CSD 等信息。后续文件内容中的 `<card name>` 来自 `card->cid.name`。

### 写文件辅助函数

`s_example_write_file()` 位于 `64-77`：

```text
fopen(path, "w")
  -> fprintf(f, data)
  -> fclose(f)
```

注意点：

| 注意点 | 说明 |
|---|---|
| `"w"` 会覆盖旧文件 | 目标文件已存在时会清空原内容 |
| 当前使用 `fprintf(f, data)` | 示例固定数据问题不大；若 `data` 来自外部输入，建议改成 `fprintf(f, "%s", data)` 或 `fputs(data, f)` |
| 写完立即 `fclose()` | 关闭文件会推动缓冲数据落到 FATFS |

### 读文件辅助函数

`s_example_read_file()` 位于 `79-99`：

```text
fopen(path, "r")
  -> fgets(line, sizeof(line), f)
  -> fclose(f)
  -> strchr(line, '\n') 去掉换行
  -> ESP_LOGI() 打印第一行
```

它只读取第一行，且最多读取 `EXAMPLE_MAX_CHAR_SIZE - 1` 个字符。用于示例验证足够，但不适合直接处理大文件、二进制文件、音频文件或日志流。

### 文件操作流程

第一组文件操作位于 `216-242`：

```text
/sdcard/hello.txt
  -> 写入 "Hello <card name>!"
  -> 如果 /sdcard/foo.txt 已存在，先 unlink()
  -> rename("/sdcard/hello.txt", "/sdcard/foo.txt")
  -> 读取 /sdcard/foo.txt
```

这里使用 `stat(file_foo, &st) == 0` 判断目标文件是否存在。存在则先 `unlink(file_foo)`，避免 `rename()` 因目标文件已存在而失败或行为不符合预期。

第二组文件操作位于 `260-272`：

```text
/sdcard/nihao.txt
  -> 写入 "Nihao <card name>!"
  -> 读取 /sdcard/nihao.txt
```

如果启用了 `CONFIG_EXAMPLE_FORMAT_SD_CARD`，第二组操作会发生在格式化之后，用于验证格式化后仍能正常写入和读取。

### 可选格式化流程

当 `CONFIG_EXAMPLE_FORMAT_SD_CARD` 打开时，代码执行：

```c
ret = esp_vfs_fat_sdcard_format(mount_point, card);
```

随后检查 `foo.txt` 是否仍存在：

| 判断 | 行号 | 说明 |
|---|---:|---|
| `stat(file_foo, &st) == 0` | `252-254` | 文件仍存在，认为格式化结果异常 |
| `stat()` 失败 | `255-257` | 文件不存在，认为格式化完成 |

这个分支会清空 SD 卡文件系统内容，只应在可擦除数据的测试卡或明确的产品初始化流程中使用。

### 卸载与资源释放

正常流程最后执行：

```c
esp_vfs_fat_sdcard_unmount(mount_point, card);
```

若启用片上 LDO 电源控制，还会执行：

```c
sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
```

需要注意：示例在多个中途失败路径直接 `return`，例如写文件失败、重命名失败、读取失败、格式化失败。这些路径不会走到统一卸载逻辑。示例程序短生命周期可以接受，产品代码建议改为 `goto cleanup` 结构。

## 数据流与资源生命周期

| 资源 | 创建/获取 | 使用 | 释放 |
|---|---:|---|---:|
| `mount_config` | `108-116` | 传给 `esp_vfs_fat_sdmmc_mount()` | 栈变量自动释放 |
| `host` | `131-141` | 配置 SDMMC host、承载可选电源控制句柄 | 栈变量自动释放 |
| `pwr_ctrl_handle` | `150-157` | 赋给 `host.pwr_ctrl_handle` | `280` |
| `slot_config` | `162-190` | 配置 slot、总线宽度、引脚和上拉 | 栈变量自动释放 |
| `card` | `193` | 打印卡信息、格式化、卸载 | `275` |
| `/sdcard` 挂载点 | `193` | 所有 `fopen/stat/rename/unlink` 文件操作 | `275` |
| `FILE *f` | `67`、`82` | 文件读写 | `73`、`89` |

## 错误路径与风险

| 错误点 | 当前处理 | 风险/说明 |
|---|---|---|
| 创建 LDO 失败 | 打日志并 `return` | 电源控制不可用，继续挂载没有意义 |
| 挂载失败 | 区分 `ESP_FAIL` 和其他错误后 `return` | 不做降频、重试、重新上电或卡检测 |
| 写 `hello.txt` 失败 | 直接 `return` | 如果已经挂载成功，不会执行卸载 |
| `rename()` 失败 | 直接 `return` | 不会执行卸载 |
| 读 `foo.txt` 失败 | 直接 `return` | 不会执行卸载 |
| 格式化失败 | 直接 `return` | 不会执行卸载 |
| 写/读 `nihao.txt` 失败 | 直接 `return` | 不会执行卸载 |
| 删除 LDO 驱动失败 | 打日志并 `return` | 文件系统已卸载，但电源控制释放异常 |

## 生产化改造建议

| 建议 | 原因 |
|---|---|
| 将挂载、卸载、文件测试拆成独立函数 | 避免 `app_main()` 过长，方便业务复用 |
| 使用统一 `cleanup` 路径 | 确保任何失败路径都能卸载 FATFS 并释放电源控制 |
| 将 `fprintf(f, data)` 改为 `fputs(data, f)` | 避免格式化字符串风险 |
| 检查 `fgets()` 返回值 | 当前读取失败时可能继续处理未初始化或旧缓冲内容 |
| 根据硬件补充 CD/WP | 示例默认不使用卡检测和写保护 |
| 不依赖内部上拉作为正式方案 | SDMMC 总线应按硬件规范连接外部上拉 |
| 高速模式增加降频重试 | 转接板、长线或部分卡在高频下可能不稳定 |
| 掉电敏感场景增加同步策略 | 可按需求使用 `fflush()`、`fsync()` 或事务式写入 |
| 控制格式化入口 | 避免误格式化用户 SD 卡数据 |

## 与 SDSPI 示例的主要差异

| 维度 | SDMMC 示例 | SDSPI 示例 |
|---|---|---|
| 物理接口 | SoC 原生 `SDMMC` 外设 | SPI 总线 |
| host 默认配置 | `SDMMC_HOST_DEFAULT()` | `SDSPI_HOST_DEFAULT()` |
| slot/device 配置 | `sdmmc_slot_config_t` | `sdspi_device_config_t` 和 `spi_bus_config_t` |
| 总线宽度 | 支持 1-bit/4-bit | SPI 单线协议 |
| 性能潜力 | 通常更高，适合原生 SD 卡连接 | 通常较低，但引脚更灵活 |
| 硬件要求 | CMD/CLK/D0-D3、外部上拉、部分模式需要更严格供电和走线 | MOSI/MISO/SCLK/CS，接线直观 |
| 总线共享 | SDMMC 专用外设语义更强 | SPI 可与其他设备共享，但要管理 CS 和总线参数 |

## 移植到项目中的步骤

1. 根据硬件确认使用 1-bit 还是 4-bit SDMMC。
2. 根据原理图配置 `CONFIG_EXAMPLE_PIN_CLK`、`CONFIG_EXAMPLE_PIN_CMD`、`CONFIG_EXAMPLE_PIN_D0-D3`。
3. 确认 SDMMC 总线外部上拉、电源、电平和卡座连接满足要求。
4. 先用默认速度验证，再根据稳定性切换 HS 或 UHS-I。
5. 如果芯片和板级设计需要片上 LDO，启用并配置 `CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO`。
6. 将 `esp_vfs_fat_sdmmc_mount()` 前后的 host/slot/mount 配置封装成项目自己的 `sdcard_mount()`。
7. 将示例读写替换为业务文件路径和读写策略。
8. 增加统一错误清理逻辑，保证挂载成功后任何失败都会卸载。
9. 根据产品需求决定是否支持热插拔、卡检测、写保护、自动格式化和降频重试。

## 验证方式

| 验证项 | 预期结果 |
|---|---|
| 正常插卡并启动 | 日志出现 `Filesystem mounted`，并打印 SD 卡信息 |
| 写入 `hello.txt` | 日志出现 `File written` |
| 重命名为 `foo.txt` | 日志出现 `Renaming file /sdcard/hello.txt to /sdcard/foo.txt` |
| 读取 `foo.txt` | 日志输出 `Read from file: 'Hello <card name>!'` |
| 启用格式化 | `foo.txt` 不再存在，日志输出 `file doesn't exist, formatting done` |
| 写入并读取 `nihao.txt` | 日志输出 `Read from file: 'Nihao <card name>!'` |
| 正常结束 | 日志出现 `Card unmounted` |
| 拔卡或接线错误 | 挂载失败，并提示检查上拉或引脚 |

## 总结

`sd_card_example_main.c` 展示的是 SDMMC + FATFS 的完整入门链路：配置 SDMMC host/slot，挂载 FATFS 到 `/sdcard`，再通过标准文件接口验证读写能力。它非常适合做硬件 bring-up 和 ESP-IDF 配置验证。

移植到正式项目时，重点不在照搬文件读写示例，而在保留 SDMMC 初始化和挂载思路，同时补齐统一清理、错误恢复、格式化保护、数据落盘策略和硬件检测逻辑。
