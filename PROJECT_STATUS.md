# F4Camera 工程状态

> 最后更新：2026-07-17
> 工程平台：STM32F407ZGT6 + OV2640 Camera + 正点原子 2.8 寸 LCD  
> 状态标记：✅ 已完成｜⚠️ 部分完成｜⏳ 未完成

本文档用于记录当前仓库的真实工程状态，供后续开发和新的 AI 会话接手使用。配置存在、编译通过与硬件验证是三个不同层级，未进行实机验证的功能不得标记为硬件完成。

## 1. 项目当前阶段

当前阶段：**Camera Frame Display on LCD（第六阶段）已完成软件和硬件闭环验证。**

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
- ✅ 已完成：LCD 中央区域成功显示 OV2640 采集的 QQVGA RGB565 画面，串口输出 `LCD Frame Drawn`。

### 未完成

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
- ✅ 已完成：LCD 实机成功显示 OV2640 采集的 RGB565 画面，第六阶段已完成软件和硬件闭环验证。

#### 第六阶段代码修改记录

修改文件：`Core/Src/main.c`

- 收到完整 DCMI 帧后调用 `lcd_color_fill()`。
- 将 160×120 RGB565 图像显示到 LCD 中央区域。
- FSMC 写屏操作放在主循环中执行。
- DCMI 帧中断只设置通知标志，不在中断中执行 LCD 刷新。
- 增加 `LCD Frame Drawn` USART1 调试输出。

该实现保持中断快速响应原则：DCMI 中断只负责通知主循环，完整帧的 LCD 刷新由主循环处理。

#### 第六阶段硬件验证结果

| 验证项 | 结果 |
|---|---|
| OV2640 PID | ✅ `0x26` |
| OV2640 VER | ✅ `0x42` |
| OV2640 初始化 | ✅ 成功 |
| DCMI Snapshot | ✅ 实机验证成功 |
| DMA Normal 单帧搬运 | ✅ 实机验证成功 |
| FSMC/LCD 通信 | ✅ 正常 |
| LCD 基础显示 | ✅ 正常 |
| 摄像头 RGB565 画面显示 | ✅ 成功 |
| USART1 调试输出 | ✅ 正常 |

实机串口输出：

```text
OV2640 PID: 26
OV2640 VER: 42
OV2640 Init OK
DCMI Frame OK
LCD Frame Drawn
```

#### 第六阶段验证结论

当前完整链路已经打通：

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

状态：✅ Camera image displayed on LCD successfully。

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
| GPIO | ✅ 已完成 | 已生成 FSMC、LCD 背光、DCMI、SCCB、OV2640 控制和 USART 引脚配置；SCCB、OV2640 控制、DCMI 数据与同步引脚均已通过完整图像链路实机验证。 |
| FSMC | ✅ 已完成 | Bank1 NOR/SRAM4、16 bit、NE4/A6、Extended Mode；已完成 LCD 实机验证。 |
| USART1 | ✅ 已完成 | PA9/PA10，115200，TX/RX；已通过实机串口输出 PID、VER 和 OV2640 初始化状态。 |
| USART2 | ⚠️ 部分完成 | PA2/PA3，921600，当前 HAL 模式为 TX；尚无图像协议和发送代码。 |
| DCMI | ✅ 已完成 | 硬件同步、8 bit、全帧、PCLK 上升沿、HSYNC/VSYNC 低有效、`JPEGMode = DISABLE`；QQVGA snapshot 单帧及帧回调已通过实机验证。 |
| DMA | ✅ 已完成 | DCMI 使用 DMA2 Stream1 / Channel 1、Normal 模式、32 位对齐、Very High 优先级；38,400 字节静态 SRAM 单帧传输已通过实机验证。 |
| TIM6 | ⚠️ 部分完成 | 已生成约 1 Hz 的基础定时器配置，当前应用未启动使用。 |

DCMI/DMA 已形成 OV2640 → DCMI → DMA → 内部 SRAM 的最小采集链路，并通过 `DCMI Frame OK` 实机验证；帧缓冲数据也已通过 FSMC LCD 成功显示。

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
- ✅ 已完成：160×120 RGB565 摄像头画面已显示在 LCD 中央区域，串口输出 `LCD Frame Drawn`。

### 未完成

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
| 当前 HEAD | `d64234f` |
| 版本标签 | `v1.0.0` |
| Release | `STM32F407 OV2640 Camera Project v1.0.0` |
| 里程碑状态 | ✅ 已完成 |

