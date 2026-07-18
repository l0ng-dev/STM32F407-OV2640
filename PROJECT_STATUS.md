# F4Camera 工程状态

> 最后更新：2026-07-18
> 工程平台：STM32F407ZGT6 + OV2640 Camera + 正点原子 2.8 寸 LCD  
> 状态标记：✅ 已完成｜⚠️ 部分完成｜⏳ 未完成

本文档用于记录当前仓库的真实工程状态，供后续开发和新的 AI 会话接手使用。配置存在、编译通过与硬件验证是三个不同层级，未进行实机验证的功能不得标记为硬件完成。

## 1. 项目当前阶段

当前阶段：**v1.2.0 Camera 双模式功能开发完成并通过实机验证。**

## v1.2.0

发布日期：2026-07-18

### 新增

- ✅ RGB565 LCD 预览模式：OV2640 输出 320×240 RGB565，DCMI/DMA 直接写入 FSMC LCD GRAM。
- ✅ JPEG XCAM 传输模式：OV2640 输出 320×240 JPEG，64 KB SRAM 缓存完成 SOI/EOI 解析，并通过 USART2 分块发送。
- ✅ 按键模式切换：WKUP（PA0）进入 RGB565 LCD 模式，KEY0（PE4）进入 JPEG XCAM 模式，并提供 30 ms 软件消抖。
- ✅ Camera Application 状态管理：新增 `Application/Camera/camera_app.c/.h`，集中管理 `IDLE`、`RGB565_LCD`、`JPEG_XCAM` 三种状态。
- ✅ USART1 日志系统优化：统一模块化日志格式；`DEBUG_VERBOSE=0` 默认仅输出初始化、模式切换和错误，设为 `1` 时恢复逐帧调试信息。

### 实现与验证

- ✅ 保留 v1.1.0 的 320×240 RGB565 LCD 实时预览，并通过 WKUP 启动验证。
- ✅ JPEG 帧完成后解析 SOI `FF D8` 和 EOI `FF D9`，USART2 按 1024 字节分块发送；发送完成前不启动下一帧。
- ✅ KEY0 可进入 JPEG XCAM 模式，PC 端 XCAM 能够显示 JPEG 图像。
- ✅ 模式切换按“停止 DCMI/DMA → 切换 OV2640 输出 → 配置 DCMI JPEG 位 → 完整重配 DMA → 启动 Snapshot”执行。
- ✅ RGB565 与 JPEG 模式分别使用独立 DMA 参数，切换过程中未出现 DMA 冲突、HardFault、DCMI BUSY 或 LCD 花屏。
- ✅ USART1 仅用于调试输出，USART2 专用于 JPEG 二进制数据传输。
- ✅ `main.c` 只保留外设初始化、`CameraApp_Init()`、`CameraApp_Process()` 和 HAL 回调转发。

### 验证平台

- STM32F407ZGTx
- OV2640
- FSMC LCD
- USART2 921600 baud
- Keil ArmClang AC6

### 编译

- 0 Error
- 0 Warning
- Code=36976、RO-data=19800、RW-data=16、ZI-data=67816 bytes

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
- ✅ 已完成：OV2640 软件复位、Sensor/DSP Bank 配置、UXGA 基础初始化表和 RGB565 输出格式迁移。
- ✅ 已完成：OV2640 完整初始化流程的 Keil 编译验证。
- ✅ 已完成：SCCB 初始化失败的软件修复：先执行 OV2640 硬件复位，再初始化 SCCB，并将 PD6/PD7 默认状态设为释放高电平；Keil 全量编译为 0 Error、0 Warning。
- ✅ 已完成：实机读取 OV2640 PID `0x26`、VER `0x42`，串口输出 `OV2640 Init OK`。
- ✅ 已完成：实现 160×120 RGB565、DCMI snapshot、DMA normal 到内部 SRAM 的最小单帧采集软件流程。
- ✅ 已完成：实机串口输出 `DCMI Frame OK`，确认 DCMI/DMA 单帧完成链路正常。
- ✅ 已完成：将 160×120 RGB565 帧缓冲通过现有 `lcd_color_fill()` 居中写入 LCD，Keil 全量编译通过。
- ✅ 已完成：LCD 显示功能代码，支持将 QQVGA 160×120 RGB565 帧缓冲居中写入 LCD；写屏位于主循环，不在 DCMI 中断中刷新 LCD。
- ✅ 已完成：RGB565 图像已通过 LCD 实机显示验证。
- ✅ 已完成：放弃 JPEG + USART2 路线，将 OV2640 恢复为 RGB565 初始化输出，并将 DCMI 恢复为 `DCMI_JPEG_DISABLE`。
- ✅ 已完成：实现 QQVGA 160×120 Snapshot 循环采集；每帧完成后由主循环刷新 LCD，刷新完成后启动下一帧。
- ✅ 已完成：RGB565 实时预览软件通过 Keil AC6 全量编译，结果为 0 Error、0 Warning。
- ✅ 已完成：OV2640 输出尺寸提升到 320×240，DCMI DMA 直接写入 FSMC LCD GRAM。
- ✅ 已完成：LCD 初始化完成后，根据 LCD 控制器 ID 将 FSMC 扩展写时序由 9/9 优化为 3/3。
- ✅ 已完成：320×240 RGB565 实时 LCD 全屏预览已通过硬件验证。

