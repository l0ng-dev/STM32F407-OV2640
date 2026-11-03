# STM32F407 OV2640 Dual Mode Camera

基于 STM32F407ZGT6、OV2640 和 FSMC LCD 的双模式摄像头系统。

当前工程基于 v1.1.0 开发外部 SRAM 和 VOFA+ 支持；手势 AI 尚未实现。v1.1.0 的历史验证记录保留在项目状态文档中。

## 硬件平台

- MCU：STM32F407ZGTx
- Camera：OV2640
- LCD：正点原子 2.8 寸 FSMC LCD
- 外部 SRAM：板载 XM8A51216V33A，1 MiB，FSMC Bank 3
- 图像与调试接口：USART2，921600 baud、8N1（FireWater）
- 调试器：ST-Link

## RGB565 LCD 模式

- OV2640 RGB565 输出
- DCMI Snapshot 采集
- DMA 图像传输
- 320×240 横屏全屏实时预览
- DMA 直接写入 LCD GRAM
- FSMC 高速写时序优化

按下 WKUP（PA0）进入 RGB565 LCD 实时预览模式。

数据链路：

```text
OV2640 RGB565 -> DCMI -> DMA -> FSMC LCD GRAM
```

## JPEG VOFA+ 模式

- OV2640 JPEG 模式切换
- DCMI JPEG Snapshot 采集
- 外部 SRAM 起始处的 64 KiB JPEG 缓冲
- JPEG SOI/EOI 解析
- USART2 JPEG 数据分块发送
- PC/VOFA+ 图像显示

按下 KEY0（PE4）进入 JPEG VOFA+ 模式。

上位机选择 VOFA+ 的 **FireWater** 数据引擎，打开 USART2 对应的串口，将图片控件绑定到 `img0`。USART2 的 PA2 为发送端，USB 转串口的接收端连接 PA2，并与开发板共地。

数据链路：

```text
OV2640 JPEG -> DCMI JPEG -> 外部 SRAM Buffer -> SOI/EOI -> USART2 -> VOFA+
```

## 双模式管理

Camera Application 统一管理以下状态：

- `CAMERA_MODE_IDLE`
- `CAMERA_MODE_RGB565_LCD`
- `CAMERA_MODE_JPEG_VOFA`

模式切换时先停止 DCMI/DMA，再切换 OV2640 输出格式、DCMI JPEGMode 和 DMA 参数，避免 DMA 冲突、DCMI BUSY 和 LCD 花屏。

## 串口分工

- USART2：发送 VOFA+ 图片包、状态数值和帧间日志；USART1 已停用。
- 图片前导帧和 JPEG 数据作为完整包发送，日志在图片包之间输出。
- `sram:` 数值依次表示启动自检结果、JPEG 字节数、采集及解析耗时、发送耗时、首次观察到 DMA 进展的耗时，以及从该时刻到帧结束的耗时；时间单位为毫秒，最后两项是近似值。
- 串口发送发生错误后，固件停止向该数据流追加数据；需重置 VOFA+ 接收流并复位目标板后重试。
- `DEBUG_VERBOSE=0`：默认事件型日志。
- `DEBUG_VERBOSE=1`：输出逐帧和 JPEG Marker 调试信息。

## 软件架构

```text
Core
  -> Camera Application
       -> Camera BSP -> DCMI/DMA HAL
       -> OV2640 BSP -> SCCB BSP
       -> JPEG Application
       -> LCD BSP / VOFA 传输
```

- `Core/`：CubeMX 初始化、主循环和 HAL 回调转发。
- `BSP/`：LCD、SCCB、OV2640、Camera 及 SRAM 自检。
- `Application/`：Camera 模式管理、JPEG 缓存与解析、VOFA 传输和日志。

详细架构参见 [PROJECT_ARCHITECTURE.txt](PROJECT_ARCHITECTURE.txt)，工程状态参见 [PROJECT_STATUS.md](PROJECT_STATUS.md)。

## 外部 SRAM 自检

固件当前在每次启动时执行一次破坏性 SRAM 自检，会覆盖 `0x68000000` 起的整个 1 MiB 空间。它用于本阶段验证；正式使用 SRAM 保存图像或其他数据前，应移除或改为按需执行。

## 开发与验证

- STM32CubeMX / STM32 HAL
- Keil MDK ArmClang AC6
- 当前重构版使用 ArmClang 6.24 构建成功：0 Error、0 Warning。
- 重构后用户提供的板端日志显示 SRAM 自检通过，并依次进入 LCD 预览和 JPEG VOFA+ 模式，连续完成 JPEG 发送；日志本身不能证明上位机画面解码或长期稳定性。
- 早期版本有 LCD 实时预览和 VOFA+ 图像显示的用户实机反馈；未完成长期或压力测试。

## 版本

- `v1.0`：RGB565 基础摄像头预览。
- `v1.1.0`：完整 RGB565 LCD + JPEG XCAM 双模式摄像头系统。
