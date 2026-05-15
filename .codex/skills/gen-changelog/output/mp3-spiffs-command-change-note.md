# MP3 SPIFFS 播放与语音命令词变更说明

## 背景

本次工作区相对 `HEAD` 的改动主要集中在两个方向：

1. 新增基于 SPIFFS 的 MP3 文件解码播放能力。
2. 调整语音命令词 `cmd_phoneme` 的顺序和覆盖范围。

当前变更包含：

| 类型 | 文件 | 说明 |
|---|---|---|
| 修改 | `main/CMakeLists.txt` | 将 `audio/audio_mp3_decode.c` 加入编译，并为 `mp3` 分区注册 SPIFFS 镜像 |
| 修改 | `main/com/command_word.c` | 重排并扩展 MultiNet 命令词列表 |
| 新增 | `main/audio/audio_mp3_decode.c` | 新增 SPIFFS 挂载、MP3 解码和 PCM 写入播放逻辑 |
| 新增 | `main/audio/audio_mp3_decode.h` | 新增 MP3 解码播放模块对外声明 |

另外，`main/asset` 目录中已有 `101.mp3` 到 `211.mp3` 共 111 个 MP3 文件，总大小约 636 KB。该目录当前已经被 Git 跟踪，不属于本次 diff 新增内容。

## 推荐方案：以 `mp3` SPIFFS 分区承载提示音资源

本次改动的目标可以理解为：将 MP3 提示音作为独立 SPIFFS 镜像随固件烧录，并在设备运行时通过 `esp_audio_dec` 解码后写入已有音频播放链路。

构建侧通过：

```cmake
spiffs_create_partition_image(mp3 ./asset FLASH_IN_PROJECT)
```

将 `main/asset` 打包到分区表中的 `mp3` 分区。运行侧通过：

```c
esp_vfs_spiffs_register(&conf);
fopen("/spiffs/<file>", "rb");
esp_audio_dec_process(...);
audio_write(...);
```

完成从 Flash 文件系统到扬声器输出的路径。

## 为什么选择这个方案

| 方案 | 优点 | 代价 |
|---|---|---|
| SPIFFS 分区存放 MP3 | 资源独立于代码，便于替换提示音；不占用 `.rodata`；适合较多小文件 | 需要维护分区容量、镜像路径和运行时文件名 |
| `EMBED_FILES` 嵌入二进制 | 构建简单，运行时不需要挂载文件系统 | 多文件管理不便，资源直接进入应用镜像 |
| 外部存储读取 | 容量弹性更大 | 增加硬件和挂载失败场景 |

当前项目已有 `partitions.csv` 中的 `mp3` 分区：

```text
mp3, data, spiffs, 0xef0000, 0x100000
```

因此使用 SPIFFS 承载 MP3 资源与现有分区设计是匹配的。

## 架构总览

```text
main/asset/*.mp3
        |
        | 构建期 spiffs_create_partition_image
        v
mp3 SPIFFS 分区
        |
        | 运行期 esp_vfs_spiffs_register
        v
/spiffs/<file>.mp3
        |
        | fread + esp_audio_dec_process
        v
PCM 数据
        |
        | audio_write
        v
ES8311 / I2S 播放链路
```

语音识别侧的数据流保持原有结构：

```text
audio_read -> AFE feed/fetch -> Ringbuffer -> MultiNet detect -> result queue -> sr_handler_task
```

但命令词编号发生了明显变化，需要同步检查 `sr_handler_task` 的 `switch(command_id)` 映射。

## 详细设计

### 构建侧：注册 MP3 SPIFFS 镜像

`main/CMakeLists.txt` 新增：

| 位置 | 改动 | 作用 |
|---|---|---|
| `main/CMakeLists.txt:6` | 加入 `audio/audio_mp3_decode.c` | 让 MP3 解码模块参与编译 |
| `main/CMakeLists.txt:14` | 调用 `spiffs_create_partition_image(mp3 ./asset FLASH_IN_PROJECT)` | 将 `main/asset` 打包并烧录到 `mp3` 分区 |

注意：`./asset` 在 `main/CMakeLists.txt` 中按组件目录解析，对应当前项目的 `main/asset`。如果改成 `../asset`，则会指向项目根目录下的 `asset`，这会导致 `spiffsgen.py` 找不到目录或打包错误目录。

### 播放侧：SPIFFS 挂载

`mount_storage_partition()` 负责挂载 `mp3` 分区：

| 字段 | 当前值 | 说明 |
|---|---|---|
| `base_path` | `/spiffs` | 文件访问路径前缀 |
| `partition_label` | `mp3` | 对应分区表中的 `mp3` |
| `max_files` | `5` | 最多同时打开文件数 |
| `format_if_mount_failed` | `false` | 挂载失败时不自动格式化，避免误清资源 |

