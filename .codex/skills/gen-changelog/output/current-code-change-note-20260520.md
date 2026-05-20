# 当前代码改动说明

## 背景

本次说明基于当前工作区相对 `HEAD` 的代码差异生成，覆盖音频初始化、MP3 播放、语音识别状态流转、串口收发、Wi-Fi 日志、SD 卡挂载方式和历史说明文档清理。

当前改动的主线有两条：

1. 将多个业务模块的 `ESP_LOGx` 日志替换为项目封装的 `MY_LOGx`，并通过 `DEBUG` 宏统一控制日志输出。
2. 将 SD 卡示例代码从 SDMMC 4 线模式改为 SDSPI 模式，并增加简单文本读写测试。

## 改动总览

| 类型 | 文件 | 说明 |
|---|---|---|
| 日志控制 | `main/com/com_debug.h` | `DEBUG` 从 `1` 改为 `0`，默认关闭 `MY_LOGx` 输出 |
| 音频初始化 | `main/audio/audio_init.c` | 初始化阶段日志改用 `MY_LOGI/MY_LOGD` |
| MP3 播放 | `main/audio/audio_mp3_decode.c` | SPIFFS、解码、播放状态相关日志改用 `MY_LOGx` |
| 语音识别 | `main/audio/audio_sr.c` | VAD、MultiNet、超时和异常日志改用 `MY_LOGx` |
| 指令处理 | `main/audio/audio_sr_handler.c` | 识别结果处理日志改用 `MY_LOGI` |
| Wi-Fi | `main/bsp/bsp_wifi.c` | STA 连接流程日志改用 `MY_LOGx` |
| 状态管理 | `main/com/com_status.c` | 唤醒状态、工作状态切换日志改用 `MY_LOGI` |
| 主入口 | `main/main.c` | 启动日志和开机 MP3 播放失败日志改用 `MY_LOGx` |
| 串口 | `main/usart/usart_init.c` | 发送、接收、校验日志改用 `MY_LOGx` |
| SD 卡 | `main/bsp/bsp_sdcard.c` | 从 SDMMC 4 线挂载示例改为 SDSPI 挂载和文本读写示例 |
| 文档清理 | `.codex/skills/gen-changelog/output/*.md` | 删除 3 份旧 SD 卡相关说明文档 |

## 推荐方案：统一日志控制 + SDSPI 示例迁移

### 统一日志控制

当前多数模块已引入 `com/com_debug.h`，并将直接调用 `ESP_LOGI/W/E/D` 的位置替换为 `MY_LOGI/W/E/D`。

`MY_LOGx` 的行为由 `main/com/com_debug.h` 中的 `DEBUG` 控制：

| `DEBUG` 值 | 行为 |
|---:|---|
| `1` | `MY_LOGx` 展开为带文件名和行号的 `ESP_LOGx` |
| `0` | `MY_LOGx` 展开为空宏，相关日志不输出 |

这意味着当前默认状态下，已经替换为 `MY_LOGx` 的日志不会输出，适合减少运行时串口日志噪声。

### SD 卡 SDSPI 示例迁移

`main/bsp/bsp_sdcard.c` 删除了原 SDMMC 4 线模式相关内容：

- `SD_PIN_CLK/CMD/D0/D1/D2/D3`
- `SD_LDO_CHAN_ID`
- `sd_pwr_ctrl_handle_t pwr_ctrl_handle`
- `init_sd_card`
- `sd_write_binary_chunk`
- `sd_read_file_example`

新增 SDSPI 相关实现：

- `PIN_NUM_MISO`
- `PIN_NUM_MOSI`
- `PIN_NUM_CLK`
- `PIN_NUM_CS`
- `sd_write_text_file`
- `sd_read_text_file`
- `init_sd_card_spi`

新的 SD 卡初始化流程为：

```text
配置 FAT 挂载参数
-> 创建 SDSPI host
-> 配置 SPI bus 引脚
-> spi_bus_initialize()
-> 配置 SD 卡 CS 引脚
-> esp_vfs_fat_sdspi_mount()
-> 打印卡信息
-> 写入 /sdcard/test.txt
-> 读取 /sdcard/test.txt
```

