#!/usr/bin/env python3
"""Converts a binary PPM (P6) written by NOVA GPU into a PNG.

Only used for looking at renderer output; it is deliberately dependency-free
(zlib is part of the Python standard library).

Usage: python3 tools/ppm_to_png.py input.ppm output.png
"""
import struct
import sys
import zlib


def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()

    fields = []
    i = 0
    while len(fields) < 4:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b'#':
            while i < len(data) and data[i:i + 1] != b'\n':
                i += 1
            continue
        start = i
        while i < len(data) and not data[i:i + 1].isspace():
            i += 1
        fields.append(data[start:i])
    i += 1  # single whitespace character after the maxval

    magic, width, height, maxval = fields[0], int(fields[1]), int(fields[2]), int(fields[3])
    if magic != b'P6' or maxval != 255:
        raise ValueError('expected a binary P6 PPM with maxval 255')
    pixels = data[i:i + width * height * 3]
    if len(pixels) != width * height * 3:
        raise ValueError('truncated pixel data')
    return width, height, pixels


def write_png(path, width, height, rgb):
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)  # filter: none
        raw += rgb[y * stride:(y + 1) * stride]

    def chunk(tag, payload):
        out = struct.pack('>I', len(payload)) + tag + payload
        return out + struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF)

    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', header))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip())
        return 2
    width, height, pixels = read_ppm(sys.argv[1])
    write_png(sys.argv[2], width, height, pixels)
    print(f'{sys.argv[1]} -> {sys.argv[2]} ({width}x{height})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
