# F4Camera 工程状态

> 审计日期：2026-07-14  
> 审计依据：当前 `F4Camera.ioc`、`Core`、`BSP`、`MDK-ARM/F4Camera.uvprojx`、Keil 构建日志和 MAP 文件。  
> 状态含义：✅ 已完成｜⚠️ 部分完成｜⏳ 未完成｜❌ 存在问题

本文档用于在脱离当前聊天上下文后交接工程。未在源码、配置或构建产物中找到证据的功能，不视为已完成。

## 一、项目基本信息

| 项目 | 状态 | 当前信息 |
|---|---|---|
| MCU | ✅ 已完成 | STM32F407ZGT6；CubeMX/Keil 器件名 `STM32F407ZGTx`；LQFP144 |
| 开发板 | ⚠️ 部分完成 | LCD BSP 注释指向正点原子 M144Z-M4 STM32F407 平台；仓库内没有原理图，具体板卡版本和全部连线未独立核验 |
| 摄像头 | ⏳ 未完成 | 项目目标为 OV2640；当前没有 OV2640 驱动文件 |
| LCD | ⚠️ 部分完成 | 正点原子 2.8 寸、8080 并口 LCD；FSMC 驱动与测试代码已编译，硬件显示结果未知 |
| 软件框架 | ✅ 已完成 | STM32CubeMX 生成结构 + STM32 HAL；固件包 `STM32Cube FW_F4 V1.28.3` |
| CubeMX | ✅ 已完成 | `F4Camera.ioc`，CubeMX 6.18.0 |
| Keil | ✅ 已完成 | μVision 5.43.1 / MDK-ARM Community 5.43 |
| 编译器 | ✅ 已完成 | Arm Compiler 6，ArmClang 6.24 |
| 调试器配置 | ⚠️ 部分完成 | Keil 工程使用 ST-Link 调试配置；仓库内没有本轮下载或运行记录 |

### 工程目标

将 OV2640 摄像头实验迁移为可维护的 STM32CubeMX + Keil + HAL 工程，最终实现：

- OV2640 初始化与图像输出。
- DCMI + DMA 图像采集。
- JPEG 数据接收、帧边界和长度处理。
- USART1 调试输出。
- USART2 图像数据发送至 PC。
- LCD 用于显示和硬件调试。

当前阶段：**LCD/FSMC 软件接入和编译已完成，等待硬件确认；摄像头应用层尚未迁移。**

## 二、当前完成状态

### 1. CubeMX 工程状态

| 模块 | 状态 | 当前配置与验证范围 |
|---|---|---|
| GPIO | ⚠️ 部分完成 | 已生成 LED、按键、LCD 背光、FSMC、DCMI、SCCB、OV_PWDN、OV_RESET、OV_FLASH、USART 引脚配置；只确认配置和编译存在，未确认全部硬件连线 |
| RCC | ⚠️ 部分完成 | HSE=8 MHz，PLL M/N/P=8/336/2，SYSCLK/HCLK=168 MHz，APB1=42 MHz，APB2=84 MHz；编译通过，实机时钟未记录 |
| FSMC | ⚠️ 部分完成 | Bank1 NOR/SRAM4、16 bit、Extended Mode；NE4=PG12，A6=PF12；HAL 初始化和 LCD 代码已编译，LCD 总线实测未知 |
| DCMI | ⚠️ 部分完成 | 硬件同步、8 bit、全帧、PCLK 上升沿、VSYNC/HSYNC 低有效；`JPEGMode=DISABLE`；只存在初始化代码，未开始采集 |
| DMA | ⚠️ 部分完成 | DCMI 使用 DMA2 Stream1 / Channel 1，外设到内存，Word 对齐，循环模式，Very High 优先级，FIFO Half Full；无采集缓冲和启动调用 |
| USART1 | ⚠️ 部分完成 | PA9/PA10，115200，8N1，TX/RX；仅初始化，无调试发送代码 |
| USART2 | ⚠️ 部分完成 | PA2/PA3，921600，8N1，HAL 模式为 TX；仅初始化，无图像发送协议或发送代码 |
| TIM6 | ⚠️ 部分完成 | APB1 定时器时钟 84 MHz，Prescaler=8399，Period=9999，更新频率约 1 Hz；中断已配置为优先级 2，但未调用启动函数 |
| NVIC | ⚠️ 部分完成 | DCMI 和 DMA2 Stream1 中断均启用、抢占优先级均为 0；中断入口调用 HAL，应用回调未实现 |