## 详细设计

### 日志封装迁移

本次迁移的核心目标是让日志输出受项目级调试开关控制，而不是各模块直接输出。

典型替换关系：

| 原实现 | 新实现 | 影响 |
|---|---|---|
| `ESP_LOGI(TAG, "...")` | `MY_LOGI("...")` | 日志由 `DEBUG` 宏统一开关 |
| `ESP_LOGW(TAG, "...")` | `MY_LOGW("...")` | 警告日志默认关闭 |
| `ESP_LOGE(TAG, "...")` | `MY_LOGE("...")` | 错误日志默认关闭 |
| `ESP_LOGD(TAG, "...")` | `MY_LOGD("...")` | 调试日志默认关闭 |

需要注意的是，`MY_LOGx` 宏内部自己生成 tag，因此迁移后虽然部分文件仍保留 `static const char *TAG`，但这些 `TAG` 在对应日志语句中不再使用。

### 音频与语音识别状态流

本次没有改变音频主流程，只改变日志输出方式。

当前状态流仍然是：

```text
app_main
-> usart_init()
-> audio_init()
-> audio_mp3_play_file_async(STARTUP_MP3_FILE)
-> app_sr_start()
```

语音识别侧状态仍围绕 `com_status` 与 `is_awake` 工作：

```text
IDLE
-> 检测到语音进入 LISTENING
-> MultiNet 检测到命令或超时
-> 回到 IDLE
```

MP3 播放侧仍保留与语音识别的互斥逻辑：

- 当 `com_status == LISTENING` 时跳过 MP3 播放。
- 播放互斥由 `s_playback_mutex` 控制。
- 播放完成后调用 `com_status_change(IDLE)`。

### 串口收发

串口模块的行为未改变：

- `usart_send_data()` 仍按 `data[1]` 作为包长发送。
- 接收任务仍只寻找 `FRAME_HEADER_BB`。
- 接收后继续执行长度检查、剩余字节读取和异或校验。

本次变化只是把发送、非法长度、校验成功、校验失败等日志改为 `MY_LOGx`。

### SD 卡模块

SD 卡代码从 SDMMC 4 线模式切到 SDSPI 模式，接口职责也从“二进制分块写入 + 示例读取”变为“文本文件写入 + 文本文件读取 + 初始化时自测”。

当前 SDSPI 固定引脚为：

| 信号 | GPIO |
|---|---:|
| MISO | `13` |
| MOSI | `11` |
| CLK | `12` |
| CS | `10` |

挂载点为：

```text
/sdcard
```

初始化成功后会写入并读取：

```text
/sdcard/test.txt
```

## 可删除或待清理代码

| 可删除项 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| 未使用的 `TAG` | `main/audio/audio_init.c` 等 | 多处 | 日志调用已经改为 `MY_LOGx`，不再传入局部 `TAG` |
| 未使用的 `TAG` | `main/usart/usart_init.c` | `~1` 行 | 文件中仍定义 `static const char *TAG = "USART"`，但日志已改用 `MY_LOGx` |
| SDMMC 相关头文件 | `main/bsp/bsp_sdcard.h` | `~2` 行 | 当前实现已切到 SDSPI，`driver/sdmmc_host.h` 和 `sd_pwr_ctrl_by_on_chip_ldo.h` 是否仍需要应重新确认 |
| 旧 SD 卡说明文档 | `.codex/skills/gen-changelog/output/` | 3 个文件 | 当前工作区已删除旧的 SDMMC/SDSPI/IO 测试说明文档 |