挂载成功后会通过 `esp_spiffs_info()` 打印分区总容量和已用容量。

### 播放侧：MP3 解码与输出

`mp3_decode_file()` 使用 `esp_audio_dec` 默认解码器注册机制：

1. 调用 `esp_audio_dec_register_default()` 注册默认解码器。
2. 使用 `ESP_AUDIO_TYPE_MP3` 创建解码器。
3. 以 1024 字节读取 MP3 输入流。
4. 通过 `esp_audio_dec_process()` 解码为 PCM。
5. 当输出缓冲不足时按 `out_frame.needed_size` 扩容。
6. 将 `out_frame.decoded_size` 写入 `audio_write()`。

该实现复用了现有 `audio_init.c` 中的播放设备句柄，输出格式当前按 ES8311 初始化配置为 16 kHz、16-bit、mono。

### 任务侧：播放入口

新增两个入口：

| 函数 | 作用 |
|---|---|
| `mp3_player_task(void *pvParameters)` | 按文件名打开 `/spiffs/<file>`，播放完成后删除当前 FreeRTOS task |
| `audio_mp3_decode_task(void)` | 挂载 SPIFFS 后直接调用 `mp3_player_task(DEFAULT_MP3_FILE)` |

当前 `audio_mp3_decode_task()` 不是标准 FreeRTOS task 函数签名，也没有在 `app_main()` 或语音处理流程中被调用。因此本次新增的是播放能力模块，尚未接入业务触发点。

### 命令词侧：重排和扩展 `cmd_phoneme`

`main/com/command_word.c` 将命令词数组从原来的模式/升降/按摩顺序，调整为：

1. 头部档位：`tou bu ling dang` 到 `tou bu shi dang`
2. 脚部档位：`jiao bu ling dang` 到 `jiao bu shi dang`
3. 升降、音量、灯光、模式、护腰、恒温、放松等命令

新增或恢复的词包括：

| 类别 | 示例 |
|---|---|
| 音量 | `yin liang jia`、`yin liang jian` |
| 灯光 | `deng da kai`、`deng guan bi` |
| 记忆模式 | `ji yi ping tang mo shi`、`ji yi xiu xian mo shi` 等 |
| 唤醒模式 | `jiao xing mo shi` |
| 温度 | `wen du sheng gao`、`wen du jiang di` |
| 显式打开按摩 | `da kai tou bu fang song`、`da kai jiao bu fang song` |

`audio_sr.c` 会按数组下标调用 `esp_mn_commands_add(i, cmd_phoneme[i])`，所以数组顺序就是识别结果 `command_id` 的来源。

## ESP32 侧代码影响

### 资源和分区

| 项目 | 当前实现 | 影响 |
|---|---|---|
| SPIFFS 分区 | `mp3`，大小 `0x100000` | 可容纳当前约 636 KB 的 MP3 资源 |
| 构建镜像路径 | `main/asset` | 与当前资源目录一致 |
| 运行挂载路径 | `/spiffs` | 播放时使用 `/spiffs/<file>` |

### 内存和解码

| 项目 | 当前值 | 影响 |
|---|---|---|
| 输入缓冲 | `1024` 字节 | 分块读取 MP3 文件 |
| 初始输出缓冲 | `4096` 字节 | 不足时会 `realloc` 到解码器要求大小 |
| 解码器注册 | 每次播放调用 `esp_audio_dec_register_default()` | 已处理 `ESP_AUDIO_ERR_ALREADY_EXIST`，可重复调用 |
| 输出链路 | `audio_write()` | 依赖 `audio_init()` 已经初始化 ES8311 播放设备 |

### 语音命令映射

这是本次变更中风险最高的部分。`cmd_phoneme` 顺序已变化，但 `audio_sr_handler.c` 中的 `switch(result.command_id)` 仍按旧编号发送串口命令。

例如，当前 `cmd_phoneme[0]` 已变为：

```text
tou bu ling dang
```

但 `sr_handler_task()` 中 `case 0` 仍发送：

```c
usart_send_data(Sleep_Mode);
```

这意味着用户说“头部零档”时，可能会被映射成“睡眠模式”的串口指令。除非后续计划同步重排 `audio_sr_handler.c`，否则该问题会导致控制行为错位。

## 可删除的代码

本次变更没有明确替换掉一段旧播放实现，因此暂不建议直接删除业务代码。

| 可删除 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| 暂无 | - | `0 行` | 新增 MP3 播放能力尚未接入主流程，也未替代现有控制逻辑 |