#### 关键引脚

- LCD：PG12=`FSMC_NE4`，PF12=`FSMC_A6`，PD/PE 为 16 位数据总线，PD4=`FSMC_NOE`，PD5=`FSMC_NWE`。
- LCD 背光：PB15 普通 GPIO 输出；实际连接和有效电平未确认。
- DCMI：PC6/7/8/9=`D0..D3`，PC11=`D4`，PB6=`D5`，PE5/6=`D6/D7`，PA4=`HSYNC`，PA6=`PIXCLK`，PB7=`VSYNC`。
- SCCB 预留：PD6=`SCCB_SCL`，PD7=`SCCB_SDA`。
- OV2640 控制预留：PG9=`OV_PWDN`，PG15=`OV_RESET`，PA8=`OV_FLASH`。
- USART1：PA9/PA10；USART2：PA2/PA3。

### 2. LCD/FSMC 状态

| 检查项 | 状态 | 结果 |
|---|---|---|
| `BSP/LCD` 目录 | ✅ 已完成 | 实际存在 |
| `lcd.c` | ✅ 已完成 | 存在并加入 Keil `BSP/LCD` 分组 |
| `lcd_ex.c` | ✅ 已完成 | 存在、独立编译并加入 Keil，不由 `lcd.c` 直接包含 |
| `lcd.h` | ✅ 已完成 | 存在；定义 NE4/A6 地址映射和 PB15 背光 |
| `lcd_ex.h` | ✅ 已完成 | 存在；声明各 LCD 控制器初始化函数 |
| `lcdfont.h` | ✅ 已完成 | 存在并由 `lcd.c` 引用 |
| Keil Include Path | ✅ 已完成 | 已包含 `..\BSP\LCD` |
| 重复 FSMC 初始化 | ✅ 已完成 | LCD BSP 中没有 `HAL_SRAM_Init`、`HAL_SRAM_MspInit` 或旧 `g_sram_handle`；FSMC 由 `MX_FSMC_Init()` 管理 |
| LCD 调用 | ✅ 已完成 | `main.c` 的 `USER CODE BEGIN 2` 中调用 `lcd_init()` |
| LCD 测试代码 | ✅ 已完成 | 白色清屏；显示 `LCD TEST`、`FSMC OK`、十六进制 `lcddev.id`；绘制红/绿/蓝色块 |
| 编译与链接 | ✅ 已完成 | LCD 源文件编译通过；MAP 确认 `main()` 引用并链接 `lcd_init()` |
| LCD 实际显示 | ⏳ 未完成 | 工程内没有上板显示结果 |
| LCD ID 结果 | ⏳ 未完成 | 代码会显示 `LCD ID: 0xXXXX`，但工程内没有实际读取值记录 |

当前完整初始化顺序：

```text
HAL_Init()
SystemClock_Config()
MX_GPIO_Init()
MX_DMA_Init()
MX_DCMI_Init()
MX_FSMC_Init()
MX_TIM6_Init()
MX_USART1_UART_Init()
MX_USART2_UART_Init()
lcd_init()
LCD 测试内容
```

LCD 依赖关系满足：`HAL_Init()`、系统时钟、GPIO 和 `MX_FSMC_Init()` 均先于 `lcd_init()`。

FSMC/LCD 地址配置：

- Bank：FSMC Bank1 NOR/SRAM4。
- 数据宽度：16 bit。
- `LCD_FSMC_NEX=4`，`LCD_FSMC_AX=6`。
- `LCD_BASE=0x6C00007E`。
- `LCD_REG=0x6C00007E`，`LCD_RAM=0x6C000080`。
- 读时序：AddressSetup=15，DataSetup=60。
- 写时序：AddressSetup=9，DataSetup=9。

### 3. 摄像头、DCMI、JPEG 和 USART 状态

#### OV2640 / SCCB

