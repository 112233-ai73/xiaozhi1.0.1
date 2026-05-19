# ESP-IDF sd_test_io.c 代码说明文档

## 背景

本文分析 ESP-IDF SDMMC SD 卡示例中的 `sd_test_io.c` 和 `sd_test_io.h`。这组文件用于在 SD 卡初始化失败时辅助检查 SDMMC 相关引脚的连接状态、上拉强度、电压水平和引脚间串扰。

源码位置：

`d:\Development\esp_idf_5.5.3\Espressif\frameworks\esp-idf-v5.5.3\examples\storage\sd_card\sdmmc\components\sd_card\sd_test_io.c`

头文件位置：

`d:\Development\esp_idf_5.5.3\Espressif\frameworks\esp-idf-v5.5.3\examples\storage\sd_card\sdmmc\components\sd_card\sd_test_io.h`

## 结论概览

`sd_test_io.c` 是一个示例级硬件诊断工具，不参与 SD 卡正常读写流程。它的入口函数是 `check_sd_card_pins()`，通常由 `sd_card_example_main.c` 在 `esp_vfs_fat_sdmmc_mount()` 初始化失败时调用，用来辅助判断 SD 卡 CLK/CMD/D0-D3 线路是否存在上拉不足、短路、接错或串扰异常。

整体执行链路如下：

```text
check_sd_card_pins()
  -> 将每个 SD 引脚配置为开漏输入输出
  -> 测试无内部上拉时的引脚恢复时间
  -> 测试开启弱上拉后的引脚恢复时间
  -> 如果启用 CONFIG_EXAMPLE_ENABLE_ADC_FEATURE:
      -> 初始化 ADC oneshot unit
      -> 为每个待测通道尝试 ADC 校准
      -> 测量各引脚电压
      -> 测量开启弱上拉后的各引脚电压
      -> 逐个拉低引脚，测量其他引脚电压，输出串扰矩阵
      -> 逐个拉低引脚，并给被测其他引脚开弱上拉，输出串扰矩阵
      -> 释放 ADC 校准句柄
```

## 文件结构

| 区域 | 行号 | 作用 |
|---|---:|---|
| 头文件与宏定义 | `sd_test_io.c:6-18` | 引入 ADC、GPIO、CPU cycle、日志接口，定义 ADC 衰减和 GPIO bit mask |
| ADC 校准初始化 | `sd_test_io.c:20-66` | 在启用 ADC 功能时，为 ADC 通道创建校准句柄 |
| ADC 校准释放 | `sd_test_io.c:68-76` | 按支持的校准方案删除校准句柄 |
| 单通道电压读取 | `sd_test_io.c:78-94` | 配置 ADC 通道，读取 raw 值，可选转换为 mV 后返回 V |
| GPIO 恢复时间测量 | `sd_test_io.c:97-104` | 用 CPU cycle 计数等待 GPIO 到达指定电平 |
| 对外诊断入口 | `sd_test_io.c:106-231` | 执行恢复时间、电压、串扰等测试 |
| 配置结构和声明 | `sd_test_io.h:15-24` | 定义 `pin_configuration_t` 并声明 `check_sd_card_pins()` |

## 对外接口

### `pin_configuration_t`

`pin_configuration_t` 在 `sd_test_io.h:15-21` 定义：

```c
typedef struct {
    const char** names;
    const int* pins;
#if CONFIG_EXAMPLE_ENABLE_ADC_FEATURE
    const int *adc_channels;
#endif
} pin_configuration_t;
```

| 字段 | 说明 |
|---|---|
| `names` | 每个 SD 信号的显示名称，例如 `CLK`、`CMD`、`D0`、`D1`、`D2`、`D3` |
| `pins` | 每个 SD 信号对应的 GPIO 编号 |
| `adc_channels` | 启用 ADC 功能时，每个 SD 信号对应的 ADC 通道编号 |

