# 语音识别状态机重构说明

## 背景

本次改动围绕 ESP32-S3 端语音识别链路展开，目标是让 `audio_sr` 模块从原来的 WakeNet + Ringbuffer + MultiNet 多任务链路，收敛为更直接的 AFE VAD + MultiNet 命令词识别流程。

当前实现中，AFE 负责音频前端处理和 VAD 人声检测；检测到人声后系统进入 `LISTENING` 状态，并直接将 AFE 输出音频送入 MultiNet；当 1 秒内没有继续检测到人声，系统回到 `IDLE`。MP3 播放完成后也会回切到 `IDLE`，避免系统停留在 `SPEAKING` 状态。

## 推荐方案：VAD 驱动的命令词识别流程

新的识别流程不再依赖 WakeNet 唤醒词模型，也不再通过 ringbuffer 把 AFE 数据转发给独立 MultiNet 任务。核心路径如下：

```text
audio_feed_task
  -> audio_read()
  -> afe_handle->feed()

audio_detect_task
  -> afe_handle->fetch()
  -> VAD_SPEECH ? IDLE -> LISTENING
  -> LISTENING 时调用 multinet->detect()
  -> ESP_MN_STATE_DETECTED / ESP_MN_STATE_TIMEOUT
  -> xQueueSend(g_result_que)

sr_handler_task
  -> 收到命令结果
  -> 执行串口控制和 MP3 播放

mp3_player_task
  -> MP3 正常播放完成
  -> com_status_change(IDLE)
```

## 为什么选择这个方案

| 原问题 | 新方案 | 影响 |
|---|---|---|
| WakeNet 模型未加载时仍保留唤醒词状态逻辑 | 移除 WakeNet 依赖，直接使用 VAD 触发监听 | 启动链路更清晰，减少无效状态 |
| AFE 数据通过 ringbuffer 再给 MultiNet | `audio_detect_task` 内直接调用 `multinet->detect()` | 少一个任务和两个 ringbuffer，数据路径更短 |
| `audio_detect_task` 内状态判断、识别、队列发送混在一起 | 拆分为 `handle_vad_state()`、`handle_multinet_detect()` 等函数 | 便于定位 VAD、MultiNet、状态切换问题 |
| MP3 播放完后状态没有统一回切 | `mp3_player_task()` 正常结束时调用 `com_status_change(IDLE)` | 避免命令播报结束后仍处于 `SPEAKING` |

## 架构总览

### 任务划分

| 任务 | 所在文件 | 职责 |
|---|---|---|
| `audio_feed_task` | `main/audio/audio_sr.c` | 从 codec 读取 PCM，并送入 AFE |
| `audio_detect_task` | `main/audio/audio_sr.c` | 从 AFE 拉取增强后音频，处理 VAD 状态，并驱动 MultiNet |
| `sr_handler_task` | `main/audio/audio_sr_handler.c` | 消费识别结果，执行控制命令和提示音播放 |
| `mp3_player_task` | `main/audio/audio_mp3_decode.c` | 解码和播放 MP3，播放完成后回到 `IDLE` |

### 状态流转

```text
START
  -> app_sr_start()
  -> IDLE

IDLE
  -> AFE VAD 检测到 VAD_SPEECH
  -> LISTENING

LISTENING
  -> 继续检测人声
  -> multinet->detect()

LISTENING
  -> 1 秒没有检测到人声
  -> IDLE

LISTENING
  -> MultiNet 识别成功
  -> sr_handler_task 处理命令
  -> SPEAKING

SPEAKING
  -> MP3 正常播放完成
  -> IDLE
```

## 详细设计

### `audio_sr.c` 识别流程重构

`main/audio/audio_sr.c` 从原来的长函数状态机，拆成多个小函数：

| 函数 | 作用 |
|---|---|
| `clean_multinet()` | 封装 `multinet->clean(model_data)`，避免空指针调用 |
| `switch_to_idle()` | 切换到 `IDLE` 并清理 MultiNet 状态 |
| `switch_to_listening()` | 切换到 `LISTENING`，进入识别窗口 |
| `send_sr_result()` | 统一封装识别结果入队 |
| `log_fetch_result()` | 打印 VAD 状态和采样值，用于调试采集链路 |
| `configure_afe()` | 集中配置 AFE 输入、VAD、WakeNet、内存策略 |
| `load_multinet_model()` | 加载中文 MultiNet 模型 |
| `load_speech_commands()` | 注册并更新命令词 |
| `handle_vad_state()` | 基于 `res->vad_state` 控制 `IDLE/LISTENING` |
| `handle_multinet_detect()` | 处理 MultiNet 的 detecting、timeout、detected 三类结果 |

