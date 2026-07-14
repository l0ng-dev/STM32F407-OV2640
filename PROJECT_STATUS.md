# F4Camera 工程状态

> 最后更新：2026-07-14  
> 工程平台：STM32F407ZGT6 + OV2640 Camera + 正点原子 2.8 寸 LCD  
> 状态标记：✅ 已完成｜⚠️ 部分完成｜⏳ 未完成｜❌ 存在问题

本文档用于记录当前仓库的真实工程状态，供后续开发和新的 AI 会话接手使用。配置存在、编译通过与硬件验证是三个不同层级，未进行实机验证的功能不得标记为硬件完成。

## 1. 项目当前阶段

当前阶段：**OV2640 最小 SCCB 通信验证代码已完成并通过编译，等待硬件读取 PID/VER。**

### 已完成

- ✅ 已完成：创建 STM32CubeMX HAL 工程，目标器件为 STM32F407ZGT6。
- ✅ 已完成：FSMC/LCD 底层配置。
- ✅ 已完成：正点原子 LCD BSP 迁移。
- ✅ 已完成：LCD BSP 加入 Keil 工程并配置头文件搜索路径。
- ✅ 已完成：Keil Arm Compiler 6 编译和链接验证。
- ✅ 已完成：LCD 实机显示验证。
- ✅ 已完成：SCCB 通信模块迁移及 Keil 编译验证。
- ✅ 已完成：OV2640 最小寄存器访问与 PID/VER 读取流程迁移。
- ✅ 已完成：USART1 `printf` 调试输出接入及 Keil 编译验证。

### 未完成

- ⏳ 未完成：SCCB 硬件通信验证。
- ⏳ 未完成：OV2640 PID/VER 实机读取验证。
- ⏳ 未完成：OV2640 完整寄存器初始化。
- ⏳ 未完成：DCMI 实际采集流程。
- ⏳ 未完成：DMA 图像缓存设计与采集启动。
- ⏳ 未完成：JPEG 数据处理。
- ⏳ 未完成：USART 图像输出。

## 2. 已完成模块记录

### LCD/FSMC 模块（迁移完成并通过硬件验证）

#### 硬件与总线配置

| 项目 | 当前配置 |
|---|---|
| MCU | STM32F407ZGT6，LQFP144 |
| LCD | 正点原子 2.8 寸并口 LCD |
| FSMC Bank | FSMC Bank1 NOR/SRAM4，即 NE4 地址空间 |
| 存储器模式 | NOR/SRAM 控制器，SRAM 类型，异步访问 |
| 数据宽度 | 16 bit |
| 片选 | PG12 = FSMC_NE4 |
| 寄存器选择 | PF12 = FSMC_A6 |
| 扩展模式 | Enabled，读写时序分离 |

FSMC 读时序：AddressSetupTime = 15，DataSetupTime = 60。  
FSMC 写时序：AddressSetupTime = 9，DataSetupTime = 9。

#### 地址映射

LCD 使用 NE4/A6 映射，16 位总线下 A6 对应字节地址偏移 `0x80`：

```text
LCD_REG = 0x6C00007E
LCD_RAM = 0x6C000080
```

`LCD_REG` 和 `LCD_RAM` 由 `BSP/LCD/lcd.h` 中的 `LCD_TypeDef` 结构体映射。结构体基地址为 `0x6C00007E`，第二个 16 位成员对应 `0x6C000080`。

#### 软件职责

- ✅ 已完成：CubeMX 生成的 `MX_FSMC_Init()` 负责 FSMC 外设、GPIO 和访问时序初始化。
- ✅ 已完成：LCD BSP 负责 LCD 寄存器访问、控制器识别、显示方向、绘图和字符显示。
- ✅ 已完成：旧工程 LCD BSP 内重复的 `g_sram_handle`、`HAL_SRAM_MspInit()` 和 FSMC 初始化代码已经移除。
- ✅ 已完成：`lcd.c`、`lcd_ex.c`、`lcd.h`、`lcd_ex.h`、`lcdfont.h` 已加入当前工程。

注意：CubeMX 生成的 `Core/Src/fsmc.c` 仍通过 `HAL_SRAM_Init(&hsram4, ...)` 完成 FSMC 初始化，这是当前正确且唯一的初始化入口；删除的是旧 LCD BSP 内的重复初始化代码，并非删除 HAL 的正常调用。

#### 初始化顺序

当前 `main.c` 的关键调用顺序为：

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