该结构不持有内存，只保存外部数组指针。调用方需要保证 `names`、`pins` 和 `adc_channels` 的元素数量与 `pin_count` 一致。

### `check_sd_card_pins()`

```c
void check_sd_card_pins(pin_configuration_t *config, const int pin_count);
```

这是唯一对外暴露的诊断入口。它不会返回错误码，诊断结果通过 `ESP_LOGI()` 和 `printf()` 输出到串口日志。

调用方通常这样准备配置：

```c
const char* names[] = {"CLK", "CMD", "D0", "D1", "D2", "D3"};
const int pins[] = {pin_clk, pin_cmd, pin_d0, pin_d1, pin_d2, pin_d3};

pin_configuration_t config = {
    .names = names,
    .pins = pins,
};

check_sd_card_pins(&config, sizeof(pins) / sizeof(pins[0]));
```

## 关键宏与配置项

| 宏/配置项 | 行号 | 说明 |
|---|---:|---|
| `ADC_ATTEN_DB` | `sd_test_io.c:17` | ADC 衰减配置，当前为 `ADC_ATTEN_DB_12`，用于扩大可测电压范围 |
| `GPIO_INPUT_PIN_SEL(pin)` | `sd_test_io.c:18` | 将 GPIO 编号转换为 `gpio_config_t.pin_bit_mask` |
| `CONFIG_EXAMPLE_ENABLE_ADC_FEATURE` | `sd_test_io.c:20`、`143` | 控制是否编译 ADC 电压和串扰检测逻辑 |
| `CONFIG_EXAMPLE_ADC_UNIT` | `sd_test_io.c:147`、`155` | 指定使用哪个 ADC unit |
| `ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED` | `sd_test_io.c:27` | 支持曲线拟合校准时优先使用 |
| `ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED` | `sd_test_io.c:42` | 不支持曲线拟合或未校准时，尝试线性拟合校准 |

## 详细设计

### ADC 校准初始化

`adc_calibration_init()` 只在 `CONFIG_EXAMPLE_ENABLE_ADC_FEATURE` 打开时编译，位于 `sd_test_io.c:21-66`。

流程如下：

```text
adc_calibration_init(unit, channel, atten, out_handle)
  -> 优先尝试 adc_cali_create_scheme_curve_fitting()
  -> 如果未成功，再尝试 adc_cali_create_scheme_line_fitting()
  -> 将 handle 写入 out_handle
  -> 根据 ret 打印校准不可用或参数/内存错误日志
  -> 返回 calibrated
```

| 校准方案 | 代码位置 | 使用条件 |
|---|---:|---|
| Curve Fitting | `27-40` | 当前芯片和 IDF 配置支持 `ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED` |
| Line Fitting | `42-54` | 支持 `ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED` 且尚未校准成功 |

如果 eFuse 没有烧录 ADC 校准数据，函数会打印 `eFuse not burnt, skip software calibration`，并返回 `false`。

### ADC 校准释放

`example_adc_calibration_deinit()` 位于 `sd_test_io.c:68-76`，根据编译期支持的校准方案调用对应删除函数：

```text
adc_cali_delete_scheme_curve_fitting()
或
adc_cali_delete_scheme_line_fitting()
```

当前函数使用 `ESP_ERROR_CHECK()`，如果删除失败会触发错误检查逻辑。示例场景可以接受，产品代码可以改成显式返回错误并继续清理其他资源。

### ADC 电压读取

`get_pin_voltage()` 位于 `sd_test_io.c:78-94`，用于读取一个 ADC 通道电压：

```text
adc_oneshot_config_channel()
  -> adc_oneshot_read()
  -> 如果 do_calibration 为 true:
       adc_cali_raw_to_voltage()
  -> 返回 voltage / 1000.0
```

注意：如果没有校准成功，当前 `voltage` 初始值为 `0`，函数会返回 `0.0V`，而不是根据 raw ADC 值估算电压。因此未校准时的电压输出不能当作真实电压值使用。

