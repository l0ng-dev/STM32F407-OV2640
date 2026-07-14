# OV2640 SCCB 通信验证报告

> 日期：2026-07-14  
> 当前结论：最小通信验证代码已完成 Keil 编译，硬件 PID/VER 读取待验证。

## 1. 验证目标与边界

本阶段只验证 STM32F407ZGT6 能否通过 SCCB 与 OV2640 通信，并读取芯片 PID/VER。未启动 DCMI 和 DMA，未加入完整摄像头初始化表，也未实现 JPEG、LCD 图像显示或 USART 图像发送。

## 2. 修改文件

| 文件 | 修改内容 |
|---|---|
| `BSP/OV2640/ov2640.h` | 定义 OV2640 SCCB 地址、期望 ID、ID 结构体和最小访问接口。 |
| `BSP/OV2640/ov2640.c` | 实现硬件复位、寄存器读写封装，以及 PID/VER 读取。 |
| `Core/Src/main.c` | 在 USER CODE 区调用 SCCB 和 OV2640 验证流程，并将 `printf` 重定向到 USART1。 |
| `MDK-ARM/F4Camera.uvprojx` | 添加 `BSP/SCCB`、`BSP/OV2640` 源文件分组和头文件路径。 |
| `PROJECT_STATUS.md` | 记录本阶段软件完成、编译结果和硬件待验证状态。 |

受保护文件 `Core/Src/gpio.c`、`Core/Src/dcmi.c`、`Core/Src/dma.c` 和 `F4Camera.ioc` 未修改。

## 3. 驱动依赖关系

```text
main.c 最小测试流程
├── BSP/OV2640
│   ├── PWDN/RESET：main.h 引脚定义 + HAL_GPIO_WritePin()
│   ├── 时序延时：HAL_Delay()
│   └── 寄存器访问：BSP/SCCB
│       └── PD6(SCL)、PD7(SDA)：main.h 引脚定义 + HAL GPIO
└── USART1 printf
    └── HAL_UART_Transmit(&huart1, ...)
```

`OV2640_Read_ID()` 先使摄像头退出 PWDN 并完成硬件复位，然后写 `0xFF = 0x01` 选择 Sensor Bank，依次读取 PID 寄存器 `0x0A` 和 VER 寄存器 `0x0B`。每一步都保留 SCCB 状态返回；任一步失败都会立即返回，不会进入错误死循环。

## 4. 编译验证

- Keil 工程目标：`F4Camera`
- 工具链：ArmClang V6.24
- 全量重编译：0 Error、0 Warning
- `sccb.c` 与 `ov2640.c` 均已参与编译
- 固件大小：Code 28704 bytes，RO-data 18820 bytes，RW-data 16 bytes，ZI-data 2200 bytes

以上结果只证明软件能够编译链接，不代表 OV2640 已通过硬件通信验证。

## 5. 硬件测试方法

1. 断电检查 OV2640 模块方向、供电、电压等级和公共地连接。
2. 确认 SCCB SCL 连接 PD6、SDA 连接 PD7，PWDN/RESET 与 CubeMX 当前定义一致。
3. 连接 USART1：PA9 为 TX，PA10 为 RX；串口工具设置为 115200 baud、8 data bits、1 stop bit、no parity。
4. 编译、下载并复位开发板，观察串口启动输出。
5. 正常识别时应看到：

```text
OV2640 PID: 26
OV2640 VER: 42
```

6. 若出现 `SCCB init failed` 或 `SCCB read failed`，记录括号中的状态码，并优先检查供电、连线、SCL/SDA 空闲高电平和 ACK 波形。
7. 若能读到数值但不是 `26/42`，程序会输出 `unexpected chip ID`；此时检查 Bank Select 写入是否成功、模块型号及总线波形。

摄像头缺失或通信失败时，程序只输出错误信息并继续进入主循环，不会因本验证流程死锁。

## 6. 下一步 DCMI 准备事项

完成 PID/VER 实机验证后，再进入以下工作；本阶段未修改相关配置：

- 迁移并裁剪 OV2640 完整初始化寄存器表，先明确输出格式和分辨率。
- 根据 OV2640 实际输出确认 DCMI PCLK、HSYNC、VSYNC 极性。
- 确认 DCMI 8 位数据引脚与摄像头 D0-D7 连接一致。
- 规划 DMA 可访问的 SRAM 图像缓冲区，不使用 CCM RAM。
- 根据帧格式确定 DMA 模式、缓冲策略、帧完成回调和错误恢复机制。
- JPEG 阶段开始前，再决定是否启用 DCMI JPEG 模式及帧边界处理。

在 PID/VER 未稳定读出前，不建议开始 DCMI/DMA 数据链路调试。
