# station_example_main.c 代码说明文档

## 背景

`station_example_main.c` 是 ESP-IDF `examples/wifi/getting_started/station` 示例的主入口文件，用于演示 ESP32 以 Wi-Fi Station 模式连接路由器 AP 的完整流程。

该文件重点展示了以下能力：

| 功能点 | 说明 |
|---|---|
| NVS 初始化 | Wi-Fi 驱动依赖 NVS 保存校准数据和配置，启动前必须初始化 |
| Wi-Fi STA 初始化 | 初始化 `esp_netif`、默认事件循环、Wi-Fi 驱动和 STA 网络接口 |
| 事件驱动连接 | 通过 `WIFI_EVENT` 和 `IP_EVENT` 响应启动、断连、获取 IP 等事件 |
| 自动重连 | 断开连接后按 `CONFIG_ESP_MAXIMUM_RETRY` 限制进行重试 |
| 同步等待结果 | 用 FreeRTOS Event Group 等待连接成功或连接失败 |

源码位置：

`D:\Development\esp_idf_5.5.3\Espressif\frameworks\esp-idf-v5.5.3\examples\wifi\getting_started\station\main\station_example_main.c`

## 总体流程

程序启动后执行路径如下：

```text
app_main()
  -> nvs_flash_init()
  -> 可选设置 Wi-Fi 日志级别
  -> wifi_init_sta()
       -> 创建事件组
       -> 初始化 esp_netif
       -> 创建默认事件循环
       -> 创建默认 Wi-Fi STA 网络接口
       -> 初始化 Wi-Fi 驱动
       -> 注册 Wi-Fi / IP 事件回调
       -> 填充 wifi_config_t
       -> 设置 Wi-Fi 为 STA 模式
       -> 写入 STA 配置
       -> 启动 Wi-Fi
       -> 等待连接成功或失败事件位
```

连接状态流转可以理解为：

```text
启动
  -> WIFI_EVENT_STA_START
  -> esp_wifi_connect()
  -> 连接 AP 中
      -> IP_EVENT_STA_GOT_IP
          -> WIFI_CONNECTED_BIT
          -> 连接成功
      -> WIFI_EVENT_STA_DISCONNECTED
          -> 未超过最大重试次数
              -> esp_wifi_connect()
              -> 继续重试
          -> 超过最大重试次数
              -> WIFI_FAIL_BIT
              -> 连接失败
```

## 依赖头文件

| 头文件 | 作用 |
|---|---|
| `freertos/FreeRTOS.h` | FreeRTOS 基础定义 |
| `freertos/task.h` | FreeRTOS 任务相关接口，本示例未直接创建任务 |
| `freertos/event_groups.h` | 使用事件组同步 Wi-Fi 连接结果 |
| `esp_system.h` | ESP 系统基础接口 |
| `esp_wifi.h` | Wi-Fi 驱动初始化、配置、启动和连接接口 |
| `esp_event.h` | ESP-IDF 事件循环和事件回调注册 |
| `esp_log.h` | 日志输出 |
| `nvs_flash.h` | NVS Flash 初始化和擦除 |
| `lwip/err.h`、`lwip/sys.h` | LWIP 网络栈相关定义 |

## 配置宏说明

文件开头通过 `CONFIG_...` 宏从 `menuconfig` 获取 Wi-Fi 示例参数。

| 宏 | 来源 | 作用 |
|---|---|---|
| `EXAMPLE_ESP_WIFI_SSID` | `CONFIG_ESP_WIFI_SSID` | 目标 AP 的 SSID |
| `EXAMPLE_ESP_WIFI_PASS` | `CONFIG_ESP_WIFI_PASSWORD` | 目标 AP 的密码 |
| `ESP_MAXIMUM_RETRY` | `CONFIG_ESP_MAXIMUM_RETRY` | 最大重连次数 |
| `ESP_WIFI_SAE_MODE` | WPA3 SAE PWE 相关配置 | WPA3 SAE 密码元素派生模式 |
| `EXAMPLE_H2E_IDENTIFIER` | `CONFIG_ESP_WIFI_PW_ID` 或空字符串 | WPA3 Hash-to-Element 标识符 |
| `ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD` | 认证方式配置 | 扫描和连接时允许的最低认证模式 |

认证阈值 `ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD` 用于限制允许连接的 AP 安全等级。例如配置为 `WIFI_AUTH_WPA2_PSK` 后，低于 WPA2 的网络不会作为可接受目标。

## 全局状态

