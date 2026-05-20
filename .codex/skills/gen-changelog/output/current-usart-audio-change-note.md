# USART 接收事件化与音频默认音量调整说明

## 背景

本次改动主要围绕串口收发稳定性和音频启动体验做收敛：

- `main/usart/usart_init.c` 将 USART 接收从外部可创建的阻塞读取任务，改为 UART 驱动事件队列触发的内部任务。
- `main/usart/usart_init.h` 收敛对外接口，只保留初始化和发送接口，隐藏接收任务实现细节。
- `main/audio/audio_init.c` 将 ES8311 播放设备的初始输出音量从 `100` 调整为 `50`。

## 推荐方案：UART 事件队列驱动接收

当前方案由 `usart_init()` 统一完成 UART 驱动安装、事件队列绑定、事件任务创建和接收回调注册。接收数据时，ESP-IDF UART 驱动先投递 `UART_DATA` 事件，`usart_event_task()` 再调用内部注册的 `usart_receive_callback()` 解析帧。

这种方式把“串口初始化”和“串口接收调度”合并到 USART 模块内部，调用方不再需要额外创建 `usart_receive_task()`。

## 为什么选择这个方案

1. 降低调用方负担：外部只调用 `usart_init()` 即可完成收发能力初始化。
2. 利用 UART 驱动事件：接收任务通过 `xQueueReceive()` 等待 UART 事件，避免手写一个长期阻塞在 `uart_read_bytes(..., portMAX_DELAY)` 的公开任务。
3. 增加异常处理入口：新增 `UART_FIFO_OVF` 和 `UART_BUFFER_FULL` 分支，溢出时执行 `uart_flush_input()` 和 `xQueueReset()`。
4. 收敛模块边界：`usart_receive_task()` 不再暴露在头文件中，接收解析逻辑成为 USART 模块内部实现。

## 架构总览

### 改造前

```text
外部模块
  -> usart_init()
  -> 手动创建/调用 usart_receive_task()
       -> uart_read_bytes(..., portMAX_DELAY)
       -> 查找帧头
       -> 读取长度和剩余数据
       -> XOR 校验
```

### 改造后

```text
外部模块
  -> usart_init()
       -> uart_driver_install(..., USART_EVENT_QUEUE_SIZE, &s_uart_event_queue, ...)
       -> usart_register_rx_callback(usart_receive_callback)
       -> xTaskCreate(usart_event_task)

UART 驱动事件队列
  -> usart_event_task()
       -> UART_DATA
            -> usart_receive_callback()
                 -> uart_get_buffered_data_len()
                 -> uart_read_bytes()
                 -> 帧头/长度/XOR 校验
                 -> 打印完整接收包日志
       -> UART_FIFO_OVF / UART_BUFFER_FULL
            -> flush input
            -> reset queue
```

## 详细设计

### USART 初始化

`usart_init()` 现在负责创建 UART 事件队列和接收事件任务：

| 项目 | 当前实现 | 说明 |
|---|---|---|
| UART 号 | `UART_NUM_0` | 保持不变 |
| TX/RX 引脚 | `GPIO_NUM_43` / `GPIO_NUM_44` | 保持不变 |
| 波特率 | `115200` | 保持不变 |
| RX buffer | `UART_BUF_SIZE * 2` | 保持原驱动安装参数 |
| 事件队列长度 | `USART_EVENT_QUEUE_SIZE = 20` | 新增，用于接收 UART 驱动事件 |
| 事件任务栈 | `USART_EVENT_TASK_STACK_SIZE = 4 * 1024` | 新增 |
| 事件任务优先级 | `USART_EVENT_TASK_PRIORITY = 5` | 新增 |

涉及代码：

- `main/usart/usart_init.c:13`：定义事件队列长度。
- `main/usart/usart_init.c:141`：新增 `usart_event_task()`。
- `main/usart/usart_init.c:174`：`usart_init()` 内部创建 UART 事件任务。

### 接收回调机制