### 未完成

- ⏳ 未完成：实际帧率统计和长时间运行稳定性量化记录。

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
FSMC 初始化写时序：AddressSetupTime = 9，DataSetupTime = 9。LCD 控制器初始化完成后，ILI9341、ST7789、ST7796、NT35310 和 NT35510 动态切换为 AddressSetupTime = 3、DataSetupTime = 3；其他控制器保持初始化时序。

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

当前关键调用顺序为：

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
CameraApp_Init()
  ├── OV2640/SCCB 初始化
  └── lcd_init() 与 LCD 测试内容
while (1): CameraApp_Process()
```

`MX_FSMC_Init()` 在 `CameraApp_Init()` 内部的 `lcd_init()` 之前执行，满足 LCD 访问前必须先建立 FSMC 总线的要求。

#### 测试内容与结果

当前 LCD 测试内容由 `CameraApp_Init()` 调用，包含：

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
- ✅ 已完成：AXF 固件已生成。
- ✅ 已完成：HEX 固件已生成。

当前构建尺寸：

| 类型 | 大小 |
|---|---:|
| Code | 31576 bytes |
| RO-data | 19348 bytes |
| RW-data | 16 bytes |
| ZI-data | 40608 bytes |
| Total ROM | 50940 bytes（49.75 KiB） |
| Total RW | 40624 bytes（39.67 KiB） |

### SCCB 通信模块

#### 软件实现

- ✅ 已完成：建立 `BSP/SCCB/`，包含 `sccb.c` 和 `sccb.h`。
- ✅ 已完成：实现 SCCB 总线初始化、单字节寄存器写和单字节寄存器读。
- ✅ 已完成：使用 `main.h` 中的 `SCCB_SCL`（PD6）和 `SCCB_SDA`（PD7）引脚定义。
- ✅ 已完成：SCCB 初始化仅设置总线电平，不重新配置 CubeMX GPIO。
- ✅ 已完成：提供无效参数、NACK 和总线超时状态返回。
- ✅ 已完成：加入 Keil 的 `BSP/SCCB` 分组并通过 ArmClang V6.24 全量编译，结果为 0 Error、0 Warning。

#### 验证范围

- ✅ 已完成：SCCB ACK、PID/VER 寄存器读取和完整初始化写入流程已通过实机运行验证。
- 实机串口已输出 PID `26`、VER `42` 和 `OV2640 Init OK`。
- SCCB 层不包含 OV2640 器件逻辑；PD6/PD7 默认释放为高电平，PD7 保持开漏输出。

### OV2640 驱动与完整初始化模块

#### 软件实现

- ✅ 已完成：建立 `BSP/OV2640/`，包含 `ov2640.c` 和 `ov2640.h`。
- ✅ 已完成：通过现有 `BSP/SCCB` 接口实现 OV2640 单寄存器读写封装。
- ✅ 已完成：使用 `main.h` 中的 `OV_PWDN` 和 `OV_RESET` 引脚定义执行上电退出和硬件复位时序，不重新初始化 GPIO。
- ✅ 已完成：读取 Sensor Bank 的 PID（`0x0A`）和 VER（`0x0B`），期望值分别为 `0x26` 和 `0x42`。
- ✅ 已完成：SCCB 初始化和每次寄存器访问均检查返回值；摄像头不存在时打印错误并继续进入主循环，不进入错误死循环。
- ✅ 已完成：`printf` 重定向到 USART1，输出 PID、VER 或带状态码的错误信息。
- ✅ 已完成：ArmClang V6.24 全量编译通过，结果为 0 Error、0 Warning。
- ✅ 已完成：将硬件复位接口独立为 `OV2640_HardwareReset()`，由主流程在 `SCCB_Init()` 前调用；`OV2640_Init()` 不再重复硬件复位。
- ✅ 已完成：软件复位通过 Sensor Bank 的 COM7（`0x12 = 0x80`）执行，并等待 50 ms。
- ✅ 已完成：迁移正点原子实验24的 UXGA 基础初始化表，共 178 组 Sensor/DSP 寄存器配置。
- ✅ 已完成：迁移 RGB565 输出格式表，共 14 组配置；当前只对外验证 RGB565 模式。
- ✅ 已完成：每一项表写入均检查 SCCB 返回值，遇到首个 NACK 或超时立即停止并返回错误。
- ✅ 已完成：主流程按 `OV2640_HardwareReset()` → `SCCB_Init()` → `OV2640_Init()` → PID/VER 复读顺序执行，并输出 `OV2640 Init OK` 或 `OV2640 Init Failed`。

#### 验证范围

- ✅ 已完成：硬件复位、PID/VER 读取、UXGA 基础表和 RGB565 输出格式初始化已通过实机运行验证。
- 本阶段未修改 SCCB、OV2640 寄存器初始化表、LCD 或 USART1 调试实现。

### DCMI/DMA 最小单帧采集

- OV2640 通过独立运行时接口将 DSP 输出设置为 QQVGA 160×120，RGB565 每帧为 38,400 字节。
- 静态帧缓冲区类型为 `uint32_t[9600]`，链接地址为 `0x200000D4`，位于 DMA 可访问的内部 SRAM。
- DCMI 保持硬件同步、8 bit、全帧、PCLK 上升沿、HSYNC/VSYNC 低有效和 `DCMI_JPEG_DISABLE`。
- DMA 使用 DMA2 Stream1 / Channel 1、外设到内存、地址递增、外设和内存 32 位对齐、Normal 模式、Very High 优先级。
- 使用 `HAL_DCMI_Start_DMA(..., DCMI_MODE_SNAPSHOT, ..., 9600)` 启动一次采集；帧回调只设置标志，主循环输出 `DCMI Frame OK`。
- ✅ 已完成：实机串口输出 `DCMI Frame OK`，确认单帧完成中断和 DMA 传输流程正常。

### LCD 单帧画面显示

- 帧完成回调仍只设置标志，不在中断上下文中访问 FSMC 或打印串口。
- 主循环检测到完整帧后，将 `uint32_t[9600]` 缓冲按 RGB565 16 位像素传给现有 `lcd_color_fill()`。
- 160×120 画面根据 `lcddev.width`、`lcddev.height` 计算起点，并以原始尺寸居中显示，不做缩放。
- 写屏完成后 USART1 输出 `LCD Frame Drawn`，用于确认程序已执行到 LCD GRAM 写入完成位置。
- ✅ 已完成：LCD 显示代码和RGB565摄像头画面实机验证。

#### 第六阶段代码修改记录

修改文件：`Core/Src/main.c`

- 收到完整 DCMI 帧后调用 `lcd_color_fill()`。
- 将 160×120 RGB565 图像显示到 LCD 中央区域。
- FSMC 写屏操作放在主循环中执行。
- DCMI 帧中断只设置通知标志，不在中断中执行 LCD 刷新。
- 增加 `LCD Frame Drawn` USART1 调试输出。

该实现保持中断快速响应原则：DCMI 中断只负责通知主循环，完整帧的 LCD 刷新由主循环处理。

#### 第六阶段当前验证状态

| 验证项 | 结果 |
|---|---|
| OV2640 PID | ✅ `0x26` |
| OV2640 VER | ✅ `0x42` |
| OV2640 初始化 | ✅ 成功 |
| DCMI Snapshot | ✅ 实机验证成功 |
| DMA Normal 单帧搬运 | ✅ 实机验证成功 |
| FSMC/LCD 通信 | ✅ 正常 |
| LCD 基础显示 | ✅ 正常 |
| 摄像头 RGB565 画面显示代码 | ✅ 已完成 |
| 摄像头 RGB565 画面实际显示 | ✅ 实机验证成功 |
| USART1 已有调试输出 | ✅ 正常 |

LCD 显示验证时的预期串口输出：

```text
OV2640 PID: 26
OV2640 VER: 42
OV2640 Init OK
DCMI Frame OK
LCD Frame Drawn
```

#### 第六阶段当前结论

当前软件目标链路：

```text
OV2640
↓
SCCB 初始化
↓
DCMI 采集
↓
DMA 搬运
↓
RGB565 Frame Buffer
↓
FSMC LCD 显示
```

状态：✅ DCMI/DMA和RGB565摄像头画面LCD显示均已通过硬件验证。

#### 第六阶段保持未修改记录

- SCCB 底层通信未修改。
- OV2640 寄存器配置表未修改。
- DCMI 基础配置未修改。
- LCD 底层驱动未修改。
- USART2 JPEG 发送未修改。
- JPEG 模块未修改。

## 3. 当前工程结构

当前实际存在的主要工程目录和文件：

```text
F4Camera/
├── Application/
│   ├── Camera/
│   │   ├── camera_app.c
│   │   └── camera_app.h
│   └── JPEG/
│       ├── jpeg.c
│       └── jpeg.h
├── BSP/
│   ├── Camera/
│   │   ├── camera.c
│   │   └── camera.h
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