`MX_FSMC_Init()` 在 `lcd_init()` 之前执行，满足 LCD 访问前必须先建立 FSMC 总线的要求。

#### 测试内容与结果

当前 `main.c` 的 `USER CODE BEGIN 2` 区域包含：

- 白色清屏。
- 显示 `LCD TEST`。
- 显示 `FSMC OK`。
- 显示 `lcddev.id`。
- 绘制红、绿、蓝三个色块。

实机验证结果：

- ✅ 已完成：LCD 正常初始化。
- ✅ 已完成：LCD ID 读取成功；当前文档未记录具体十六进制 ID 值。
- ✅ 已完成：字符显示正常。
- ✅ 已完成：红、绿、蓝色块显示正常。
- ✅ 已完成：FSMC NE4/A6 通信正常。

### Keil 工程接入

- ✅ 已完成：`lcd.c` 和 `lcd_ex.c` 位于 Keil 的 `BSP/LCD` 分组并参与编译。
- ✅ 已完成：Include Path 包含 `..\BSP\LCD`。
- ✅ 已完成：目标器件为 `STM32F407ZGTx`。
- ✅ 已完成：使用 ArmClang V6.24（ARM Compiler 6）。
- ✅ 已完成：当前构建结果为 0 Error、0 Warning。

当前构建尺寸：

| 类型 | 大小 |
|---|---:|
| Code | 28704 bytes |
| RO-data | 18820 bytes |
| RW-data | 16 bytes |
| ZI-data | 2200 bytes |
| Total ROM | 47540 bytes（46.43 KiB） |
| Total RW | 2216 bytes（2.16 KiB） |

### SCCB 通信模块

#### 软件实现

- ✅ 已完成：建立 `BSP/SCCB/`，包含 `sccb.c` 和 `sccb.h`。
- ✅ 已完成：实现 SCCB 总线初始化、单字节寄存器写和单字节寄存器读。
- ✅ 已完成：使用 `main.h` 中的 `SCCB_SCL`（PD6）和 `SCCB_SDA`（PD7）引脚定义。
- ✅ 已完成：SCCB 初始化仅设置总线电平，不重新配置 CubeMX GPIO。
- ✅ 已完成：提供无效参数、NACK 和总线超时状态返回。
- ✅ 已完成：加入 Keil 的 `BSP/SCCB` 分组并通过 ArmClang V6.24 全量编译，结果为 0 Error、0 Warning。

#### 验证范围

- 当前只完成软件迁移和编译验证。
- 尚未在 OV2640 硬件上验证 SCCB ACK、寄存器读写和总线波形。
- SCCB 层不包含 OV2640 器件逻辑；DCMI、DMA 和 CubeMX 生成的 GPIO 初始化未修改。

### OV2640 最小通信验证模块

#### 软件实现

- ✅ 已完成：建立 `BSP/OV2640/`，包含 `ov2640.c` 和 `ov2640.h`。
- ✅ 已完成：通过现有 `BSP/SCCB` 接口实现 OV2640 单寄存器读写封装。
- ✅ 已完成：使用 `main.h` 中的 `OV_PWDN` 和 `OV_RESET` 引脚定义执行上电退出和硬件复位时序，不重新初始化 GPIO。
- ✅ 已完成：读取 Sensor Bank 的 PID（`0x0A`）和 VER（`0x0B`），期望值分别为 `0x26` 和 `0x42`。
- ✅ 已完成：SCCB 初始化和每次寄存器访问均检查返回值；摄像头不存在时打印错误并继续进入主循环，不进入错误死循环。
- ✅ 已完成：`printf` 重定向到 USART1，输出 PID、VER 或带状态码的错误信息。
- ✅ 已完成：ArmClang V6.24 全量编译通过，结果为 0 Error、0 Warning。

#### 验证范围

- 当前只完成最小驱动、测试流程和编译验证，尚未连接硬件确认 PID/VER 返回值。
- 本阶段没有导入完整 OV2640 初始化表，没有启动 DCMI/DMA，也没有实现 JPEG、LCD 图像显示或 USART 图像发送。
- `Core/Src/gpio.c`、`Core/Src/dcmi.c`、`Core/Src/dma.c` 和 `F4Camera.ioc` 均未因本阶段修改。

## 3. 当前工程结构

当前实际存在的主要工程目录和文件：