### GPIO 恢复时间测量

`get_cycles_until_pin_level()` 位于 `sd_test_io.c:97-104`：

```c
while (gpio_get_level(i) == !level &&
       esp_cpu_get_cycle_count() - start < timeout) {
    ;
}
```

它记录从开始等待到引脚达到目标电平之间经过的 CPU cycles。`timeout` 也是 cycle 计数阈值，不是微秒或毫秒。调用方传入 `10000`，含义是最多等待约 10000 个 CPU cycle。

这个函数用于观察引脚从被拉低释放到变高的速度。恢复越慢，通常越可能说明上拉较弱、电容较大、连接异常或线路被其他器件影响。

### 引脚基础配置

`check_sd_card_pins()` 开始时将每个待测 GPIO 配置为开漏输入输出：

```c
io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
io_conf.pull_down_en = 0;
io_conf.pull_up_en = 0;
gpio_config(&io_conf);
```

使用开漏模式的原因是测试时需要先把线拉低，然后释放输出，让线路依靠外部上拉或内部弱上拉恢复为高电平。这与 SD 卡总线需要上拉电阻的诊断目标一致。

### PIN recovery time

第一组输出标题是：

```text
**** PIN recovery time ****
```

对应代码位于 `sd_test_io.c:119-128`。每个引脚执行：

```text
设置开漏输出
输出低电平
等待 100 us
输出高电平，也就是释放开漏输出
等待 GPIO 读到高电平
打印恢复所需 CPU cycles
```

如果某个引脚恢复时间明显大于其他引脚，可能存在外部上拉不足、线路电容过大、短路、焊接异常或有其他器件强拉低。

### PIN recovery time with weak pullup

第二组输出标题是：

```text
**** PIN recovery time with weak pullup ****
```

对应代码位于 `sd_test_io.c:130-141`。流程与第一组类似，但测试前会调用 `gpio_pullup_en()` 打开内部弱上拉，测试后再 `gpio_pullup_dis()`。

这组数据用于对比内部弱上拉是否能明显改善恢复时间。若打开弱上拉后恢复时间仍异常，说明问题可能不只是上拉不足，也可能是硬件短路、接错或外部负载过重。

### PIN voltage levels

启用 ADC 功能后，代码会创建 ADC oneshot unit，并给每个待测项准备校准句柄，位置为 `sd_test_io.c:145-156`。

随后输出：

```text
**** PIN voltage levels ****
```

对应代码位于 `sd_test_io.c:158-163`。它会读取 `config->adc_channels[i]` 对应通道，并打印每个 SD 引脚的电压。

这组数据用于判断引脚静态电压是否接近预期高电平。对于 3.3 V SD 卡总线，空闲状态一般应接近 3.3 V；如果某条线显著偏低，可能有短路、未上拉、外设拉低或配置冲突。

### PIN voltage levels with weak pullup

输出标题：

```text
**** PIN voltage levels with weak pullup ****
```

对应代码位于 `sd_test_io.c:165-173`。每个引脚先打开内部弱上拉，等待 100 us 后读取电压，再关闭弱上拉。

这组数据和恢复时间类似，用来判断内部弱上拉是否能把线拉到较高电压。如果弱上拉开启后仍明显偏低，说明线路可能被较强下拉或短路影响。

### PIN cross-talk

输出标题：

```text
**** PIN cross-talk ****
```

对应代码位于 `sd_test_io.c:175-197`。测试方法是逐个把某个引脚拉低，然后测量其他引脚的 ADC 电压，形成一个矩阵。

```text
for each i:
  将 pins[i] 配置为开漏输出并拉低
  for each j != i:
    读取 adc_channels[j] 的电压
  将 pins[i] 恢复为输入
```

如果拉低某一根线时，其他线电压也明显下降，可能说明引脚间存在短路、串扰、焊接桥连或板级设计问题。

### PIN cross-talk with weak pullup