| 变量/宏 | 代码位置 | 作用 |
|---|---:|---|
| `s_wifi_event_group` | ~60 行 | FreeRTOS 事件组句柄，用来通知连接成功或失败 |
| `WIFI_CONNECTED_BIT` | ~65 行 | 事件组 bit0，表示已经连接 AP 并获取 IP |
| `WIFI_FAIL_BIT` | ~66 行 | 事件组 bit1，表示达到最大重试次数后仍连接失败 |
| `TAG` | ~68 行 | 日志标签，输出为 `wifi station` |
| `s_retry_num` | ~70 行 | 当前重试次数计数器 |

这里没有单独创建 Wi-Fi 任务，而是使用 ESP-IDF 默认事件循环分发 Wi-Fi/IP 事件，再由事件组把异步事件结果同步回 `wifi_init_sta()`。

## event_handler() 事件处理

`event_handler()` 是整个示例的连接状态机核心，注册给两个事件源：

| 事件源 | 事件 ID | 处理行为 |
|---|---|---|
| `WIFI_EVENT` | `WIFI_EVENT_STA_START` | Wi-Fi STA 启动后立即调用 `esp_wifi_connect()` 发起连接 |
| `WIFI_EVENT` | `WIFI_EVENT_STA_DISCONNECTED` | 断连后判断是否继续重连，超过最大次数则设置失败位 |
| `IP_EVENT` | `IP_EVENT_STA_GOT_IP` | 获取 IPv4 地址后清零重试次数，并设置成功位 |

### STA 启动事件

```c
if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
}
```

`esp_wifi_start()` 启动 Wi-Fi 驱动后会产生 `WIFI_EVENT_STA_START`。示例没有在 `wifi_init_sta()` 中直接调用 `esp_wifi_connect()`，而是等 STA 启动事件触发后再连接，这符合 ESP-IDF 的事件驱动模型。

### STA 断连事件

```c
if (s_retry_num < ESP_MAXIMUM_RETRY) {
    esp_wifi_connect();
    s_retry_num++;
} else {
    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
}
```

当连接失败或连接中断时进入该分支：

1. 如果 `s_retry_num` 小于最大重试次数，继续调用 `esp_wifi_connect()`。
2. 每次重试后 `s_retry_num++`。
3. 如果已经达到最大重试次数，设置 `WIFI_FAIL_BIT`。
4. `wifi_init_sta()` 中阻塞等待的 `xEventGroupWaitBits()` 会因为失败位被设置而返回。

注意：这里的最大重试次数由 `CONFIG_ESP_MAXIMUM_RETRY` 控制，示例适合演示连接流程。实际产品中通常还会加入退避延时、网络状态上报、配网入口或后台持续重连策略。

### 获取 IP 事件

```c
ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
s_retry_num = 0;
xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
```

`IP_EVENT_STA_GOT_IP` 表示 STA 已经完成关联、认证、DHCP 获取地址等关键步骤。示例在这里打印 IP，重置重试计数，并设置 `WIFI_CONNECTED_BIT`，表示连接流程成功完成。

## wifi_init_sta() 初始化流程

`wifi_init_sta()` 是 Wi-Fi Station 模式的主要初始化函数。

### 创建事件组

```c
s_wifi_event_group = xEventGroupCreate();
```

事件组用于把异步事件回调中的连接结果传递给当前初始化流程。该示例只使用两个 bit：

| bit | 含义 |
|---|---|
| `WIFI_CONNECTED_BIT` | 已获取 IP，连接成功 |
| `WIFI_FAIL_BIT` | 重试耗尽，连接失败 |

### 初始化网络接口和事件循环

```c
ESP_ERROR_CHECK(esp_netif_init());
ESP_ERROR_CHECK(esp_event_loop_create_default());
esp_netif_create_default_wifi_sta();
```

这三步完成 TCP/IP 网络层和默认事件循环准备：

1. `esp_netif_init()` 初始化 ESP-IDF 网络接口组件。
2. `esp_event_loop_create_default()` 创建默认事件循环，用于投递 Wi-Fi/IP 事件。
3. `esp_netif_create_default_wifi_sta()` 创建默认 STA 网络接口，并将 Wi-Fi 驱动与网络栈绑定。

### 初始化 Wi-Fi 驱动

```c
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
```

`WIFI_INIT_CONFIG_DEFAULT()` 提供 Wi-Fi 驱动默认初始化参数，通常包括内部缓冲区、任务、队列等配置。示例没有改动默认值。

### 注册事件回调

```c
esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);
```

这里注册了两个事件处理实例：

| 注册项 | 覆盖范围 |
|---|---|
| `WIFI_EVENT` + `ESP_EVENT_ANY_ID` | 所有 Wi-Fi 事件都会进入 `event_handler()`，函数内部再筛选关心的事件 |
| `IP_EVENT` + `IP_EVENT_STA_GOT_IP` | 只处理 STA 获取 IP 事件 |

示例中保存了 `instance_any_id` 和 `instance_got_ip`，但后续没有注销事件回调。对于一次性示例这没有问题；如果封装成可反复启动/停止的模块，应在释放 Wi-Fi 时使用这些句柄注销回调。