Camera 模式管理位于 `Application/Camera/`，JPEG 缓存与解析位于 `Application/JPEG/`，DCMI/DMA 采集适配位于 `BSP/Camera/`。

## 4. CubeMX 配置状态

| 模块 | 状态 | 当前配置与验证范围 |
|---|---|---|
| RCC | ✅ 已完成 | HSE = 8 MHz，SYSCLK/HCLK = 168 MHz，APB1 = 42 MHz，APB2 = 84 MHz；已支持当前程序和 LCD 实机运行。 |
| GPIO | ✅ 已完成 | 已生成 FSMC、LCD 背光、DCMI、SCCB、OV2640 控制和 USART 引脚配置；SCCB、OV2640 控制、DCMI 数据与同步引脚均已通过 SCCB 通信或 DCMI/DMA 采集实机验证。 |
| FSMC | ✅ 已完成 | Bank1 NOR/SRAM4、16 bit、NE4/A6、Extended Mode；已完成 LCD 实机验证。 |
| USART1 | ✅ 已完成 | PA9/PA10，115200，TX/RX；已通过实机串口输出 PID、VER 和 OV2640 初始化状态。 |
| USART2 | ✅ 已完成 | PA2/PA3，921600，HAL TX；有效 JPEG 原始字节流分块发送和 XCAM 显示已通过实机验证。 |
| DCMI | ✅ 已完成 | 硬件同步、8 bit、全帧、PCLK 上升沿、HSYNC/VSYNC 低有效；运行时按 RGB565/JPEG 模式切换 JPEGMode，双模式均已通过实机验证。 |
| DMA | ✅ 已完成 | DCMI 使用 DMA2 Stream1 / Channel 1、Normal 模式、Very High 优先级；RGB565 使用目标 HalfWord/不递增，JPEG 使用目标 Word/递增。 |
| TIM6 | ⚠️ 部分完成 | 已生成约 1 Hz 的基础定时器配置，当前应用未启动使用。 |

