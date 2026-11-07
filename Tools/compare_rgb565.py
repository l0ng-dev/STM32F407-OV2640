"""接收 USART1 单帧及板端量化结果，与 Pillow 参考预处理比较。"""
# 调试协议依赖板端单帧验证入口；当前双模式主流程没有接入该入口。
import argparse
import time
from pathlib import Path

import numpy as np
import serial
from PIL import Image

FRAME_BYTES = 320 * 240 * 2
TENSOR_BYTES = 3 * 160 * 160


def read_exact(port, size, timeout):
    end = time.monotonic() + timeout
    data = bytearray()
    while len(data) < size and time.monotonic() < end:
        data.extend(port.read(min(size - len(data), 4096)))
    if len(data) != size:
        raise TimeoutError(f"接收超时：期望 {size} 字节，实际 {len(data)} 字节")
    return bytes(data)


def reference(frame):
    pixels = np.frombuffer(frame, dtype='<u2').reshape(240, 320)
    r, g, b = (pixels >> 11) & 31, (pixels >> 5) & 63, pixels & 31
    rgb = np.stack(((r << 3) | (r >> 2), (g << 2) | (g >> 4),
                    (b << 3) | (b >> 2)), axis=2).astype(np.uint8)
    image = Image.fromarray(rgb)
    reduced = image.resize((160, 120), Image.Resampling.BILINEAR)
    canvas = Image.new('RGB', (160, 160), (0, 0, 0))
    canvas.paste(reduced, (0, 20))
    values = np.asarray(canvas, dtype=np.float32).transpose(2, 0, 1) / np.float32(255)
    mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)[:, None, None]
    std = np.array([0.229, 0.224, 0.225], dtype=np.float32)[:, None, None]
    values = (values - mean) / std
    quant = np.rint(values / np.float32(0.01865844801068306)) - 14
    return image, np.clip(quant, -128, 127).astype(np.int8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='USART1 对应的 COM 口')
    parser.add_argument('--output', required=True, type=Path, help='新的结果目录')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('结果目录已存在，请选择新目录')

    print('等待板端预处理和发送；请先完成 SRAM 单帧采集。')
    with serial.Serial(args.port, 115200, timeout=1, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(b'P')
        header = read_exact(port, 4, 120)
        if header != b'R5C1':
            raise ValueError(f'帧头错误: {header!r}')
        payload = read_exact(port, FRAME_BYTES + TENSOR_BYTES + 4, 90)
    data, trailer = payload[:-4], payload[-4:]
    # 与板端一致，FNV-1a 覆盖原始帧及量化输入，不包含帧头和校验尾。
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    if checksum != int.from_bytes(trailer, 'little'):
        raise ValueError('数据校验失败，请重新采集和接收')

    frame, board_bytes = data[:FRAME_BYTES], data[FRAME_BYTES:]
    image, expected = reference(frame)
    board = np.frombuffer(board_bytes, dtype=np.int8).reshape(3, 160, 160)
    difference = np.abs(board.astype(np.int16) - expected.astype(np.int16))
    mismatches = int(np.count_nonzero(difference))
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'frame_rgb565_le.bin').write_bytes(frame)
    (args.output / 'board_input_int8.bin').write_bytes(board_bytes)
    (args.output / 'reference_input_int8.bin').write_bytes(expected.tobytes())
    image.save(args.output / 'frame_rgb.png')
    print(f'校验通过；不一致元素 {mismatches}/{TENSOR_BYTES}，最大差值 {difference.max()}')
    print(f'结果目录: {args.output.resolve()}')
    if mismatches:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