关键参数：

```c
#define VAD_IDLE_TIMEOUT_MS 1000
#define MULTINET_TIMEOUT_MS 5760
```

`VAD_IDLE_TIMEOUT_MS` 表示 `LISTENING` 状态下 1 秒未检测到人声则回到 `IDLE`。`MULTINET_TIMEOUT_MS` 保留原 MultiNet 识别超时时间 5760 ms。

### AFE 配置调整

`configure_afe()` 当前配置为：

```c
afe_config->pcm_config.mic_num = 2;
afe_config->pcm_config.total_ch_num = 2;
afe_config->se_init = false;
afe_config->aec_init = true;
afe_config->wakenet_init = false;
afe_config->wakenet_model_name = NULL;
afe_config->vad_init = true;
afe_config->vad_mode = VAD_MODE_0;
afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
```

含义：

| 配置 | 说明 |
|---|---|
| `mic_num = 2` / `total_ch_num = 2` | 与 ES7210 双麦输入保持一致 |
| `se_init = false` | 关闭双麦盲源分离，避免 BSS 对识别链路产生额外影响 |
| `aec_init = true` | 保留 AEC 配置，但当前日志中若无参考通道，AFE 会自动禁用 AEC |
| `wakenet_init = false` | 不加载唤醒词模型 |
| `vad_init = true` | 使用 AFE VAD 驱动监听状态 |

### MP3 播放完成状态回切

`main/audio/audio_mp3_decode.c` 中，`mp3_player_task()` 在 MP3 正常播放完成时新增：

```c
com_status_change(IDLE);
```

位置在：

```c
} else {
    ESP_LOGI(TAG, "MP3 playback finished");
    com_status_change(IDLE);
}
```

这表示只有 MP3 解码播放正常结束，才回到 `IDLE`。如果播放错误，仍保留错误日志，不把错误伪装成正常完成。

### 音频输入配置调整

`main/audio/audio_init.c` 中 ES7210 输入从单麦改为双麦：

```c
.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2
```

输入增益从 `20.0` 调整为 `30.0`：

```c
esp_codec_dev_set_in_gain(record_dev_handle, 30.0)
```

这与 `audio_sr.c` 中 AFE 双麦配置保持一致。

## 代码侧变更

| 文件 | 主要变更 | 影响 |
|---|---|---|
| `main/audio/audio_sr.c` | 重构识别状态机，移除 WakeNet/ringbuffer/MultiNet 独立任务，新增 VAD 驱动监听 | 识别路径更短，状态逻辑更集中 |
| `main/audio/audio_sr.h` | 移除 WakeNet 和 ringbuffer 相关 include，删除 `sr_result_t.wakenet_mode` | 结果结构只保留 MultiNet 识别结果 |
| `main/audio/audio_sr_handler.c` | 移除 WakeNet 结果分支和 wakeword 日志 | handler 只处理 timeout 和 detected |
| `main/audio/audio_mp3_decode.c` | MP3 正常播放结束后切回 `IDLE` | 播报完成后恢复识别空闲状态 |
| `main/audio/audio_init.c` | ES7210 改为 MIC1 + MIC2，输入增益提高 | 与 AFE 双麦输入配置对齐 |

## 可删除的代码

| 可删除项 | 所在文件 | 规模 | 原因 |
|---|---|---:|---|
| `send_afe_data_to_multinet()` | `main/audio/audio_sr.c` | ~13 行 | MultiNet 直接消费 `res->data`，不再需要 ringbuffer 转发 |
| `audio_multinet_task()` | `main/audio/audio_sr.c` | ~80 行 | 独立 MultiNet 任务被 `handle_multinet_detect()` 替代 |
| `afe_rb_1` / `afe_rb_2` | `main/audio/audio_sr.c` | ~2 个全局变量 | 不再使用 ringbuffer |
| `AFE_RINGBUF_FRAME_NUM` | `main/audio/audio_sr.c` | 1 个宏 | ringbuffer 被移除 |
| `SPEECH_END_TIMEOUT_MS` / `WAKE_WORKING_HOLD_MS` / `NO_SPEECH_IDLE_TIMEOUT_MS` | `main/audio/audio_sr.c` | 3 个宏 | WakeNet/WORKING 过渡状态被移除 |
| `sr_result_t.wakenet_mode` | `main/audio/audio_sr.h` | 1 个字段 | handler 不再处理 WakeNet 结果 |
| WakeNet handler 分支 | `main/audio/audio_sr_handler.c` | ~8 行 | 当前不再发送 WakeNet 事件 |

