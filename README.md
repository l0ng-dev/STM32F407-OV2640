# STM32F407 OV2640 摄像头与单帧手势 AI

基于 STM32F407ZGT6、OV2640、FSMC LCD 和板载 1 MiB SRAM。保留 RGB565 实时预览、JPEG/VOFA+，并已接入用于验证的单帧采集、预处理及 ShuffleNet 推理。

截至 2026-11-06，用户已完成真实帧的板端/电脑端预处理对照，以及模型输出对照；多次单帧推理约 941～942 ms，不含采集、预处理和串口传输。张手有正确实例，无手和握拳也出现被判为张手的情况；这不是准确率验收。中文分类界面、连续识别、完整时延及长期稳定性仍未完成。

## 操作与串口

| 操作 | 当前行为 |
|---|---|
| 复位 | FSMC 初始化后执行破坏性 SRAM 自检，然后进入 IDLE |
| WKUP（PA0，高有效） | RGB565 320×240 LCD 实时预览 |
| KEY0（PE4，高有效）短按松开 | JPEG/VOFA+ 连续传输 |
| KEY0 长按约 1.2 s | 重新初始化传感器，等待 1 s 后采集 RGB565 到 SRAM，在 LCD 显示静止帧 |

重新采集单帧时，先切回其他模式再长按 KEY0。长按目前只采集并等待电脑命令，不会自动运行 AI。

- **USART2，PA2 TX，921600/8N1**：VOFA+ FireWater 图片、状态和帧间日志。图片控件绑定 `img0`，日志不插入 JPEG 包内。
- **USART1，PA9 TX / PA10 RX，115200/8N1**：单帧验证工具。COM4 在本轮用户接线中已实际通信成功，其他机器或接线必须重新识别端口。串口不能由多个程序同时占用。
- SRAM 单帧完成后，USART1 接收 `P` 导出原始帧和量化输入，`I` 对当前帧预处理及推理，`T` 回放电脑提供的量化输入。传输有 FNV-1a 校验；这些操作为阻塞验证流程，不能当作正式交互界面。
- `sram:` 数值依次表示自检结果、JPEG 字节数、采集及解析耗时、发送耗时、首次观察到 DMA 进展耗时、此后到帧结束耗时。末两项是主循环近似测量。

## 代码与数据流

```text
Core（CubeMX 初始化、主循环、HAL 回调）
  → camera_app：按键、模式切换、单帧采集、LCD 显示
      → Camera / OV2640 / SCCB / LCD BSP
      → JPEG Capture → USART2 VOFA+
      → camera_validation：USART1 验证协议
          → rgb565_preprocess：RGB565 → INT8 NCHW
          → gesture_inference：STAI 初始化、激活区、输入复制、同步推理
              → AI/Model + AI/Middlewares
```

- `AI/Model/`：Studio 生成的模型文件，来源为当前量化 ShuffleNet V2 x0.5；不手工编辑生成实现。
- `AI/Middlewares/`：匹配的 ST AI 头文件和 `NetworkRuntime1210_CM4_Keil.lib`，许可证保存在 `AI/LICENSE.txt`。
- `BSP/SRAM/sram_layout.h`：外部 SRAM 地址和容量的唯一配置入口，编译期检查各区不重叠。
- `Application/Camera/camera_validation.c`：仅在采集完成、DMA 停止后处理验证命令；导出的输入保存在独立区，避免模型复用激活区后丢失参考输入。
- `Application/Camera/gesture_inference.c`：首次请求时初始化模型，检查每步返回值，输出三个原始 INT8 logits 与仅推理耗时。
- `Tools/`：电脑端原始帧和推理对照脚本。模型训练仍在既有 Ubuntu 工作区，未复制训练数据到固件仓库。

按当前请求，2026-11-06 只维护本 README 和本地《F4Camera边缘AI项目交接报告.md》；`PROJECT_ARCHITECTURE.txt`、`PROJECT_STATUS.md` 保留历史内容，不能据其判断本轮 AI 状态。

