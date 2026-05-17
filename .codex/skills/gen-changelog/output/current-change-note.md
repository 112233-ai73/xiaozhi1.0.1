# 语音控制与 MP3 播放改动说明

## 背景

本次改动围绕 ESP32-S3 离线语音控制链路展开，主要解决以下问题：

1. 语音识别状态流转不完整，唤醒、监听、命令识别和空闲回退之间缺少明确时序。
2. MP3 提示音需要接入三个业务节点：初始化完成播放 `107.mp3`、唤醒后播放 `105.mp3`、`WORKING -> IDLE` 播放 `106.mp3`。
3. 原 MP3 解码逻辑对跨块半帧和尾部填充数据兼容不足，容易在文件末尾报 `ESP_AUDIO_ERR_NOT_SUPPORT(-7)`。
4. UART 未在 `app_main()` 中初始化，同时发送日志使用大栈缓冲，存在 `uart driver error` 和任务栈风险。
5. 多命令词顺序与串口控制指令映射需要按当前 `command_word.c` 的命令编号重新对齐。

当前工作区相对 `HEAD` 涉及 8 个文件：

| 文件 | 主要改动 |
|---|---|
| `main/main.c` | 精简测试任务，增加 UART 初始化和开机提示音播放 |
| `main/audio/audio_mp3_decode.c` | 增加异步播放接口、播放互斥、MP3 半帧缓存和尾部容错 |
| `main/audio/audio_mp3_decode.h` | 暴露 `audio_mp3_play_file_async()`，引入信号量头文件 |
| `main/audio/audio_sr.c` | 重构唤醒/监听/识别状态机，接入唤醒提示音和超时回空闲 |
| `main/audio/audio_sr.h` | 引入 MP3 播放接口 |
| `main/audio/audio_sr_handler.c` | 修正 MN 状态判断，调整命令 ID 到 UART 指令映射 |
| `main/com/com_status.c` | 在 `WORKING -> IDLE` 状态变化时播放 `106.mp3` |
| `main/usart/usart_init.c` | 修复 UART 日志栈占用、长度校验和串口宏一致性 |

## 推荐方案：事件驱动的语音状态机 + 异步 MP3 播放

本次方案将提示音播放从同步测试函数中抽离出来，改为事件触发：

| 事件 | 触发位置 | 播放文件 |
|---|---|---|
| 初始化完成 | `app_main()` 调用 `app_sr_start()` 后 | `107.mp3` |
| 唤醒词通道确认 | `audio_detect_task()` 收到 `WAKENET_CHANNEL_VERIFIED` | `105.mp3` |
| 工作态回空闲态 | `com_status_change(WORKING -> IDLE)` | `106.mp3` |

MP3 播放采用 `audio_mp3_play_file_async()` 创建独立 `mp3_player_task`，避免阻塞语音识别任务。播放期间用 `s_playback_mutex` 保证同一时间只有一个 MP3 占用音频输出。

## 架构总览

### 启动链路

```text
app_main()
  -> usart_init()
  -> audio_init()
  -> app_sr_start()
      -> 创建 AFE / MultiNet / Ringbuffer / FreeRTOS tasks
  -> audio_mp3_play_file_async("107.mp3")
```

`usart_init()` 前移到启动阶段，确保语音命令识别后调用 `uart_write_bytes()` 时 UART 驱动已经安装。

### 语音识别链路

```text
audio_feed_task
  -> audio_read()
  -> afe_handle->feed()

audio_detect_task
  -> afe_handle->fetch()
  -> 处理 wakeup_state / vad_state
  -> send_afe_data_to_multinet()
  -> afe_rb_1

audio_multinet_task
  -> xRingbufferReceive(afe_rb_1)
  -> multinet->detect()
  -> xQueueSend(g_result_que)

sr_handler_task
  -> xQueueReceive(g_result_que)
  -> 按 command_id 调用 usart_send_data()
```

### MP3 播放链路