| 检查项 | 状态 | 结果 |
|---|---|---|
| OV2640 驱动文件 | ⏳ 未完成 | `BSP` 中只有 `LCD`；没有 OV2640 目录或源文件 |
| 加入 Keil | ⏳ 未完成 | Keil 工程没有 OV2640/SCCB 分组或源文件 |
| SCCB GPIO | ⚠️ 部分完成 | PD6/PD7 已配置并生成宏；没有 SCCB 起始、停止、读写或 ACK 代码 |
| PWDN/RESET | ⚠️ 部分完成 | PG9/PG15 已配置；没有 OV2640 上电复位时序和初始化流程 |
| 摄像头 XCLK | ❌ 存在问题 | 工程内未找到 XCLK 输出配置或代码；PA8 当前为 `OV_FLASH` 普通 GPIO。若模块无板载时钟，此项会阻塞摄像头工作 |
| OV2640 初始化 | ⏳ 未完成 | 没有寄存器表、ID 检测、分辨率或格式配置 |

#### DCMI / DMA

| 检查项 | 状态 | 结果 |
|---|---|---|
| CubeMX DCMI 配置 | ⚠️ 部分完成 | DCMI 引脚、同步极性、8 位模式和中断已生成 |
| HAL 初始化 | ✅ 已完成 | `MX_DCMI_Init()`、MSP、DMA 链接和 IRQ Handler 均存在并编译 |
| DMA 配置 | ⚠️ 部分完成 | DMA2 Stream1 / Channel 1 已配置；未验证是否满足最终 JPEG 变长数据流程 |
| DMA 缓冲区 | ⏳ 未完成 | 没有静态图像/JPEG 缓冲区及容量设计 |
| 启动采集 | ⏳ 未完成 | 没有 `HAL_DCMI_Start_DMA()` 调用 |
| 帧/错误回调 | ⏳ 未完成 | 没有应用层 Frame/Vsync/Line/Error 回调 |
| 实际图像采集 | ⏳ 未完成 | 没有采集结果、帧计数或图像数据记录 |

#### JPEG

| 检查项 | 状态 | 结果 |
|---|---|---|
| OV2640 JPEG 输出配置 | ⏳ 未完成 | 无 OV2640 驱动和 JPEG 寄存器配置 |
| DCMI JPEG 模式 | ❌ 存在问题 | 当前为 `DCMI_JPEG_DISABLE`，与最终 JPEG 目标不一致 |
| JPEG 帧边界/长度 | ⏳ 未完成 | 没有 SOI/EOI、DMA 写入位置或帧长度处理 |
| JPEG 数据流程 | ⏳ 未完成 | 没有摄像头→DCMI/DMA→缓冲区→USART/LCD 数据链路 |

#### USART

| 检查项 | 状态 | 结果 |
|---|---|---|
| USART1 调试口 | ⚠️ 部分完成 | 115200 TX/RX 初始化存在；无 `printf` 重定向和调试发送代码 |
| USART2 图像传输 | ⚠️ 部分完成 | 921600、TX 模式初始化存在；无发送调用、帧协议、流控、DMA 或 PC 端适配 |
| PC 端 XCAM | ⏳ 未完成 | 工程内没有 XCAM 协议或 PC 端验证记录 |

## 三、当前工程结构

仅列出当前实际存在的主要目录和文件：

```text
F4Camera/
├── .agents/
├── BSP/
│   └── LCD/
│       ├── lcd.c
│       ├── lcd.h
│       ├── lcd_ex.c
│       ├── lcd_ex.h
│       └── lcdfont.h
├── Core/
│   ├── Inc/
│   │   ├── dcmi.h
│   │   ├── dma.h
│   │   ├── fsmc.h
│   │   ├── gpio.h
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   ├── stm32f4xx_it.h
│   │   ├── tim.h
│   │   └── usart.h
│   └── Src/
│       ├── dcmi.c
│       ├── dma.c
│       ├── fsmc.c
│       ├── gpio.c
│       ├── main.c
│       ├── stm32f4xx_hal_msp.c
│       ├── stm32f4xx_it.c
│       ├── system_stm32f4xx.c
│       ├── tim.c
│       └── usart.c
├── Docs/
│   └── LCD_FSMC迁移阶段检查报告.md
├── Drivers/
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── MDK-ARM/
│   ├── F4Camera.uvprojx
│   ├── F4Camera.uvoptx
│   ├── startup_stm32f407xx.s
│   └── F4Camera/
│       ├── F4Camera.axf
│       ├── F4Camera.hex
│       ├── F4Camera.map
│       └── F4Camera.build_log.htm
├── .mxproject
├── F4Camera.ioc
└── PROJECT_STATUS.md
```