新增内部回调类型和注册函数：

| 名称 | 类型 | 作用 |
|---|---|---|
| `usart_rx_callback_t` | 函数指针类型 | 表示 UART 数据到达后的处理入口 |
| `s_rx_callback` | 静态全局变量 | 保存当前接收处理函数 |
| `usart_register_rx_callback()` | 静态函数 | 在初始化阶段绑定 `usart_receive_callback()` |

该回调目前只在 USART 模块内部使用，后续如果要支持不同业务解析器，可以把注册接口改为公开 API。

### 接收数据流

`usart_receive_callback()` 的接收解析流程如下：

1. 通过 `uart_get_buffered_data_len()` 查询 UART RX buffer 中是否还有数据。
2. 非阻塞读取 1 字节，查找 `FRAME_HEADER_CMD`。
3. 读取长度字节 `packet_len`。
4. 判断 `packet_len < 3` 时丢弃并记录警告。
5. 读取剩余 `packet_len - 2` 字节。
6. 对 `packet_len - 1` 字节做 XOR 校验。
7. 校验通过后打印命令字和完整报文十六进制日志。

当前状态流转可以概括为：

```text
UART_DATA event -> FIND_HEADER -> READ_LEN -> READ_BODY -> CHECKSUM -> LOG_PACKET
```

### 溢出处理

`usart_event_task()` 对 UART 接收异常新增处理：

| 事件 | 处理方式 | 影响 |
|---|---|---|
| `UART_FIFO_OVF` | `uart_flush_input()` + `xQueueReset()` | 丢弃当前输入，恢复接收队列 |
| `UART_BUFFER_FULL` | `uart_flush_input()` + `xQueueReset()` | 丢弃当前输入，避免持续满缓冲 |
| 其他事件 | 忽略 | 保持当前逻辑简单 |

### 发送逻辑保持兼容

`usart_send_data()` 对外函数名和参数未变，仍然根据 `data[1]` 作为报文长度，并调用 `uart_write_bytes()` 发送。

本次保留了发送前的空指针和长度检查，也保留了最多 `USART_LOG_HEX_MAX_BYTES` 字节的发送日志截断策略。

### 音频默认音量调整

`audio_es8311_init()` 中的初始播放音量从：

```c
esp_codec_dev_set_out_vol(play_dev_handle, 100)
```

调整为：

```c
esp_codec_dev_set_out_vol(play_dev_handle, 50)
```

该改动会降低设备启动后的默认播放音量，避免上电或首次播放时音量过大。运行时仍可通过 `inf_es8311_set_volume(int volume)` 动态调整。

## ESP32 侧代码改动

| 文件 | 改动规模 | 主要改动 | 影响 |
|---|---:|---|---|
| `main/audio/audio_init.c` | `~1 行` | ES8311 初始输出音量 `100 -> 50` | 降低默认播放响度 |
| `main/usart/usart_init.c` | `~216 行改动` | 新增 UART 事件队列、事件任务、接收回调、溢出处理、接收包日志 | USART 接收调度方式改变 |
| `main/usart/usart_init.h` | `~33 行改动` | 移除 `usart_receive_task()` 声明，精简注释和 include 顺序 | 外部不再直接依赖接收任务 |

## 可删除的代码

| 可删除项 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| `usart_receive_task()` 对外声明 | `main/usart/usart_init.h` | `~7 行` | 接收任务已改为 USART 模块内部创建，不再由外部创建 |
| 旧的公开阻塞接收任务实现 | `main/usart/usart_init.c` | `~50 行` | 被 `usart_event_task()` + `usart_receive_callback()` 替代 |
| UART 驱动安装时不创建事件队列的旧逻辑 | `main/usart/usart_init.c` | `~5 行` | 当前需要 `s_uart_event_queue` 接收 UART 驱动事件 |

## 与当前方案的映射