输出标题：

```text
**** PIN cross-talk with weak pullup ****
```

对应代码位于 `sd_test_io.c:199-223`。它同样逐个拉低某根线，但在测量其他线前，会给被测的其他 GPIO 打开内部弱上拉。

这能帮助区分“被测线本身没有上拉”与“被拉低线影响了其他线”。如果开启弱上拉后其他线仍被明显拉低，硬件耦合或短路嫌疑更大。

## 输出解读建议

| 输出项 | 正常倾向 | 异常倾向 |
|---|---|---|
| `PIN recovery time` | 各 SD 线恢复 cycles 接近，且不接近超时值 | 某一根线 cycles 特别大或接近 `10000` |
| `PIN recovery time with weak pullup` | 开弱上拉后恢复更快或更稳定 | 开弱上拉后仍明显变慢 |
| `PIN voltage levels` | 空闲线接近 SD IO 电压 | 某线电压偏低或为 0 |
| `PIN voltage levels with weak pullup` | 弱上拉后电压上升 | 弱上拉后仍偏低 |
| `PIN cross-talk` | 拉低一根线时，其他线电压基本保持高电平 | 其他线随之明显降低 |
| `PIN cross-talk with weak pullup` | 其他线弱上拉后保持高电平 | 其他线仍被拉低 |

这些结果是辅助诊断，不等同于完整电气测试。最终仍应结合原理图、万用表/示波器测量、SD 卡座焊接检查和 ESP-IDF 引脚配置确认。

## 与 sd_card_example_main.c 的关系

`sd_card_example_main.c` 在挂载失败时调用该诊断函数：

```text
esp_vfs_fat_sdmmc_mount()
  -> 如果返回 ESP_FAIL:
       提示文件系统挂载失败，可考虑格式化
  -> 如果返回其他错误:
       提示检查上拉电阻
       可选调用 check_sd_card_pins()
```

也就是说，`sd_test_io.c` 主要处理“SD 卡初始化失败、疑似硬件线路问题”的场景，而不是“FAT 文件系统损坏”的场景。`ESP_FAIL` 更偏文件系统挂载失败；其他错误更可能与通信、供电、上拉或引脚有关。

## 资源与生命周期

| 资源 | 创建位置 | 释放位置 | 说明 |
|---|---:|---:|---|
| GPIO 配置 | `110-117` | 无显式恢复 | 函数会改变传入 SD 引脚的 GPIO 模式 |
| ADC oneshot unit | `145-149` | 未显式释放 | 示例未调用 ADC unit 删除接口 |
| `adc_cali_handle` 数组 | `151` | 未 `free()` | 示例只释放每个校准句柄，没有释放数组内存 |
| `do_calibration` 数组 | `152` | 未 `free()` | 示例未释放该数组 |
| 每个 ADC 校准句柄 | `154-156` | `225-229` | 仅在 `do_calibration[i]` 为 true 时释放 |

因为该函数通常只在初始化失败时运行一次，示例中这些资源问题影响有限。但如果在产品中频繁调用，应补齐 ADC unit 删除、数组 `free()` 和 GPIO 状态恢复。

## 风险点与注意事项

| 风险点 | 代码位置 | 说明 |
|---|---:|---|
| 会改写 SD 引脚 GPIO 模式 | `110-141`、`182-223` | 调用后不再保持 SDMMC 外设配置，适合失败诊断，不适合在正常挂载期间调用 |
| `timeout` 是 CPU cycles | `97-104` | 不是时间单位；CPU 频率不同会影响数值含义 |
| 未校准时电压返回 0 | `80-93` | `do_calibration == false` 时没有把 ADC raw 值换算为电压 |
| ADC 动态数组未释放 | `151-152` | 示例短流程可接受，长期运行或重复调用需修正 |
| ADC oneshot unit 未释放 | `145-149` | 重复调用可能造成资源占用 |
| ADC 校准通道索引需确认 | `154-156` | 当前初始化传入的是循环索引 `i`，读取使用的是 `config->adc_channels[i]`；如果 ADC 通道号不等于数组索引，建议改为使用 `config->adc_channels[i]` 初始化校准 |
| 串扰读取使用校准索引需确认 | `191-193`、`215-218` | 读取 `adc_channels[j]` 时传入的是 `do_calibration[i]` 和 `adc_cali_handle[i]`，更稳妥的写法通常应对应 `j` |
| `ESP_ERROR_CHECK()` 会中止流程 | `86`、`88`、`90`、`149` | 示例便于暴露错误，产品代码应返回错误并清理资源 |