DCMI/DMA 已形成 OV2640 → DCMI → DMA → 内部 SRAM → FSMC LCD 的采集显示链路，并已通过RGB565单帧画面实机验证。

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
- ✅ 已完成：OV2640 SCCB 通信、硬件复位、PID `0x26`、VER `0x42` 和 RGB565 寄存器初始化正常。
- ✅ 已完成：QQVGA 160×120 DCMI Snapshot 与 DMA Normal 单帧采集正常，串口输出 `DCMI Frame OK`。
- ✅ 已完成：320×240 RGB565、DCMI Snapshot、DMA Normal 直接写 FSMC LCD GRAM 的全屏实时预览正常。
- ✅ 已完成：320×240 JPEG 采集、SOI/EOI 解析、USART2 921600 分块发送正常。
- ✅ 已完成：PC/XCAM 能够接收并显示 JPEG 图像。
- ✅ 已完成：WKUP/KEY0 可分别进入 RGB565 LCD 与 JPEG XCAM 模式。

### 未完成

- ⏳ 未完成：实际帧率和长时间连续运行稳定性的量化记录。

## 6. 当前 Git 里程碑

### Milestone

**v1.2.0 RGB565 LCD + JPEG XCAM 双模式完成并通过硬件验证**

完成内容：

- CubeMX HAL 工程与正点原子 LCD 驱动融合。
- FSMC Bank1 NOR/SRAM4、NE4/A6 和 16 位数据总线配置完成。
- LCD BSP 加入 Keil 工程并通过 Arm Compiler 6 构建。
- 完成第一次 LCD/FSMC 硬件验证。
- Camera Application 状态机、JPEG 采集和 USART2 XCAM 传输完成。
- WKUP/KEY0 双模式切换及 USART1 日志控制完成。

