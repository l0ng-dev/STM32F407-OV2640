# STM32F407 OV2640 Camera Project

基于 STM32F407ZGT6 的摄像头采集实验，使用 STM32CubeMX、STM32 HAL 和 Keil MDK 开发。

## 硬件

- STM32F407ZGT6
- OV2640 Camera
- 正点原子 2.8 寸 LCD
- ST-Link

## 软件

- STM32CubeMX
- Keil MDK
- ARM Compiler 6
- STM32 HAL

## 当前实现

- ✅ CubeMX 工程建立
- ✅ FSMC LCD 驱动移植
- ✅ LCD 显示测试代码及编译验证
- ⏳ OV2640 初始化
- ⏳ DCMI DMA 采集
- ⏳ JPEG 传输

## 开发阶段

当前项目正在进行正点原子 STM32F407 摄像头实验向 CubeMX HAL 工程的迁移。LCD/FSMC 软件接入已经完成，OV2640、DCMI DMA 采集、JPEG 数据处理和 USART 图像传输仍在开发中。

详细工程状态见 [PROJECT_STATUS.md](PROJECT_STATUS.md)。