## 模型与内存

候选模型为 ImageNet 预训练 ShuffleNet V2 x0.5，使用本人自采数据三分类微调。类别顺序：`no_hand / palm / fist`。输入先将 320×240 RGB565 展开为 RGB888，以双线性抗锯齿缩小到 160×120，上下各填 20 行黑边，再按 ImageNet mean/std 归一化并量化为 INT8 NCHW。第一版取最高分，不做其他手势拒识。QDQ 图中仍可能包含浮点运算，不能称为全 INT8。

| 区域 | 起始地址 | 字节数 |
|---|---|---:|
| JPEG 缓冲 | `0x68000000` | 65,536 |
| 外部模型激活 | `0x68010000` | 76,804 |
| RGB565 单帧 | `0x68022C20` | 153,600 |
| 量化输入副本/回放 | `0x68048420` | 76,800 |

模型另需 102,400 字节片内激活区，权重 352,832 字节位于 Flash；输入输出复用片内激活区。Keil 栈已由用户设为 4 KiB；此前静态最大调用链为 1,624 字节并含未知项，不能替代实际栈水位测试。SRAM 只能在 FSMC 初始化和启动自检后使用，自检不得与采集或推理并发。

## 构建与验证

使用 Keil MDK / ArmClang 6.24。打开 `MDK-ARM/F4Camera.uvprojx`，重新加载外部修改后构建。CubeMX 配置和 Keil GUI/下载由用户操作；保持 CubeMX 原始文件注释，应用实现放在自有模块。

2026-11-06 用户已对整理版完成整工程 Rebuild：0 errors / 0 warnings，Code=85024、RO-data=393604、RW-data=16192、ZI-data=110168；RW+ZI=126360 B。两片内 RAM 链接区剩余合计 4712 B；静态最大调用链 1656 B，另有未知项。**整理版已编译链接成功，尚未完成新的下载和板端回归。**

Python 工具需要 `numpy`、`Pillow`、`pyserial`；推理对照另需 `onnxruntime`。先长按 KEY0 并等待 `RGB SRAM frame complete`，保持单帧模式，再执行命令；每次选择不存在的新结果目录：

```powershell
python Tools/compare_rgb565.py --port COM4 --output D:/F407ZGT6/frame_check_new
python Tools/compare_inference.py --port COM4 --output D:/F407ZGT6/inference_check_new
# 回放已有输入时额外指定 --input D:/F407ZGT6/rgb565_compare_01/board_input_int8.bin
```

推理脚本默认读取原 Ubuntu 工作区的量化 ONNX，校验 SHA-256：`994428cdddb23caa0b6587114e8b5093915164093dcf5c48f1a561deae11ac6a`；可用 `--model` 指定同一模型的新路径。输出 logits 不是置信度；当前对照没有实现置信度和中文分类显示。

## 当前限制与下一步

- 曾出现 RGB DCMI overrun 和单帧不完整超时；RGB `CLKRC=0x01` 后有成功记录，但没有充分证据证明偶发问题已根治。
- 单帧初始化后等待从 50 ms 改为 1 s，用户反馈静止画面恢复正常；曝光未稳定是工作假设，不是已测得的根因。
- 预处理真实帧对照出现过 0/76800 差异；板端/ONNX 推理输出有完全一致实例，也有约一个量化步长差异。模型准确率问题仍在，不能以数值一致替代识别正确。
- 模块当前测量符合 STROBE 支路接通、外部 FLASH 支路断开。用户决定保留焊接；厂家回复 STROBE 仅曝光同步闪光、不支持常亮，未提供具体配置。固件未实现补光控制，PA8 高电平不能经未接通支路点灯。
- 优先以固定机位和外部持续照明诊断无手、握拳误判，再决定图像或模型调整；A04 留待方案确定后评估。中文并排 UI、连续识别、其他手势拒识及长期测试仍待后续安排。