| 原实现 | 新实现 | 行为变化 |
|---|---|---|
| `uart_driver_install(..., 0, NULL, 0)` | `uart_driver_install(..., USART_EVENT_QUEUE_SIZE, &s_uart_event_queue, 0)` | UART 驱动开始向队列投递事件 |
| 外部调用/创建 `usart_receive_task()` | `usart_init()` 内部 `xTaskCreate(usart_event_task, ...)` | 接收任务生命周期由 USART 模块管理 |
| 接收任务直接阻塞读取串口 | 事件任务等待 `UART_DATA` 后读取 RX buffer | 接收触发方式由读阻塞改为事件触发 |
| 校验通过后仅打印命令 | 校验通过后打印命令和完整报文 | 调试信息更完整 |
| 未处理 RX 溢出事件 | `UART_FIFO_OVF` / `UART_BUFFER_FULL` 时 flush 和 reset queue | 异常恢复路径更明确 |

## 资源与风险评估

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| 接收包长度没有限制到 `UART_BUF_SIZE` | 外部发送异常长度字节 | 当前 `packet_len` 是 `uint8_t`，最大 255，小于 `UART_BUF_SIZE=1024`，风险较低；若未来扩大长度字段需补充上限检查 | 构造长度为 `0`、`2`、`255` 的报文测试 |
| `UART_DATA` 事件中连续读取 buffer | 短时间内连续多帧输入 | 回调会循环处理当前 buffered 数据，可能占用事件任务时间 | 连续发送多帧合法报文并观察任务响应 |
| 溢出处理会丢弃当前输入 | RX FIFO 或 buffer 满 | 当前未完成报文会被丢弃 | 压力发送大流量数据，确认后续合法帧可恢复接收 |
| `usart_receive_task()` 接口移除 | 项目其他文件仍引用该函数 | 编译失败 | 全局搜索 `usart_receive_task`，并执行构建 |
| 默认音量降低 | 用户期望启动即最大音量 | 启动提示音或播放内容变小 | 上电后播放音频，确认默认音量符合产品预期 |

## 迁移步骤

1. 保持外部模块只调用 `usart_init()` 初始化串口。
2. 删除或停止创建 `usart_receive_task()` 的外部代码。
3. 若业务需要处理接收到的命令，在 `usart_receive_callback()` 校验通过分支中接入命令解析。
4. 若需要支持外部注册接收处理函数，将 `usart_register_rx_callback()` 从 `static` 改为公开接口，并在头文件声明。
5. 根据实际喇叭响度确认 `esp_codec_dev_set_out_vol(..., 50)` 是否为最终默认值。

## 验证方式

### 编译验证

建议执行：

```powershell
idf.py build
```

重点确认：

- 不存在 `usart_receive_task` 未定义或未声明错误。
- `freertos/queue.h`、`freertos/task.h` 引入后编译通过。
- `xTaskCreate()` 返回值检查无类型告警。

### USART 功能验证

1. 发送合法报文：`FRAME_HEADER_CMD`、长度、命令、数据、XOR 校验位。
2. 观察日志中出现 `checksum passed, command=0xXX`。
3. 观察日志中出现 `USART recv command packet len=... data=[...]`。
4. 发送错误校验位报文，确认日志出现 `checksum failed`。
5. 发送非法长度报文，确认日志出现 `invalid packet length`。
6. 连续快速发送多帧报文，确认不会只处理第一帧。

### 音频验证

1. 上电初始化音频，确认 ES8311 正常打开。
2. 播放默认提示音或 MP3，确认初始音量比原先更低。
3. 调用 `inf_es8311_set_volume()` 设置其他音量值，确认运行时调节仍正常。

## 当前结论

本次改动将 USART 接收职责集中到 `usart_init()` 内部，使用 UART 事件队列驱动接收解析，并增加 RX 溢出恢复处理；对外接口更简洁，调试日志更完整。音频侧仅调整 ES8311 默认输出音量，属于启动体验参数变更。

后续建议优先补充接收报文的业务分发逻辑，并执行一次完整构建与串口实机收发验证。