当前不存在 `BSP/OV2640`、`BSP/JPEG` 或独立 `Application` 目录。

## 四、编译状态

| 项目 | 状态 | 结果 |
|---|---|---|
| Keil 构建 | ✅ 已完成 | 当前目标 `F4Camera` 构建成功 |
| Error | ✅ 已完成 | 0 |
| Warning | ✅ 已完成 | 0 |
| Code | ✅ 已完成 | 23166 bytes |
| RO-data | ✅ 已完成 | 18730 bytes |
| RW-data | ✅ 已完成 | 16 bytes |
| ZI-data | ✅ 已完成 | 2112 bytes |
| Total ROM | ✅ 已完成 | 41912 bytes（40.93 KiB） |
| Total RW | ✅ 已完成 | 2128 bytes（2.08 KiB） |

尺寸来自当前 `MDK-ARM/F4Camera/F4Camera.map`；当前固件已经链接 LCD 初始化、字体和测试代码。没有未解决的编译错误或警告。

## 五、硬件验证状态

工程内没有足够记录可确认任何本轮硬件功能已经通过。编译通过不等于硬件成功。

| 硬件项目 | 状态 | 当前证据 |
|---|---|---|
| ST-Link 下载和运行 | ⏳ 未完成 | 无本轮下载日志或用户确认记录 |
| LCD 背光 | ⏳ 未完成 | 代码会置 PB15，但无实测结果 |
| LCD 清屏/文字/RGB 色块 | ⏳ 未完成 | 测试代码存在，无实际显示结果 |
| LCD 控制器 ID | ⏳ 未完成 | 读取并显示代码存在，实际 ID 未记录 |
| FSMC NE4/A6 总线 | ⏳ 未完成 | 配置和编译一致，未通过实物显示确认 |
| OV2640 SCCB 通信 | ⏳ 未完成 | 无驱动和实测 |
| OV2640 图像采集 | ⏳ 未完成 | 无采集启动和实测 |
| JPEG 输出 | ⏳ 未完成 | 无实现和实测 |
| USART1 调试输出 | ⏳ 未完成 | 无发送代码和实测 |
| USART2 图像发送 | ⏳ 未完成 | 无发送代码和实测 |
| PC 端 XCAM 显示 | ⏳ 未完成 | 无协议实现和实测 |

## 六、当前问题与风险

1. ❌ 存在问题：OV2640 驱动、SCCB 协议、寄存器表和初始化流程完全缺失。
2. ❌ 存在问题：工程内没有摄像头 XCLK 输出配置；PA8 当前配置为 `OV_FLASH`。必须根据原理图确认模块是否自带时钟，否则先补充 XCLK。
3. ❌ 存在问题：最终目标包含 JPEG，但当前 DCMI 配置为 `DCMI_JPEG_DISABLE`，且没有 JPEG 帧边界和长度处理。
4. ⚠️ 部分完成：DCMI DMA 使用循环模式，尚未定义变长 JPEG 帧的缓冲容量、回卷、溢出和双缓冲策略。
5. ⚠️ 部分完成：DCMI/DMA 缓冲必须放在 DMA 可访问的 SRAM1/SRAM2，不能放入 Cortex-M4 CCM RAM；当前尚无缓冲区设计。
6. ⚠️ 部分完成：DCMI 与 DMA2 Stream1 中断均使用最高抢占优先级 0；进入连续采集后需重新评估与 USART、SysTick 和错误处理的优先级关系。
7. ⚠️ 部分完成：DCMI 的 PCLK、HSYNC、VSYNC 极性和 GPIO 上拉配置尚未与实际 OV2640 输出波形验证。
8. ⚠️ 部分完成：PLLQ=4 导致 CubeMX 的“48 MHz 域”显示为 84 MHz；当前未使用 USB/SDIO/RNG，但后续若启用这些外设必须修正时钟树。
9. ⚠️ 部分完成：FSMC 读写时序、PB15 背光极性及 LCD 控制器兼容性只通过静态检查和编译，仍需上板验证。
10. ⚠️ 部分完成：USART2 HAL 模式为 TX，但 PA3 仍保留为 USART2_RX；不会阻止当前编译，但应在最终 CubeMX 配置中确认是否需要占用该引脚。
11. ⚠️ 部分完成：`F4Camera.ioc` 同时记录 `CompilerLinker=GCC` 和 `TargetToolchain=MDK-ARM V5.32`，实际 Keil 使用 ArmClang 6.24；CubeMX 重新生成前应确认工具链设置不会发生漂移。
12. ⚠️ 部分完成：仓库没有板级原理图，开发板版本、摄像头 XCLK、LCD 背光和全部接口连线无法仅靠源码最终确认。