```text
F4Camera/
├── BSP/
│   ├── LCD/
│   │   ├── lcd.c
│   │   ├── lcd.h
│   │   ├── lcd_ex.c
│   │   ├── lcd_ex.h
│   │   └── lcdfont.h
│   ├── SCCB/
│   │   ├── sccb.c
│   │   └── sccb.h
│   └── OV2640/
│       ├── ov2640.c
│       └── ov2640.h
├── Core/
│   ├── Inc/
│   └── Src/
├── Drivers/
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── MDK-ARM/
│   ├── F4Camera.uvprojx
│   ├── F4Camera.uvoptx
│   ├── startup_stm32f407xx.s
│   └── F4Camera/                  # 本地 Keil 构建输出，Git 已忽略
├── F4Camera.ioc
├── Docs/
│   └── OV2640_SCCB通信验证报告.md
├── README.md
└── PROJECT_STATUS.md
```

目录职责：

- `Core/`：CubeMX 生成的初始化代码、HAL MSP、异常与中断入口，以及当前主程序。
- `BSP/LCD/`：正点原子 LCD 板级驱动、控制器扩展初始化和字库。
- `BSP/SCCB/`：基于 CubeMX 现有 GPIO 的 SCCB 通信驱动。
- `BSP/OV2640/`：OV2640 最小寄存器访问、硬件复位和 PID/VER 读取驱动。
- `Drivers/`：STM32F4 HAL 驱动与 CMSIS 设备支持文件。
- `MDK-ARM/`：Keil 工程、启动文件和本地构建输出。
- `F4Camera.ioc`：CubeMX 外设、引脚和时钟配置源文件。

当前不存在独立的 `Application/` 或 `BSP/JPEG/` 目录。

## 4. CubeMX 配置状态

| 模块 | 状态 | 当前配置与验证范围 |
|---|---|---|
| RCC | ✅ 已完成 | HSE = 8 MHz，SYSCLK/HCLK = 168 MHz，APB1 = 42 MHz，APB2 = 84 MHz；已支持当前程序和 LCD 实机运行。 |
| GPIO | ✅ 已完成 | 已生成 FSMC、LCD 背光、DCMI、SCCB 预留、OV2640 控制和 USART 引脚配置。摄像头相关引脚尚未进行通信验证。 |
| FSMC | ✅ 已完成 | Bank1 NOR/SRAM4、16 bit、NE4/A6、Extended Mode；已完成 LCD 实机验证。 |
| USART1 | ⚠️ 部分完成 | PA9/PA10，115200，TX/RX；已用于 OV2640 最小验证的 `printf` 调试输出，尚未进行实机串口验证。 |
| USART2 | ⚠️ 部分完成 | PA2/PA3，921600，当前 HAL 模式为 TX；尚无图像协议和发送代码。 |
| DCMI | ⚠️ 部分完成 | 硬件同步、8 bit、全帧、PCLK 上升沿，当前 `JPEGMode = DISABLE`；仅有 CubeMX 初始化代码。 |
| DMA | ⚠️ 部分完成 | DCMI 使用 DMA2 Stream1 / Channel 1、循环模式、Very High 优先级；仅有 CubeMX 配置和 HAL 链接。 |
| TIM6 | ⚠️ 部分完成 | 已生成约 1 Hz 的基础定时器配置，当前应用未启动使用。 |

DCMI/DMA 目前只是 CubeMX 配置和生成代码存在。工程中没有 `HAL_DCMI_Start_DMA()`、图像缓冲区、帧完成处理或错误回调，因此尚未形成 OV2640 → DCMI → DMA → 内存的数据链路。

## 5. 硬件状态

### 硬件组成

- 开发板：STM32F407ZGT6。
- 显示模块：正点原子 2.8 寸 LCD。
- 摄像头：OV2640 摄像头模块。
- 调试器：ST-Link。
- 通信工具：USB 串口工具。

### 已验证

- ✅ 已完成：MCU 程序能够正常运行。
- ✅ 已完成：FSMC NE4/A6 与 LCD 通信正常。
- ✅ 已完成：LCD 初始化、ID 读取、字符显示和 RGB 色块显示正常。

### 未验证

- ⏳ 未完成：OV2640 SCCB 通信。
- ⏳ 未完成：OV2640 ID 读取。
- ⏳ 未完成：DCMI 图像采集。
- ⏳ 未完成：DMA 图像缓存。
- ⏳ 未完成：JPEG 输出。
- ⏳ 未完成：USART 图像传输和 PC 端显示。

## 6. 当前 Git 里程碑

### Milestone

**LCD/FSMC 迁移完成并通过硬件验证**

完成内容：

