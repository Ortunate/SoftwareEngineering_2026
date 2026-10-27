#!/usr/bin/env python3
# 本文件只读将新典 Nintendo Opus 容器重封装为 Ogg Opus，供离线转码；不解码音频或修改源包。
"""格式核对：https://github.com/vgmstream/vgmstream/blob/master/src/meta/opus.c 。"""
import argparse
from pathlib import Path
import struct


def packet_samples(packet):
    """按 Opus TOC 计算一包的 48 kHz 样本数；拒绝截断与超过 120 ms 的包。"""
    if not packet:
        raise ValueError('空 Opus 包')
    toc = packet[0]
    if toc & 0x80:
        per_frame = 120 << ((toc >> 3) & 3)
    elif toc & 0x60 == 0x60:
        per_frame = 960 if toc & 8 else 480
    else:
        code = (toc >> 3) & 3
        per_frame = 2880 if code == 3 else 480 << code
    code = toc & 3
    if code == 3 and len(packet) < 2:
        raise ValueError('缺少 Opus 帧计数')
    frames = 1 if code == 0 else 2 if code < 3 else packet[1] & 63
    samples = per_frame * frames
    if not 0 < samples <= 5760:
        raise ValueError('非法 Opus 包时长')
    return samples


def page(packet, sequence, granule, flags):
    """封装一包为单个 Ogg 页，保留包字节；CRC 按 Ogg 非反射多项式覆盖完整页计算。"""
    lengths = bytes([255] * (len(packet) // 255) + [len(packet) % 255])
    if len(lengths) > 255:
        raise ValueError('Opus 包超出单页容量')
    result = bytearray(b'OggS' + struct.pack('<BBQIII', 0, flags, granule, 0x54483036, sequence, 0))
    result += bytes([len(lengths)]) + lengths + packet
    crc = 0
    for byte in result:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
    struct.pack_into('<I', result, 22, crc)
    return result


def rewrap(data):
    """返回单流单/双声道 Ogg Opus 字节；保留 pre-skip，拒绝未核对的版本/多流/上下文扩展。"""
    if len(data) < 40 or struct.unpack_from('<II', data) != (0x80000001, 24):
        raise ValueError('不是已核对的 Nintendo Opus 头')
    version, channels = struct.unpack_from('<BB', data, 8)
    rate, offset, seek, context = struct.unpack_from('<4I', data, 12)
    skip, = struct.unpack_from('<H', data, 28)
    if version != 0 or channels not in (1, 2) or rate != 48000 or seek or context or offset != 32:
        raise ValueError('未核对的声道/时钟/扩展区段')
    kind, size = struct.unpack_from('<II', data, offset)
    if kind != 0x80000004 or offset + 8 + size != len(data):
        raise ValueError('数据区段长度无效')
    packets = []
    at = offset + 8
    while at < len(data):
        if at + 8 > len(data):
            raise ValueError('截断的 Opus 包头')
        length, = struct.unpack_from('>I', data, at)
        if length == 0 or at + 8 + length > len(data):
            raise ValueError('Opus 包长度无效')
        packet = data[at + 8:at + 8 + length]
        packets.append((packet, packet_samples(packet)))
        at += 8 + length
    if not packets:
        raise ValueError('没有音频包')
    result = page(b'OpusHead' + struct.pack('<BBHIhB', 1, channels, skip, rate, 0, 0), 0, 0, 2)
    result += page(b'OpusTags' + struct.pack('<II', 0, 0), 1, 0, 0)
    granule = 0
    for index, (packet, samples) in enumerate(packets):
        granule += samples
        result += page(packet, index + 2, granule, 4 if index == len(packets) - 1 else 0)
    return result


def main():
    """参数为源 .opus 和目标 .ogg；先完整校验再写出，输出不能指向输入。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error('输出不能覆盖输入')
    args.output.write_bytes(rewrap(args.source.read_bytes()))


if __name__ == '__main__':
    main()