```text
audio_mp3_play_file_async(file)
  -> mount_storage_partition()
  -> 获取 s_playback_mutex
  -> xTaskCreate(mp3_player_task)

mp3_player_task
  -> fopen("/spiffs/<file>")
  -> mp3_decode_file()
      -> esp_audio_dec_open(ESP_AUDIO_TYPE_MP3)
      -> fread()
      -> esp_audio_dec_process()
      -> audio_write()
          -> esp_codec_dev_write()
  -> fclose()
  -> 释放 s_playback_mutex
```

## 详细设计

### 1. MP3 异步播放接口

`main/audio/audio_mp3_decode.c` 新增 `audio_mp3_play_file_async()`：

- 参数为空时返回 `ESP_ERR_INVALID_ARG`。
- 播放前调用 `mount_storage_partition()` 挂载 `mp3` SPIFFS 分区到 `/spiffs`。
- 首次调用时创建 `s_playback_mutex`。
- 如果已有 MP3 正在播放，打印 `MP3 playback busy, skip` 并返回 `ESP_ERR_INVALID_STATE`。
- 创建 `mp3_player_task` 进行实际播放，调用方不被阻塞。

影响：

| 项目 | 说明 |
|---|---|
| 同步方式 | 二值信号量 `s_playback_mutex` |
| 任务栈 | `mp3_player_task` 使用 `4 * 1024` |
| 播放策略 | 不打断当前播放，新请求 busy 时跳过 |

### 2. MP3 解码容错

`mp3_decode_file()` 增强了两类容错：

1. 跨读取块的半帧缓存  
   `ESP_AUDIO_ERR_DATA_LACK` 或 `ESP_AUDIO_ERR_CONTINUE` 时，保留未消费的 `raw` 数据到 `in_buf` 开头，并用 `cached_len` 记录长度。下一次 `fread()` 会从剩余数据后面继续填充，避免 MP3 帧被 2048 字节读取边界截断。

2. 尾部填充数据兼容  
   当前 `105.mp3`、`106.mp3`、`107.mp3` 文件尾部存在 `AA/55` 填充。若已经成功解码过音频，后续遇到 `ESP_AUDIO_ERR_NOT_SUPPORT` 会认为是尾部非 MP3 数据，返回 `ESP_AUDIO_ERR_OK`。

相关状态变量：

| 变量 | 作用 |
|---|---|
| `decoded_any` | 标记是否已经成功输出过 PCM，用于区分“文件开头格式错误”和“尾部填充错误” |
| `cached_len` | 记录跨块保留的未消费 MP3 数据长度 |
| `audio_info_logged` | 保证 MP3 格式信息只打印一次 |

### 3. 语音状态机

`audio_detect_task()` 从简单 VAD 转发改为明确状态流转：

```text
START / IDLE
  -> WAKENET_DETECTED：发送唤醒结果到 sr_handler_task
  -> WAKENET_CHANNEL_VERIFIED：进入 WORKING，播放 105.mp3

WORKING
  -> 等待 WAKE_WORKING_HOLD_MS
  -> VAD_SPEECH：进入 LISTENING

LISTENING
  -> VAD_SPEECH：持续送入 MultiNet
  -> 语音结束 SPEECH_END_TIMEOUT_MS：回到 WORKING

WORKING / LISTENING / SPEAKING
  -> NO_SPEECH_IDLE_TIMEOUT_MS 无语音：进入 IDLE
```

本次新增宏：

| 宏 | 值 | 说明 |
|---|---:|---|
| `WAKE_WORKING_HOLD_MS` | `500` | 唤醒后短暂保持工作态，避免立刻误入监听 |
| `NO_SPEECH_IDLE_TIMEOUT_MS` | `30000` | 长时间无语音后回到空闲 |
| `WAKE_ACK_MP3_FILE` | `105.mp3` | 唤醒确认提示音 |

`audio_multinet_task()` 在识别到命令后将状态改为 `SPEAKING`，再把结果写入 `g_result_que`。这样 `sr_handler_task` 处理串口发送期间，状态语义更贴近“正在响应命令”。

### 4. 提示音触发点

#### 初始化完成播放 `107.mp3`

`main/main.c` 在 `app_sr_start()` 成功后调用：

```c
esp_err_t ret = audio_mp3_play_file_async("107.mp3");
```