## 生产化改造建议

| 建议 | 原因 |
|---|---|
| 只在 SD 卡未挂载或初始化失败后调用 | 避免破坏 SDMMC 外设正在使用的引脚状态 |
| 将诊断结果结构化返回 | 产品中比串口文本更容易上报和自动判定 |
| 释放 ADC unit 和动态数组 | 避免重复诊断造成资源泄漏 |
| 恢复 GPIO 原始配置 | 便于诊断后重新尝试初始化 SD 卡 |
| ADC 未校准时输出 raw 值 | 避免误把 `0.0V` 当作真实低电平 |
| 校准句柄按实际 ADC 通道建立和使用 | 减少 ADC 通道映射不一致导致的误判 |
| 把 cycle 转换为时间 | 不同 CPU 频率下更容易比较 |
| 对输出阈值做板级配置 | 不同 SD IO 电压、走线和上拉值会影响判定 |

## 移植步骤

1. 确认目标工程启用了 SD 卡相关 GPIO 配置，并能传入 `names` 和 `pins` 数组。
2. 如果只需要恢复时间测试，可关闭 `CONFIG_EXAMPLE_ENABLE_ADC_FEATURE`，减少 ADC 依赖。
3. 如果需要电压和串扰测试，补充每个 SD 信号对应的 `adc_channels`。
4. 确认这些 SD 引脚确实支持 ADC 输入；不支持 ADC 的 GPIO 不能参与电压检测。
5. 将 `check_sd_card_pins()` 放在 SD 卡初始化失败后的诊断路径中，不要在 SD 卡正常挂载期间调用。
6. 如果会多次调用，补齐 ADC unit 删除、数组释放和 GPIO 状态恢复。
7. 根据板级 IO 电压和上拉设计，定义恢复时间、电压和串扰判定阈值。

## 验证方式

| 验证项 | 操作 | 预期结果 |
|---|---|---|
| 正常接线 | SD 卡线和上拉正确连接后调用 | 各引脚恢复时间接近，电压接近 SD IO 电压 |
| 断开外部上拉 | 临时去掉某条线外部上拉 | 该线恢复时间变慢，弱上拉测试有明显变化 |
| 人为短接两条线 | 在安全条件下短接两条待测信号 | 串扰矩阵中对应线电压互相影响 |
| ADC 功能关闭 | 关闭 `CONFIG_EXAMPLE_ENABLE_ADC_FEATURE` | 只输出恢复时间，不输出电压和串扰 |
| ADC 功能开启但校准不可用 | 使用未烧录 ADC 校准 eFuse 的芯片 | 打印校准警告，电压读数不应作为真实电压判断 |

## 总结

`sd_test_io.c` 是 ESP-IDF SDMMC 示例里的硬件辅助诊断代码。它通过开漏拉低/释放观察 SD 引脚恢复时间，并可选借助 ADC 测量电压和串扰，帮助定位 SD 卡初始化失败时的上拉、接线、短路和引脚配置问题。

把它迁移到正式项目时，建议保留“失败后诊断”的定位，不要把它放进正常 SD 卡读写路径；同时补齐资源释放、GPIO 状态恢复、ADC 通道映射和结构化结果输出，避免示例代码的简化处理影响长期运行稳定性。