## 与当前方案的映射

| 旧方案 | 新方案 | 说明 |
|---|---|---|
| WakeNet 检测后进入 `WORKING/LISTENING` | AFE VAD 检测到 `VAD_SPEECH` 后进入 `LISTENING` | 不再依赖唤醒词模型 |
| AFE fetch 后写入 ringbuffer | AFE fetch 后直接调用 MultiNet | 降低任务间通信复杂度 |
| `audio_multinet_task()` 独立识别 | `handle_multinet_detect()` 在 detect task 内识别 | 保持识别结果入队行为 |
| `sr_result_t` 带 `wakenet_mode` | `sr_result_t` 只包含 `state/command_id/confidence` | 结果结构更贴近当前功能 |
| 命令播报后状态可能停留在 `SPEAKING` | MP3 正常播放完成后切 `IDLE` | 状态闭环更明确 |

## 风险评估

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| VAD 长时间保持 `VAD_SPEECH` | 环境噪声、增益过高、VAD 模式较宽松 | 系统持续进入 `LISTENING`，MultiNet 周期性 timeout | 观察 `VAD: %d, sample: %d` 日志，必要时调高 `vad_mode` 或降低输入增益 |
| MultiNet 命令仍不识别 | 命令词发音与拼音表不匹配，或 AFE 输出质量不佳 | 只出现 `Time out`，不出现 `ESP_MN_STATE_DETECTED` | 先测试短命令，如 `ting zhi`，再测试长命令 |
| MP3 完成后直接切 `IDLE` | 播报结束时仍有其他业务状态需要保持 | 可能提前恢复监听 | 若后续增加执行中状态，需要区分提示音类型 |
| `aec_init = true` 但无参考通道 | AFE input format 没有 `R` 通道 | AFE 日志提示自动禁用 AEC | 若需要回声消除，应补充 playback reference channel |
| `log_fetch_result()` 每帧打印 | 运行时保持 INFO 日志 | 日志量很大，可能影响实时性 | 调试完成后改为状态变化打印或降级为 `ESP_LOGD` |

## 迁移步骤

1. 确认硬件麦克风数量与 `audio_init.c`、`audio_sr.c` 中的双麦配置一致。
2. 烧录后查看 AFE 启动日志，确认输入为 `total 2 channels(2 microphone, 0 playback)`。
3. 观察 `VAD: %d, sample: %d` 日志，确认说话时采样值有明显变化。
4. 说出短命令，例如 `ting zhi`，确认是否出现 `Deteted command` 日志。
5. 确认 `sr_handler_task` 收到 `ESP_MN_STATE_DETECTED` 后会进入 `SPEAKING` 并播放对应 MP3。
6. 确认 MP3 播放完成后出现 `MP3 playback finished`，随后状态切回 `IDLE`。
7. 调试完成后建议降低 `log_fetch_result()` 日志频率。

## 验证方式

### 编译验证

建议在 ESP-IDF 环境中执行：

```powershell
idf.py build
```

当前 Codex 会话中的 PowerShell 未加载 `idf.py`，因此本次文档生成未完成本地编译验证。

### 运行验证

重点观察以下日志：

```text
AUDIO_SR: VAD: <state>, sample: <value>
AUDIO_SR: voice detected, enter listening
AUDIO_SR: Deteted command : <id>
audio_sr_handler: mn detected
AUDIO_MP3_DECODE: MP3 playback finished
COM_STATUS: status change: SPEAKING -> IDLE
```

### 回归检查

| 场景 | 预期 |
|---|---|
| 静音环境 | 系统保持 `IDLE`，不频繁进入 `LISTENING` |
| 说命令词 | 进入 `LISTENING` 并尝试 MultiNet 识别 |
| 命令识别成功 | handler 执行对应串口命令并播放 MP3 |
| MP3 播放中 | SR detect task 跳过识别 |
| MP3 正常结束 | `com_status_change(IDLE)` |
| 识别超时 | 发送 timeout 结果并回到 `IDLE` |

## 后续建议

1. 将 `log_fetch_result()` 改为条件日志，避免每帧打印影响实时性。
2. 根据实测环境调整 `VAD_MODE_0` 到 `VAD_MODE_2/3`，降低环境噪声误触发。
3. 如果后续仍需要唤醒词，再单独恢复 WakeNet 分支，而不是混入当前 VAD 命令词链路。
4. 若需要 AEC，应把 AFE input format 调整为包含 `R` 的参考通道格式，并确保播放参考数据实际可用。
