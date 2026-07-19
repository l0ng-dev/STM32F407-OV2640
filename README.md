# STM32F407 OV2640 Dual Mode Camera

基于 STM32F407ZGT6、OV2640 和 FSMC LCD 的双模式摄像头系统。

当前正式版本为 **v1.1.0**，完整支持 RGB565 LCD 实时预览与 JPEG XCAM 图像传输。

## 硬件平台

- MCU：STM32F407ZGTx
- Camera：OV2640
- LCD：正点原子 2.8 寸 FSMC LCD
- 调试接口：USART1，115200 baud
- JPEG 传输接口：USART2，921600 baud
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

## JPEG XCAM 模式

- OV2640 JPEG 模式切换
- DCMI JPEG Snapshot 采集
- 64 KB JPEG SRAM 缓存
- JPEG SOI/EOI 解析
- USART2 JPEG 数据分块发送
- PC/XCAM 图像显示

按下 KEY0（PE4）进入 JPEG XCAM 模式。

数据链路：

```text
OV2640 JPEG -> DCMI JPEG -> DMA Buffer -> SOI/EOI -> USART2 -> XCAM
```

## 双模式管理

Camera Application 统一管理以下状态：

- `CAMERA_MODE_IDLE`
- `CAMERA_MODE_RGB565_LCD`
- `CAMERA_MODE_JPEG_XCAM`

模式切换时先停止 DCMI/DMA，再切换 OV2640 输出格式、DCMI JPEGMode 和 DMA 参数，避免 DMA 冲突、DCMI BUSY 和 LCD 花屏。

## 串口分工

- USART1：系统初始化、模式切换和错误调试日志。
- USART2：仅发送 JPEG 二进制数据，不输出 `printf` 日志。
- `DEBUG_VERBOSE=0`：默认事件型日志。
- `DEBUG_VERBOSE=1`：输出逐帧和 JPEG Marker 调试信息。

## 软件架构

```text
Core
  -> Camera Application
       -> Camera BSP -> DCMI/DMA HAL
       -> OV2640 BSP -> SCCB BSP
       -> JPEG Application
       -> LCD BSP / USART2
```

- `Core/`：CubeMX 初始化、主循环和 HAL 回调转发。
- `BSP/`：LCD、SCCB、OV2640 和 Camera 底层驱动。
- `Application/`：Camera 模式管理、JPEG 缓存与解析。

详细架构参见 [PROJECT_ARCHITECTURE.txt](PROJECT_ARCHITECTURE.txt)，工程状态参见 [PROJECT_STATUS.md](PROJECT_STATUS.md)。

## 开发与验证

- STM32CubeMX / STM32 HAL
- Keil MDK ArmClang AC6
- 全量编译：0 Error、0 Warning
- RGB565 LCD 实时预览已验证
- JPEG USART2 传输及 XCAM 显示已验证
- WKUP/KEY0 双模式切换已验证

## 版本

- `v1.0`：RGB565 基础摄像头预览。
- `v1.1.0`：完整 RGB565 LCD + JPEG XCAM 双模式摄像头系统。