## 当前风险与待确认事项

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| `DEBUG=0` 会关闭 `MY_LOGx` | 运行时需要定位问题 | 音频、语音、串口、Wi-Fi 大部分日志不可见 | 调试阶段临时改回 `DEBUG=1` 验证关键流程 |
| 错误日志也被关闭 | 模块初始化失败或运行失败 | 失败原因可能无法从串口日志看到 | 评估是否让 `MY_LOGE` 在生产环境也保留 |
| SD 卡头文件未声明新函数 | 其他文件调用 `init_sd_card_spi()` 等函数 | 可能出现隐式声明或无法调用 | 在 `bsp_sdcard.h` 增加公开函数声明 |
| SDSPI 类型依赖需要确认 | 编译 `bsp_sdcard.c` | 若头文件不完整，可能编译失败 | 执行一次完整构建 |
| SD 卡模块仍使用 `ESP_LOGx` | 期望所有模块统一日志开关 | SD 卡日志不受 `DEBUG` 控制 | 后续统一替换为 `MY_LOGx`，或明确保留直接日志 |
| `init_sd_card_spi()` 初始化后执行读写自测 | 每次初始化 SD 卡 | 会覆盖 `/sdcard/test.txt` | 若进入正式业务，应把自测逻辑移到测试函数 |
| 串口接收仍只识别 `0xBB` | 外部发送 `0xAA` 任务帧给本芯片 | `0xAA` 数据会被忽略 | 若需要本芯片执行 `AA` 指令，应扩展帧头分发逻辑 |

## 与当前方案的映射

| 当前实现 | 新方案 | 说明 |
|---|---|---|
| 直接调用 `ESP_LOGx` | 调用 `MY_LOGx` | 大部分业务模块已完成迁移 |
| `DEBUG=1` | `DEBUG=0` | 默认关闭封装日志输出 |
| SDMMC 4 线模式 | SDSPI 模式 | SD 卡接线方式和挂载 API 已变化 |
| 二进制追加写入 | 文本覆盖写入 | SD 卡写入接口语义已变化 |
| 读取任意文件前 127 字节 | 读取文本首行 | SD 卡读取接口更偏测试示例 |
| 旧 SD 卡说明文档保留 | 删除旧说明文档 | 输出目录中旧文档被清理 |

## 迁移步骤

1. 确认是否接受 `DEBUG=0` 作为默认行为；如果仍处于调试阶段，建议临时保持 `DEBUG=1`。
2. 对 `main/bsp/bsp_sdcard.c` 执行一次完整编译，确认 SDSPI 相关类型和 API 头文件齐全。
3. 在 `main/bsp/bsp_sdcard.h` 中补充 `init_sd_card_spi()`、`sd_write_text_file()`、`sd_read_text_file()` 的函数声明。
4. 根据实际硬件确认 SDSPI 引脚 `MISO=13`、`MOSI=11`、`CLK=12`、`CS=10` 是否正确。
5. 如果 SD 卡模块也要遵循全局日志开关，将 `ESP_LOGx` 统一替换为 `MY_LOGx`。
6. 如果后续要支持串口 `AA` 任务帧，建议把串口接收任务改成“收包、校验、按帧头分发”的结构。

## 验证方式

建议执行以下验证：

1. 完整构建工程，确认日志宏迁移和 SDSPI 改造没有引入编译错误。
2. 上板运行，观察 `DEBUG=0` 时串口日志是否符合预期。
3. 临时设置 `DEBUG=1`，验证音频初始化、开机 MP3、语音识别、串口发送日志是否仍可输出。
4. 接入 SD 卡，调用 `init_sd_card_spi()`，确认 `/sdcard/test.txt` 可以写入并读回。
5. 通过语音命令触发 `usart_send_data()`，确认 `AA` 控制命令仍按原格式发送。
6. 发送合法和非法 `BB` 回包，确认接收任务的长度检查和异或校验行为不变。

## 结论

当前改动整体属于“日志输出收敛 + SD 卡示例迁移”。音频、语音、串口和 Wi-Fi 主业务路径没有明显行为改造，主要变化是日志是否输出受 `DEBUG` 控制。SD 卡模块变化较大，从 SDMMC 4 线模式切到 SDSPI 模式，并改变了读写接口语义，因此需要优先通过编译和硬件读写测试确认。