`e7f0351` 是 LCD/FSMC 初始工程与 `v1.0.0` 发布对应的里程碑基线；`d64234f` 是当前实际检出的提交，包含 SCCB、OV2640 PID/VER 最小验证和 USART1 调试输出。两者用途不同：前者用于标识 LCD/FSMC 里程碑起点，后者表示本阶段开始时的仓库版本。

SCCB 阶段在 `BSP/SCCB/` 中新增独立通信模块，并将其接入 Keil 工程；CubeMX 配置和生成代码保持不变。

OV2640 最小验证阶段新增 `BSP/OV2640/`，并在 `main.c` 的 USER CODE 区接入 USART1 输出；当前工作区改动尚未形成新的 Git 提交。

OV2640 完整初始化阶段在现有 `BSP/OV2640/` 内扩展初始化表和错误返回；当前工作区改动尚未形成新的 Git 提交。

SCCB 初始化失败修复阶段记录：实机曾输出 `OV2640 ERROR: SCCB init failed (3)`，其中状态码 3 为总线超时。将 PD6/PD7 默认输出状态改为高电平，并把硬件复位移动到 `SCCB_Init()` 之前后，实机已成功输出 PID `26`、VER `42` 和 `OV2640 Init OK`。

第六阶段完成记录：收到完整 DCMI 帧后，由帧回调仅设置通知标志，主循环调用 `lcd_color_fill()` 将 QQVGA 160×120 RGB565 帧缓冲显示到 LCD 中央区域。实机已依次输出 `DCMI Frame OK` 和 `LCD Frame Drawn`，摄像头画面显示正常，完成软件和硬件闭环验证；该阶段工作区改动尚未形成新的 Git 提交。

## 7. 下一步计划

### 第六阶段历史计划（✅ 已完成）

已完成：

- 已使用当前固件确认串口依次输出 `DCMI Frame OK` 和 `LCD Frame Drawn`。
- 已确认 LCD 中央 160×120 区域能够正常显示摄像头 RGB565 画面。
- 实机画面未发现需要在本阶段处理的 RGB565 颜色异常。

### Phase 7：JPEG + USART2 Transmission（下一阶段）

目标链路：

```text
OV2640
↓
DCMI JPEG Mode
↓
JPEG 数据缓存
↓
USART2 921600 bps
↓
PC XCAM 显示
```

#### 子目标 1：实现 JPEG 数据处理

目标：

- 确认 OV2640 JPEG 输出和 DCMI JPEG 模式。
- 实现 JPEG 帧边界、有效长度和缓存溢出处理。

#### 子目标 2：实现 USART2 图像输出

目标：

- 使用 USART2 921600 bps 输出 JPEG 数据，并完成 PC XCAM 显示验证。
- 根据吞吐量评估 USART2 DMA 发送。

## 8. 保留问题与设计记录

- ⏳ 未完成：当前 DCMI 配置为 `DCMI_JPEG_DISABLE`，第七阶段需要结合 OV2640 JPEG 输出切换并验证 DCMI JPEG 模式。
- ⚠️ 部分完成：JPEG 缓存大小、帧最大长度、溢出处理和缓冲策略需要后续评估。
- ⚠️ 部分完成：USART2 当前为 921600 baud、TX 模式，后续需要评估 DMA 发送和分包机制。
- ✅ 已完成：OV2640 模块硬件时钟来源已经确认，当前模块自带晶振，不需要由 STM32 输出 XCLK。
- ✅ 已完成：DCMI DMA 当前使用 Normal 模式和 Very High 优先级，QQVGA RGB565 单帧采集与 LCD 显示已通过实机验证；后续连续采集策略和中断优先级留待新阶段评估。
- ✅ 已完成：38,400 字节帧缓冲区位于 `0x200000D4`，属于 DMA 可访问的内部 SRAM，不使用 Cortex-M4 CCM RAM。
- ✅ 已完成：当前 DCMI PCLK、HSYNC、VSYNC 极性已通过 OV2640 → DCMI → DMA → LCD 完整链路实机验证。

## 后续维护约束

- 外设配置以 `F4Camera.ioc` 为源，优先通过 CubeMX 修改并重新生成。
- CubeMX 生成文件只在 `USER CODE` 区域加入自定义代码。
- 自定义外设驱动放入 `BSP/`，应用逻辑后续放入独立应用层，不继续堆入 `main.c`。
- LCD/FSMC 初始化由 `MX_FSMC_Init()` 统一管理，LCD BSP 不得重复初始化 FSMC。
- 不得把“配置存在”或“编译通过”写成“硬件验证成功”。
- 所有新增中文内容使用 UTF-8 编码保存。

## 下一次 AI 接手要求

**下一次AI接手时必须先阅读本文件。**