当前 Git 状态：

| 项目 | 当前值 |
|---|---|
| 分支 | `main` |
| 里程碑基线 commit | `af97209`（v1.1.0） |
| 发布提交 | 由 `v1.2.0` 标签指向本次发布提交 |
| 版本标签 | `v1.0.0`、`v1.1.0`（保留）、`v1.2.0`（当前版本） |
| Release | `v1.2.0 Camera Dual Mode Release` |
| 里程碑状态 | ✅ 已完成 |

`af97209` 是 v1.1.0 RGB565 LCD 实时预览发布基线；v1.2.0 在该基线上增加 JPEG XCAM 传输、按键模式切换、Camera Application 状态管理和 USART1 日志控制。

SCCB 阶段在 `BSP/SCCB/` 中新增独立通信模块，并将其接入 Keil 工程；CubeMX 配置和生成代码保持不变。

OV2640 最小验证阶段新增 `BSP/OV2640/`，并在 `main.c` 的 USER CODE 区接入 USART1 输出。

OV2640 完整初始化阶段在现有 `BSP/OV2640/` 内扩展初始化表和错误返回。

SCCB 初始化失败修复阶段记录：实机曾输出 `OV2640 ERROR: SCCB init failed (3)`，其中状态码 3 为总线超时。将 PD6/PD7 默认输出状态改为高电平，并把硬件复位移动到 `SCCB_Init()` 之前后，实机已成功输出 PID `26`、VER `42` 和 `OV2640 Init OK`。

第六阶段开发记录：收到完整 DCMI 帧后，由帧回调仅设置通知标志，主循环调用 `lcd_color_fill()` 将 QQVGA 160×120 RGB565 帧缓冲居中写入 LCD。DCMI/DMA单帧采集和RGB565摄像头画面LCD显示均已通过实机验证。

## 7. 下一步计划

### 第六阶段：LCD 摄像头画面实机验证（已完成）

当前状态：

- ✅ 已完成：DCMI/DMA 单帧采集硬件验证，串口能够输出 `DCMI Frame OK`。
- ✅ 已完成：LCD 显示代码，支持 160×120 RGB565 居中显示，并在主循环执行 FSMC 写屏。
- ✅ 已完成：LCD中央区域能够实际显示RGB565摄像头画面。
- ✅ 已完成：确认完成写屏后串口输出 `LCD Frame Drawn`。

### Phase 7：320×240 RGB565 实时 LCD 全屏预览（已完成）

目标链路：

```text
OV2640
↓
DCMI Snapshot
↓
DMA2 Stream1 / Channel 1
↓
FSMC LCD GRAM
↓
320×240 LCD 全屏显示
```

当前实现策略：

- OV2640 输出 320×240 RGB565。
- 使用 `DCMI_MODE_SNAPSHOT` 和 DMA Normal。
- DCMI帧回调只设置完成标志。
- DMA 运行时配置为外设 Word、目标 HalfWord、目标地址不递增，直接写入 LCD GRAM。
- 主循环在每帧完成后启动下一次 Snapshot。
- 暂不使用 Continuous、Circular 或双缓冲。

## 8. 保留问题与设计记录