播放失败只打印 warning，不再用 `ESP_ERROR_CHECK` 触发重启。

#### 唤醒后播放 `105.mp3`

`audio_detect_task()` 在 `WAKENET_CHANNEL_VERIFIED` 后调用：

```c
audio_mp3_play_file_async(WAKE_ACK_MP3_FILE);
```

#### `WORKING -> IDLE` 播放 `106.mp3`

`com_status_change()` 记录旧状态：

```c
com_status_t previous_status = com_status;
```

在状态更新后判断：

```c
if (previous_status == WORKING && status == IDLE)
{
    audio_mp3_play_file_async(WORKING_TO_IDLE_MP3_FILE);
}
```

注意：只有直接 `WORKING -> IDLE` 会播放 `106.mp3`。如果状态从 `LISTENING` 或 `SPEAKING` 直接进入 `IDLE`，当前不会触发该提示音。

### 5. 唤醒词模型可观测性

`app_sr_start()` 增加唤醒模型空检查：

```c
ESP_RETURN_ON_FALSE(NULL != afe_config->wakenet_model_name, ESP_FAIL, TAG, "No wakenet model found");
```

并打印当前唤醒词：

```c
char *wake_words = esp_srmodel_get_wake_words(models, afe_config->wakenet_model_name);
```

这样烧录后可以从串口确认新唤醒词模型是否真的加载。

### 6. UART 初始化与发送稳定性

`main/main.c` 新增：

```c
usart_init();
```

解决 `uart_write_bytes()` 在 UART 驱动未安装时返回 `-1` 的问题。

`usart_send_data()` 做了三处加固：

| 改动 | 说明 |
|---|---|
| 长度校验 | `len < 3` 或 `len > UART_BUF_SIZE` 时拒绝发送 |
| 日志缓冲缩小 | 从 `UART_BUF_SIZE * 3 + 1` 降为 `USART_LOG_HEX_MAX_BYTES * 3 + 4` |
| 接收串口宏统一 | `uart_read_bytes(UART_NUM_0, ...)` 改为 `uart_read_bytes(UART_NUM, ...)` |

这降低了 `SR Handler Task` 4KB 栈上大数组造成栈溢出的风险。

### 7. 命令映射调整

`audio_sr_handler.c` 将 `ESP_MN_STATE_DETECTED & result.state` 修正为等值判断：

```c
if (ESP_MN_STATE_DETECTED == result.state)
```

命令 ID 到 UART 指令的映射重新按当前命令词顺序排列，例如：

| command_id | 当前发送指令 |
|---:|---|
| `0` ~ `10` | `Head_Zero` ~ `Head_Ten` |
| `11` ~ `21` | `Foot_Zero` ~ `Foot_Ten` |
| `22` ~ `27` | 头/脚/同步升降 |
| `30` ~ `33` | 灯光开关相关 |
| `34`、`36`、`38`、`40`、`42` | 模式类指令 |
| `45` ~ `58` | 护腰/加热相关 |
| `61` ~ `75` | 停止、按摩、放松时长等 |

需要注意：当前 `case 28/29/35/37/39/41/43/44/59/60` 等分支为空，属于已识别但不发送 UART 指令的命令。

## 可删除或已替换的代码

| 可删除/已替换内容 | 所在文件 | 规模 | 原因 |
|---|---|---:|---|
| `sr_result_test_task()` | `main/main.c` | `~30 行` | 原测试任务仅打印识别结果，已由正式 `sr_handler_task` 消费队列 |
| `sr_listening_test_task()` | `main/main.c` | `~20 行` | 原任务强制把 `WORKING` 推到 `LISTENING`，已被 `audio_detect_task()` 状态机替代 |
| `sr_test_command_name()` | `main/main.c` | `~10 行` | 测试打印逻辑删除后不再需要 |
| `COMMAND_RESULT_TIMEOUT_MS` | `main/audio/audio_sr.c` | `1 行` | 改为 `WAKE_WORKING_HOLD_MS` 和 `NO_SPEECH_IDLE_TIMEOUT_MS` 两个明确时序参数 |
| 在 `LISTENING` 内动态创建 `afe_rb_1` | `main/audio/audio_sr.c` | `~10 行` | `afe_rb_1` 已在 `app_sr_start()` 中统一创建，检测任务只负责投递数据 |