### 配置 STA 参数

```c
wifi_config_t wifi_config = {
    .sta = {
        .ssid = EXAMPLE_ESP_WIFI_SSID,
        .password = EXAMPLE_ESP_WIFI_PASS,
        .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
        .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
    },
};
```

`wifi_config_t` 的 `.sta` 字段配置 Station 连接参数：

| 字段 | 说明 |
|---|---|
| `.ssid` | 目标热点名称 |
| `.password` | 目标热点密码 |
| `.threshold.authmode` | 允许连接的最低认证模式 |
| `.sae_pwe_h2e` | WPA3 SAE PWE 模式 |
| `.sae_h2e_identifier` | WPA3 H2E 标识符 |

如果连接开放网络，认证阈值需要和 `menuconfig` 配置匹配。如果连接旧式 WEP/WPA 网络，也需要显式降低认证阈值，否则驱动可能拒绝低安全等级的 AP。

### 设置模式、写入配置并启动

```c
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
ESP_ERROR_CHECK(esp_wifi_start());
```

执行顺序很重要：

1. `esp_wifi_set_mode(WIFI_MODE_STA)` 设置为 Station 模式。
2. `esp_wifi_set_config(WIFI_IF_STA, &wifi_config)` 写入 STA 配置。
3. `esp_wifi_start()` 启动 Wi-Fi 驱动。

`esp_wifi_start()` 后会触发 `WIFI_EVENT_STA_START`，随后由事件回调调用 `esp_wifi_connect()`。

### 等待连接结果

```c
EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY
);
```

该调用会一直阻塞，直到连接成功或失败：

| 参数 | 当前值 | 含义 |
|---|---|---|
| 等待 bit | `WIFI_CONNECTED_BIT | WIFI_FAIL_BIT` | 等待成功位或失败位 |
| `clearOnExit` | `pdFALSE` | 返回后不自动清除事件位 |
| `waitForAllBits` | `pdFALSE` | 任意一个 bit 被设置即可返回 |
| 超时时间 | `portMAX_DELAY` | 永久等待 |

返回后根据 bit 判断实际结果：

| 返回 bit | 结果 |
|---|---|
| 包含 `WIFI_CONNECTED_BIT` | 连接成功，打印 SSID 和密码 |
| 包含 `WIFI_FAIL_BIT` | 连接失败，打印失败日志 |
| 其他 | 理论上不应发生，打印 `UNEXPECTED EVENT` |

## app_main() 启动入口

`app_main()` 是 ESP-IDF 应用入口，主要做三件事。

### 初始化 NVS

```c
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);
```

Wi-Fi 驱动依赖 NVS 保存 RF 校准数据、PHY 数据等信息。这里处理了两个常见异常：

| 错误码 | 含义 | 处理方式 |
|---|---|---|
| `ESP_ERR_NVS_NO_FREE_PAGES` | NVS 分区没有可用页 | 擦除 NVS 后重新初始化 |
| `ESP_ERR_NVS_NEW_VERSION_FOUND` | NVS 数据版本不兼容 | 擦除 NVS 后重新初始化 |

这段逻辑适合示例工程。产品代码中是否允许擦除整个 NVS，需要结合是否存储用户配置、证书、配网信息等数据谨慎处理。

### 调整 Wi-Fi 日志级别

```c
if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
    esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
}
```

当最大日志等级高于默认日志等级时，单独提高 `wifi` 模块日志输出级别，便于调试 Wi-Fi 驱动内部行为。

### 启动 STA 连接

```c
ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
wifi_init_sta();
```

打印当前示例模式，然后调用 `wifi_init_sta()` 完成连接。

## 关键 API 说明

| API | 作用 |
|---|---|
| `nvs_flash_init()` | 初始化 NVS Flash |
| `nvs_flash_erase()` | 擦除 NVS 分区 |
| `esp_netif_init()` | 初始化网络接口组件 |
| `esp_event_loop_create_default()` | 创建默认事件循环 |
| `esp_netif_create_default_wifi_sta()` | 创建默认 Wi-Fi STA 网络接口 |
| `esp_wifi_init()` | 初始化 Wi-Fi 驱动 |
| `esp_event_handler_instance_register()` | 注册事件处理函数 |
| `esp_wifi_set_mode()` | 设置 Wi-Fi 工作模式 |
| `esp_wifi_set_config()` | 设置 Wi-Fi 接口配置 |
| `esp_wifi_start()` | 启动 Wi-Fi |
| `esp_wifi_connect()` | 发起 STA 连接 |
| `xEventGroupSetBits()` | 设置事件组 bit |
| `xEventGroupWaitBits()` | 等待事件组 bit |

## 资源与同步关系

