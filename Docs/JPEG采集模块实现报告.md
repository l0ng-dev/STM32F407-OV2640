# JPEG采集模块实现报告

## 1. 架构说明

本阶段采用两层结构，保持 CubeMX 对底层外设的管理权：

- `BSP/Camera`：使用 CubeMX 生成的 `hdcmi` 和 `hdma_dcmi`，负责 Snapshot 启停、DMA剩余计数、采集状态和硬件错误通知。
- `Application/JPEG`：负责64KB静态缓存、SOI/EOI解析、有效帧地址和长度管理。
- `Core/Src/main.c`：仅在 USER CODE 区域完成模式切换、采集启动、周期处理和USART1结果打印。

依赖方向为：

```text
main.c
  ↓
Application/JPEG
  ↓
BSP/Camera
  ↓
CubeMX DCMI/DMA HAL
```

## 2. 文件修改

新增：

- `BSP/Camera/camera.c`
- `BSP/Camera/camera.h`
- `Application/JPEG/jpeg.c`
- `Application/JPEG/jpeg.h`
- `Docs/JPEG采集模块实现报告.md`

修改：

- `Core/Src/main.c`：仅修改 USER CODE 区域，接入JPEG模式、采集模块和USART1状态输出。
- `MDK-ARM/F4Camera.uvprojx`：新增Camera/JPEG分组、源文件和头文件搜索路径。
- `PROJECT_STATUS.md`：记录本阶段软件完成和待硬件验证状态。

未修改GPIO、DMA、DCMI、USART、FSMC和时钟初始化；未修改LCD、SCCB及OV2640 RGB565寄存器表。

## 3. 数据流程

```text
OV2640_SetJPEGMode()
  ↓
OV2640_SetOutputSize(160, 120)
  ↓
JPEG_Capture_Start()
  ↓
Camera_StartJPEGCapture()
  ↓
HAL_DCMI_Start_DMA(SNAPSHOT, 16384 words)
  ↓
DCMI Frame中断记录NDTR并停止DMA请求
  ↓
主循环Camera_Process()停止采集
  ↓
Application/JPEG搜索FF D8和FF D9
  ↓
输出有效JPEG地址和字节长度
```

中断回调只记录状态、DMA剩余计数并停止继续写入；`HAL_DCMI_Stop()`、帧解析和串口打印均在主循环执行。

## 4. 与实验24差异

实验24采用Circular DMA、两个1K-word行缓存和软件拼接大缓存；帧中断停止DMA，并使用 `JPEG_LINE_SIZE - NDTR` 计算最后一段，最终通过 `g_jpeg_data_len << 2` 得到字节数。

当前实现根据本工程已经配置并验证的DMA2 Stream1/Channel1、Normal和Word宽度，使用单个64KB连续缓存直接接收Snapshot，不重新初始化DMA，也不使用多缓冲复制。实际接收字节数为：

```text
(16384 - DMA_NDTR) × 4
```

当前STM32F4 HAL只在DMA传输完成回调中自动开启Frame中断。JPEG通常不会填满64KB，因此Camera模块在成功调用 `HAL_DCMI_Start_DMA()` 后显式开启DCMI Frame中断，以便在可变长帧结束时读取NDTR。该操作不改变CubeMX的DCMI、DMA或NVIC初始化配置。

实验24和当前实现均按字节搜索首个 `FF D8`，随后搜索 `FF D9`；有效长度包含SOI和EOI两个标志。

## 5. JPEG缓存与RAM分析

- 缓存容量：65536字节。
- DMA长度：16384个32位word。
- 对齐：4字节。
- 分配方式：静态数组，无动态内存。
- MAP地址：`0x200002B4`。
- MAP大小：`0x00010000`。
- 所在区域：`RW_IRAM1`，起始地址 `0x20000000`。
- CCM地址区域 `0x10000000` 未使用。

Keil MAP统计：

```text
Total RW Size: 67784 bytes (66.20 KiB)
RW_IRAM1 used: 0x108C8 bytes
RW_IRAM1 limit: 0x1C000 bytes
RW_IRAM2 used: 0 bytes
```

当前JPEG构建开关为常量，链接器移除了未执行的RGB帧缓存，因此当前固件RAM余量充足。RGB565源代码和LCD显示路径仍保留；切换回RGB构建时需要重新检查该模式的MAP占用。

## 6. 错误处理

Camera层覆盖：

- 参数或4字节对齐错误。
- DCMI未处于JPEG模式。
- DMA Stream/Channel、Normal模式、Word宽度或地址递增配置不匹配。
- `HAL_DCMI_Start_DMA()`失败。
- 64KB缓存填满。
- DCMI同步/溢出错误。
- DMA传输错误。
- 停止采集失败。

JPEG层覆盖：

- 未找到SOI，错误码 `0x0201`。
- 找到SOI但未找到EOI，错误码 `0x0202`。
- Camera层错误以 `0x0100 | Camera_Error` 输出。

## 7. 编译结果

Keil MDK ArmClang AC6全量Rebuild：

```text
0 Error(s)
0 Warning(s)
Code:    32904 bytes
RO-data: 19412 bytes
RW-data:    16 bytes
ZI-data: 67768 bytes
```

AXF和HEX均已生成，`camera.c`和`jpeg.c`已参与编译。当前结果为软件编译和链接验证，尚未完成JPEG实机采集验证。

## 8. 硬件测试方法

烧录后通过USART1观察：

```text
OV2640 PID: 26
OV2640 VER: 42
OV2640 Init OK
OV2640 JPEG Capture Start
JPEG Frame OK
JPEG Size: xxxx bytes
```

失败时输出：

```text
JPEG Capture Failed
Error Code: 0xXXXX
```

实机验收时还应在调试器中确认有效数据首字节为 `FF D8`、末两字节为 `FF D9`，且JPEG长度小于65536字节。

## 9. 下一步USART2设计

下一阶段使用 `JPEG_Capture_GetData()` 和 `JPEG_Capture_GetSize()` 获取已经裁剪到SOI/EOI的连续数据。第一版可先使用阻塞式 `HAL_UART_Transmit(&huart2, ...)` 验证XCAM协议；确认数据格式后，再根据921600bps传输耗时评估USART2 TX DMA和分块发送。当前阶段未加入任何USART2发送代码。