## 与当前方案的映射

| 旧实现 | 新实现 | 影响 |
|---|---|---|
| `app_main()` 创建测试识别任务 | `app_sr_start()` 创建正式识别任务，`sr_handler_task` 处理结果 | 启动流程更接近产品逻辑 |
| MP3 只能通过 `audio_mp3_decode_task()` 播默认文件 | `audio_mp3_play_file_async(file_name)` 可播放指定文件 | 支持按事件播放不同提示音 |
| 播放期间无互斥 | `s_playback_mutex` 串行化播放 | 避免多个 MP3 任务同时写 I2S |
| MP3 半帧数据直接丢弃 | `cached_len + memmove()` 保留未消费数据 | 降低跨块解码失败概率 |
| 尾部非 MP3 数据直接报错 | 已播放过音频后忽略尾部 `NOT_SUPPORT` | 兼容当前带填充的资源文件 |
| UART 发送前未显式初始化 | `app_main()` 先调用 `usart_init()` | 避免 UART driver error |
| UART 日志大数组在任务栈上 | 固定小日志缓冲 | 降低栈溢出风险 |

## 风险评估

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| MP3 busy 时提示音被跳过 | `107.mp3` 未播完时立刻唤醒 | `105.mp3` 可能不播放 | 上电后立即喊唤醒词，观察 `MP3 playback busy` |
| `WORKING -> IDLE` 限定过窄 | 从 `LISTENING/SPEAKING` 直接进入 `IDLE` | `106.mp3` 不播放 | 根据实际状态日志确认是否需要扩大为任意非空闲到 `IDLE` |
| 空分支命令不发送 UART | 识别到未映射命令 ID | 用户听到识别但床体无动作 | 对照 `command_word.c` 和协议表补全空 case |
| 唤醒词只在 `START/IDLE` 处理 | 当前处于 `WORKING/LISTENING/SPEAKING` | 重复喊唤醒词无响应 | 串口观察 `COM_STATUS`，确认产品是否需要工作态重复唤醒 |
| 未完成本地编译验证 | 当前 shell 未加载 ESP-IDF | 潜在编译问题需在 IDF 环境确认 | 在 ESP-IDF PowerShell 中运行 `idf.py build` |

## 迁移步骤

1. 确认 `main/asset` 中存在 `105.mp3`、`106.mp3`、`107.mp3`，文件名大小写与代码一致。
2. 在 ESP-IDF 环境中执行 `idf.py build`，确认 `esp_srmodel_get_wake_words()` 等符号可正常链接。
3. 烧录后观察启动日志，应出现 `load wakenet:` 和 `wake words:`。
4. 上电后确认初始化完成播放 `107.mp3`。
5. 在 `IDLE` 状态喊唤醒词，确认进入 `WORKING` 并播放 `105.mp3`。
6. 等待无语音超时，确认状态回到 `IDLE`，并根据实际状态流确认是否播放 `106.mp3`。
7. 对照命令词表逐条验证 `command_id` 与 UART 帧是否符合床控协议。

## 验证方式

建议使用以下串口日志作为验收依据：

```text
SR_TEST: speech recognition test start
AUDIO_SR: load wakenet:<model_name>
AUDIO_SR: wake words:<wake_words>
AUDIO_MP3_DECODE: start MP3 playback: /spiffs/107.mp3
AUDIO_MP3_DECODE: MP3 playback finished
COM_STATUS: status change: IDLE -> WORKING
AUDIO_MP3_DECODE: start MP3 playback: /spiffs/105.mp3
COM_STATUS: status change: WORKING -> IDLE
AUDIO_MP3_DECODE: start MP3 playback: /spiffs/106.mp3
```

如果看到：

```text
AUDIO_MP3_DECODE: ignore trailing unsupported MP3 data
```

说明 MP3 主体已成功播放，只是尾部填充数据被容错忽略，属于当前兼容逻辑的预期现象。