| 资源 | 创建位置 | 释放情况 | 说明 |
|---|---:|---|---|
| FreeRTOS Event Group | `wifi_init_sta()` ~97 行 | 示例未释放 | 用于等待连接成功/失败 |
| 默认事件循环 | `wifi_init_sta()` ~101 行 | 示例未删除 | 应用级默认事件循环，通常全局存在 |
| 默认 STA 网络接口 | `wifi_init_sta()` ~102 行 | 示例未销毁 | 绑定 Wi-Fi STA 和 TCP/IP 网络栈 |
| Wi-Fi 驱动 | `wifi_init_sta()` ~105 行 | 示例未反初始化 | 示例程序启动后持续运行 |
| 事件处理实例 | `wifi_init_sta()` ~109、~114 行 | 示例未注销 | 如果模块化复用，应补充注销逻辑 |

同步方式：

```text
ESP-IDF 事件循环线程
  -> 调用 event_handler()
  -> xEventGroupSetBits()
  -> 唤醒 wifi_init_sta() 中的 xEventGroupWaitBits()
```

## 日志输出行为

常见日志顺序如下：

```text
ESP_WIFI_MODE_STA
wifi_init_sta finished.
retry to connect to the AP
connect to the AP fail
got ip:xxx.xxx.xxx.xxx
connected to ap SSID:xxx password:xxx
```

如果密码错误、认证模式不匹配或 AP 不可达，可能多次看到：

```text
retry to connect to the AP
connect to the AP fail
```

达到最大重试次数后输出：

```text
Failed to connect to SSID:xxx, password:xxx
```

## 工程使用注意事项

| 注意点 | 说明 |
|---|---|
| 不建议在正式日志打印密码 | 示例会打印 `password:%s`，产品代码应避免泄露 Wi-Fi 密码 |
| NVS 擦除需谨慎 | 示例遇到 NVS 异常会整体擦除，产品中可能导致用户配置丢失 |
| `portMAX_DELAY` 会永久等待 | 如果连接流程没有设置成功/失败 bit，调用方会一直阻塞 |
| 重连策略较简单 | 示例只做固定次数立即重试，没有退避、持续重连或配网切换 |
| 事件处理未注销 | 作为长期运行示例可接受；模块化 Wi-Fi 管理器应补齐停止和释放流程 |
| 只覆盖 STA 模式 | 不包含 AP、APSTA、SmartConfig、BLE 配网等场景 |

## 可迁移到项目中的最小模块

如果要把该示例迁移到自己的工程，建议保留以下结构：

1. 在系统启动早期初始化 NVS。
2. 初始化 `esp_netif` 和默认事件循环。
3. 创建默认 STA 网络接口。
4. 初始化 Wi-Fi 驱动。
5. 注册 `WIFI_EVENT` 和 `IP_EVENT_STA_GOT_IP` 回调。
6. 配置 `wifi_config_t.sta`。
7. 设置 `WIFI_MODE_STA` 并启动 Wi-Fi。
8. 在事件回调中处理连接、断连、获取 IP。

建议产品化时调整以下点：

| 示例代码 | 产品化建议 |
|---|---|
| SSID/密码来自 `menuconfig` | 改为来自配网结果、NVS、云端或本地配置 |
| 日志打印密码 | 去掉密码日志或做脱敏 |
| 失败后只设置 `WIFI_FAIL_BIT` | 增加状态上报、退避重试、重新配网入口 |
| `wifi_init_sta()` 阻塞等待 | 可改为异步 Wi-Fi 管理任务或状态机 |
| 不注销事件回调 | 增加 `wifi_deinit`，停止 Wi-Fi 并释放资源 |

## 验证方式

可以通过以下方式验证该示例行为：

| 场景 | 预期结果 |
|---|---|
| SSID 和密码正确 | 打印 `got ip`，随后打印 `connected to ap` |
| 密码错误 | 多次打印重试日志，最终打印 `Failed to connect` |
| AP 关闭或超出范围 | 触发断连/失败流程，最终设置 `WIFI_FAIL_BIT` |
| 降低认证阈值连接旧 AP | 配置匹配时可连接 WEP/WPA；配置过高时可能拒绝连接 |
| 调高 Wi-Fi 日志级别 | `wifi` 模块输出更多调试日志 |

## 总结

该文件是一个标准的 ESP-IDF Wi-Fi STA 入门模板。它用较少代码串起了 NVS、网络接口、默认事件循环、Wi-Fi 驱动、事件回调和 FreeRTOS 事件组，适合理解 ESP32 连接路由器的基本链路。

核心设计思想是：Wi-Fi 连接本身是异步事件驱动的，`event_handler()` 负责响应状态变化，`xEventGroupWaitBits()` 负责把最终结果同步回初始化流程。