- CubeMX HAL 工程与正点原子 LCD 驱动融合。
- FSMC Bank1 NOR/SRAM4、NE4/A6 和 16 位数据总线配置完成。
- LCD BSP 加入 Keil 工程并通过 Arm Compiler 6 构建。
- 完成第一次 LCD/FSMC 硬件验证。

当前 Git 状态：

| 项目 | 当前值 |
|---|---|
| 分支 | `main` |
| 里程碑基线 commit | `e7f0351` |
| 当前 HEAD | `0358ecf` |
| 版本标签 | `v1.0.0` |
| Release | `STM32F407 OV2640 Camera Project v1.0.0` |
| 里程碑状态 | ✅ 已完成 |

`e7f0351` 是 LCD/FSMC 初始工程与 `v1.0.0` 发布对应的里程碑基线；`0358ecf` 是当前分支最新提交，在该基线之后更新了项目状态文档。两者用途不同：前者用于标识里程碑起点，后者表示当前实际检出的仓库版本。

SCCB 阶段在 `BSP/SCCB/` 中新增独立通信模块，并将其接入 Keil 工程；CubeMX 配置和生成代码保持不变。

OV2640 最小验证阶段新增 `BSP/OV2640/`，并在 `main.c` 的 USER CODE 区接入 USART1 输出；当前工作区改动尚未形成新的 Git 提交。

## 7. 下一步计划

### Step 1：SCCB 通信与 OV2640 ID 硬件验证（当前目标）

目标：

- 调用 `SCCB_Init()` 释放总线到空闲状态。
- 确认 SCCB 空闲电平、应答和读写时序正常。
- 通过硬件测试确认 SCCB 总线与摄像头连接可用。
- 通过 SCCB 读取 OV2640 PID/VER 寄存器。
- 确认串口输出为 PID `26`、VER `42`；异常时结合返回状态和总线波形定位问题。

### Step 2：摄像头初始化

目标：

- 完成 OV2640 上电、PWDN、RESET 和寄存器初始化流程。
- 完成摄像头基础格式与分辨率配置。

### Step 3：验证 DCMI DMA 采集

目标：

- 设计 DMA 可访问的静态图像缓冲区。
- 启动 DCMI DMA，并验证帧完成、错误处理和实际采集数据。

### Step 4：实现 JPEG 数据处理

目标：

- 确认 OV2640 JPEG 输出和 DCMI JPEG 模式。
- 实现 JPEG 帧边界、有效长度和缓存溢出处理。

### Step 5：实现 LCD 显示和 USART 输出

目标：

- 建立采集图像到 LCD 的显示路径。
- 设计 USART 图像传输协议并完成 PC 端验证。
- 根据吞吐量评估 USART2 DMA 发送。

## 8. 保留问题与设计记录

- ❌ 存在问题：当前 DCMI 配置为 `DCMI_JPEG_DISABLE`，后续 JPEG 模式选择仍需结合 OV2640 输出格式确认。
- ⚠️ 部分完成：JPEG 缓存大小、帧最大长度、溢出处理和缓冲策略需要后续评估。
- ⚠️ 部分完成：USART2 当前为 921600 baud、TX 模式，后续需要评估 DMA 发送和分包机制。
- ✅ 已完成：OV2640 模块硬件时钟来源已经确认，当前模块自带晶振，不需要由 STM32 输出 XCLK。
- ⚠️ 部分完成：DCMI DMA 当前使用循环模式和 Very High 优先级，实际采集后需要重新评估缓存策略和中断优先级。
- ⚠️ 部分完成：DMA 图像缓冲区必须位于 DMA 可访问的 SRAM1/SRAM2，不能放入 Cortex-M4 CCM RAM。
- ⏳ 未完成：DCMI 的 PCLK、HSYNC、VSYNC 极性仍需结合 OV2640 实际输出波形验证。

## 后续维护约束

- 外设配置以 `F4Camera.ioc` 为源，优先通过 CubeMX 修改并重新生成。
- CubeMX 生成文件只在 `USER CODE` 区域加入自定义代码。
- 自定义外设驱动放入 `BSP/`，应用逻辑后续放入独立应用层，不继续堆入 `main.c`。
- LCD/FSMC 初始化由 `MX_FSMC_Init()` 统一管理，LCD BSP 不得重复初始化 FSMC。
- 不得把“配置存在”或“编译通过”写成“硬件验证成功”。
- 所有新增中文内容使用 UTF-8 编码保存。

## 下一次 AI 接手要求

**下一次AI接手时必须先阅读本文件。**