- 历史记录：v1.1.0 曾暂停 JPEG 路线；v1.2.0 已恢复 JPEG + USART2 + XCAM，并以双模式保留 RGB565 实时预览。
- CubeMX 默认配置保持 `DCMI_JPEG_DISABLE`；CameraApp 在运行时按 RGB565/JPEG 模式动态切换 DCMI JPEGMode。
- ✅ 已完成：64 KB JPEG 缓存、DMA 实际长度计算、SOI/EOI 解析及溢出保护已通过实机采集验证。
- ✅ 已完成：USART2 当前为 921600 baud、TX 模式，采用阻塞式 1024 字节分块发送，已通过 XCAM 显示验证；本版本不引入 USART2 DMA。
- ✅ 已完成：OV2640 模块硬件时钟来源已经确认，当前模块自带晶振，不需要由 STM32 输出 XCLK。
- ✅ 已完成：DCMI DMA 当前使用 Normal 模式和 Very High 优先级，320×240 RGB565 Snapshot 循环采集与 LCD 全屏显示已通过实机验证。
- 历史记录：160×120 阶段使用 38,400 字节内部 SRAM 帧缓冲；当前 320×240 路线由 DMA 直接写 LCD GRAM，不再占用该帧缓冲。
- ✅ 已完成：当前 DCMI PCLK、HSYNC、VSYNC 极性以及 OV2640 → DCMI → DMA → FSMC LCD GRAM 完整链路已通过硬件验证。

## 后续维护约束

- 外设配置以 `F4Camera.ioc` 为源，优先通过 CubeMX 修改并重新生成。
- CubeMX 生成文件只在 `USER CODE` 区域加入自定义代码。
- 自定义外设驱动放入 `BSP/`，应用逻辑后续放入独立应用层，不继续堆入 `main.c`。
- LCD/FSMC 初始化由 `MX_FSMC_Init()` 统一管理，LCD BSP 不得重复初始化 FSMC。
- 不得把“配置存在”或“编译通过”写成“硬件验证成功”。
- 所有新增中文内容使用 UTF-8 编码保存。

## 9. JPEG采集模块阶段（已完成并通过硬件验证）

> 历史记录：该路线在 v1.1.0 曾暂停；v1.2.0 已重新接入 CameraApp，64 KB JPEG 缓存已链接进当前固件。

- ✅ 已完成：新增 `BSP/Camera`，复用CubeMX生成的 `hdcmi`、`hdma_dcmi`，使用Snapshot、DMA Normal和Word传输启动采集。
- ✅ 已完成：新增 `Application/JPEG`，提供64KB、4字节对齐的静态缓存及SOI `FF D8`、EOI `FF D9`解析。
- ✅ 已完成：通过DMA NDTR计算原始接收字节数，并实现缓存溢出、DCMI错误、DMA错误、无SOI和无EOI错误码。
- ✅ 已完成：Keil ArmClang AC6全量编译为0 Error、0 Warning；MAP确认缓存位于 `0x200002E0` 的普通SRAM1，不在CCM。
- ✅ 已完成：320×240 JPEG 实机采集、有效长度解析和错误检测验证。
- ✅ 已完成：USART2 有效 JPEG 原始字节流分块发送的软件实现。
- ✅ 已完成：PC 端 XCAM 实际显示验证。

当前阶段数据链路：

```text
OV2640 JPEG → DCMI JPEG → DMA2 Stream1/Channel1 → 64 KB SRAM Buffer → SOI/EOI 解析 → USART2 → XCAM
```

## 10. RGB565实时LCD预览阶段

- ✅ 已完成：RGB565 模式执行已验证的 `OV2640_Init()`；切换 JPEG 模式时调用 `OV2640_SetJPEGMode()`，切回 RGB565 时恢复原配置。
- ✅ 已完成：`F4Camera.ioc` 与 `Core/Src/dcmi.c` 保持默认 `DCMI_JPEG_DISABLE`，运行时由 CameraApp 按模式切换。
- ✅ 已完成：保持DMA2 Stream1 / Channel 1、Normal模式，运行时配置为外设 Word、目标 HalfWord和目标地址不递增。
- ✅ 已完成：首次Snapshot在LCD初始化完成后启动，避免LCD初始化阶段与采集流程交叠。
- ✅ 已完成：统一 DCMI 帧回调只向 CameraApp 转发事件，不在中断中执行 FSMC 写屏或串口打印。
- ✅ 已完成：DMA直接写FSMC LCD GRAM，主循环收到完整帧通知后启动下一次Snapshot。
- ✅ 已完成：LCD初始化完成后按控制器ID将FSMC写时序从9/9动态优化到3/3。
- ✅ 已完成：Keil ArmClang AC6全量编译为0 Error、0 Warning；Code=32444、RO-data=19340、RW-data=16、ZI-data=2208 bytes。
- ✅ 已完成：320×240 RGB565实时LCD全屏预览已通过硬件验证。
- ⏳ 未完成：统计实际帧率并记录长时间运行稳定性。

## 下一次 AI 接手要求

**下一次AI接手时必须先阅读本文件。**