## 吞吐量、资源与风险评估

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| 默认文件名不存在 | `DEFAULT_MP3_FILE` 为 `test.mp3`，但 `main/asset` 当前是 `101.mp3` 到 `211.mp3` | 运行时 `fopen("/spiffs/test.mp3")` 失败 | 将默认文件名改为真实文件，或按命令动态传入文件名 |
| 命令词编号错位 | `cmd_phoneme` 重排后未同步 `audio_sr_handler.c` | 识别结果触发错误串口指令 | 建立命令词与串口指令的统一映射表并逐项验证 |
| 播放入口未接入 | `audio_mp3_decode_task()` 未被调用 | MP3 播放功能不会在运行时触发 | 在语音结果处理或测试入口中创建播放任务 |
| 解码输出格式不匹配 | MP3 文件不是 16 kHz、16-bit、mono | 播放速度、声道或音质异常 | 用日志检查 `esp_audio_dec_get_info()` 输出，必要时增加重采样/声道转换 |
| 与语音识别争用音频外设 | 播放和录音同时使用同一 I2S/codec 链路 | 可能影响唤醒和命令识别 | 明确播放期间是否暂停识别，或设计音频状态机 |
| SPIFFS 镜像路径误改 | `./asset` 被改成不存在目录 | 构建期 `spiffsgen.py` 失败 | 构建前确认 `main/asset` 存在且分区容量足够 |

## 与当前方案的映射

| 当前实现 | 新增方案 | 说明 |
|---|---|---|
| `audio_init()` 初始化 ES8311 播放设备 | `audio_write()` 输出解码 PCM | MP3 播放复用现有播放链路 |
| `partitions.csv` 中 `mp3` 分区 | `spiffs_create_partition_image(mp3 ./asset FLASH_IN_PROJECT)` | 构建时生成并烧录资源镜像 |
| `cmd_phoneme[]` 作为 MultiNet 命令表 | 扩展并重排命令词 | 识别 ID 会随数组下标变化 |
| `sr_handler_task()` 按 `command_id` 发送串口 | 尚未同步新命令顺序 | 需要重点迁移，否则控制会错位 |

## 迁移步骤

1. 确认 `main/asset` 是唯一的 MP3 资源目录，并保持 `spiffs_create_partition_image(mp3 ./asset FLASH_IN_PROJECT)` 与目录位置一致。
2. 确认默认播放文件名，避免 `DEFAULT_MP3_FILE` 指向不存在的 `test.mp3`。
3. 为 `cmd_phoneme` 和 `audio_sr_handler.c` 建立一份一一对应表，按新的数组顺序重排 `switch(command_id)` 或改成结构化映射。
4. 决定 MP3 播放触发点：测试入口、唤醒提示、命令确认提示，或串口控制成功后的反馈音。
5. 如果播放期间需要避免麦克风误识别，增加音频状态控制，例如 `IDLE -> PLAYING -> WORKING -> LISTENING`。
6. 对 `101.mp3` 到 `211.mp3` 做格式抽检，确认是否全部符合 16 kHz、16-bit、mono 的播放链路假设。

## 验证方式

### 构建验证

```powershell
idf.py build
```

重点确认：

1. `spiffs_mp3_bin` 目标可以成功生成。
2. `build/mp3.bin` 存在。
3. 构建日志中不再出现 `Unknown CMake command` 或 `given base directory does not exist`。

### 分区验证

烧录后检查日志：

```text
mounting SPIFFS partition...
SPIFFS size: total=..., used=...
```

如果出现 `failed to find SPIFFS partition: mp3`，需要检查分区表是否实际烧录。

### 播放验证

建议先用一个真实存在的文件名进行测试，例如：

```c
mp3_player_task((void *)"101.mp3");
```

预期日志：

```text
start MP3 playback: /spiffs/101.mp3
MP3 info: ...
MP3 playback finished
```

### 命令映射验证

逐条测试 `cmd_phoneme` 中前 10 个命令，确认识别出的 `command_id` 与实际串口发送的数据一致。当前尤其需要关注 `command_id=0` 到 `command_id=21`，因为这部分已经从旧的模式命令变成了头部/脚部档位命令。

## 当前结论

本次改动已经具备“构建期打包 MP3 资源”和“运行期解码播放 MP3”的基础能力，但业务闭环尚未完成。后续最优先处理的是两个点：

1. 修正或动态化默认播放文件名，保证 SPIFFS 中存在目标 MP3。
2. 同步 `cmd_phoneme` 与 `sr_handler_task()` 的命令编号映射，避免语音识别结果触发错误控制指令。