## 七、下一步开发计划

### Phase 1：LCD/FSMC 硬件验证

- ⏳ 未完成：使用 ST-Link 下载当前 HEX。
- ⏳ 未完成：确认背光、白色清屏、`LCD TEST`、`FSMC OK` 和 RGB 色块。
- ⏳ 未完成：记录实际 `lcddev.id`。
- ⏳ 未完成：若显示异常，通过 CubeMX 调整 FSMC 时序，不直接修改 `fsmc.c` 非 USER CODE 区域。

### Phase 2：SCCB 与 OV2640 初始化

- ⏳ 未完成：根据原理图确认 XCLK、PWDN、RESET 和 SCCB 连线。
- ⏳ 未完成：建立 `BSP/OV2640`，迁移 SCCB 和 OV2640 驱动。
- ⏳ 未完成：读取 OV2640 PID/VER，验证 SCCB 通信。
- ⏳ 未完成：配置基础分辨率和原始/YUV 输出，先不同时引入 JPEG 与 USART 传输。

### Phase 3：DCMI + DMA 采集

- ⏳ 未完成：设计 DMA 可访问的静态缓冲区和容量。
- ⏳ 未完成：实现 `HAL_DCMI_Start_DMA()`、帧完成和错误处理。
- ⏳ 未完成：先验证单帧或固定长度采集，再评估循环/双缓冲模式。

### Phase 4：JPEG 数据流程

- ⏳ 未完成：配置 OV2640 JPEG 输出和 DCMI JPEG 模式。
- ⏳ 未完成：实现帧开始/结束、SOI/EOI、实际长度、溢出和丢帧处理。
- ⏳ 未完成：验证连续多帧稳定性。

### Phase 5：USART 与 PC 端传输

- ⏳ 未完成：使用 USART1 输出初始化、ID、帧长和错误状态。
- ⏳ 未完成：为 USART2 设计图像帧协议和发送机制。
- ⏳ 未完成：评估 921600 baud 吞吐量，必要时使用 UART DMA 和分包。
- ⏳ 未完成：对接并验证 PC 端 XCAM 显示。

### Phase 6：系统联调

- ⏳ 未完成：打通 OV2640 → DCMI/DMA → JPEG 缓冲 → USART/PC 数据链路。
- ⏳ 未完成：根据带宽决定 LCD 显示路径和刷新策略。
- ⏳ 未完成：增加超时、错误状态、缓冲溢出和恢复机制。

## 八、后续 AI 修改约束

- 必须以 `F4Camera.ioc` 为外设配置源，优先通过 CubeMX 修改并重新生成。
- 不得直接修改 CubeMX 生成文件的非 `USER CODE` 区域。
- 自定义驱动放入 `BSP`，应用逻辑不要继续堆入 `main.c`。
- LCD/FSMC 初始化由 `MX_FSMC_Init()` 负责，LCD BSP 不重复初始化 FSMC。
- 不得把“配置存在”或“编译通过”写成“硬件验证成功”。
- 在用户提供实际显示、串口或采集结果后，才能更新对应硬件状态。
- 新增或修改的中文内容使用 UTF-8 编码。

## 下一次 AI 接手要求

**下一次AI接手时必须先阅读本文件**
