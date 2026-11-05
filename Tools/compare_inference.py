"""请求板端单帧推理，并用同一 INT8 输入与量化 ONNX 对照。"""
import argparse
import hashlib
import time
from pathlib import Path

import numpy as np
import onnxruntime as ort
import serial

INPUT_BYTES = 3 * 160 * 160
MODEL_SHA256 = '994428cdddb23caa0b6587114e8b5093915164093dcf5c48f1a561deae11ac6a'
DEFAULT_MODEL = (r'\\wsl.localhost\Ubuntu-24.04\home\leo\projects\model\CNN_Project\F4Camera_Gesture'
                 r'\real_dataset\runs\p01_pretrained_shuffle05_01\quant_qdq_02_repeat'
                 r'\shufflenet_v2_x0_5_int8_qdq.onnx')


def read_exact(port, length, timeout):
    deadline = time.monotonic() + timeout
    result = bytearray()
    while len(result) < length and time.monotonic() < deadline:
        result.extend(port.read(min(4096, length - len(result))))
    if len(result) != length:
        raise TimeoutError(f'接收超时：期望 {length} 字节，实际 {len(result)} 字节')
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--model', type=Path, default=Path(DEFAULT_MODEL))
    parser.add_argument('--input', type=Path, help='已有的 76800 字节 INT8 输入；不填则使用当前帧')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('结果目录已存在，请选择新目录')
    digest = hashlib.sha256(args.model.read_bytes()).hexdigest()
    if digest != MODEL_SHA256:
        parser.error('ONNX 模型 SHA-256 不匹配，停止对照')
    session = ort.InferenceSession(str(args.model), providers=['CPUExecutionProvider'])
    requested_input = args.input.read_bytes() if args.input else None
    if requested_input is not None and len(requested_input) != INPUT_BYTES:
        parser.error('输入文件必须恰为 76800 字节')
    print('等待板端预处理、推理和发送；请先完成 SRAM 单帧采集。', flush=True)
    with serial.Serial(args.port, 115200, timeout=1, write_timeout=30) as port:
        port.reset_input_buffer()
        port.write(b'T' if requested_input is not None else b'I')
        if requested_input is not None:
            if read_exact(port, 4, 10) != b'RDY1':
                raise ValueError('板端未准备好接收输入')
            checksum = 2166136261
            for byte in requested_input:
                checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
            port.write(requested_input + checksum.to_bytes(4, 'little'))
        if read_exact(port, 4, 120) != b'I5C1':
            raise ValueError('帧头不正确')
        packet = read_exact(port, INPUT_BYTES + 7 + 4, 45)
    data, trailer = packet[:-4], packet[-4:]
    checksum = 2166136261
    for byte in data:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    if checksum != int.from_bytes(trailer, 'little'):
        raise ValueError('数据校验失败')

    input_bytes = data[:INPUT_BYTES]
    if requested_input is not None and input_bytes != requested_input:
        raise ValueError('板端返回的输入与发送的输入不一致')
    quant = np.frombuffer(input_bytes, dtype=np.int8).reshape(1, 3, 160, 160)
    board = np.frombuffer(data[INPUT_BYTES:INPUT_BYTES+3], dtype=np.int8).copy()
    elapsed = int.from_bytes(data[INPUT_BYTES+3:INPUT_BYTES+7], 'little')
    model_input = (quant.astype(np.float32) - np.float32(-14)) * np.float32(0.01865844801068306)
    onnx_logits = session.run(None, {session.get_inputs()[0].name: model_input})[0].reshape(-1)
    board_logits = (board.astype(np.float32) - np.float32(48)) * np.float32(0.012564831413328648)
    labels = ('no_hand', 'palm', 'fist')
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'board_input_int8.bin').write_bytes(input_bytes)
    (args.output / 'board_output_int8.bin').write_bytes(board.tobytes())
    np.save(args.output / 'onnx_logits.npy', onnx_logits)
    print(f'传输校验通过；板端推理耗时 {elapsed} ms（不含预处理及串口传输）')
    print(f'板端 INT8: {board.tolist()}，反量化 logits: {board_logits.tolist()}')
    print(f'ONNX logits: {onnx_logits.tolist()}')
    print(f'最大绝对差: {np.max(np.abs(board_logits - onnx_logits)):.8f}')
    print(f'板端类别: {labels[int(np.argmax(board))]}；ONNX 类别: {labels[int(np.argmax(onnx_logits))]}')
    print(f'结果目录: {args.output.resolve()}')


if __name__ == '__main__':
    main()
