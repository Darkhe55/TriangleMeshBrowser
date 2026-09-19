#!/usr/bin/env python3
"""生成 PrismViewer 的鸿蒙应用图标 (纯标准库, 不依赖 PIL)。

用法: python gen_icons.py <输出目录>
生成: app_icon.png / startIcon.png / foreground.png / background.png
"""
import os
import struct
import sys
import zlib


def write_png(path, w, h, pixel_fn):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (None)
        for x in range(w):
            raw.extend(pixel_fn(x, y, w, h))
    def chunk(tag, data):
        out = struct.pack('>I', len(data)) + tag + data
        out += struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
        return out
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    png += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def in_triangle(px, py, size):
    """返回 (是否在三角形内, 重心权重)。三角形居中的等边三角。"""
    cx, cy = size / 2.0, size / 2.0
    r = size * 0.34
    # 顶点: 上, 左下, 右下
    ax, ay = cx, cy - r
    bx, by = cx - r * 0.866, cy + r * 0.5
    dx, dy = cx + r * 0.866, cy + r * 0.5

    def sign(x1, y1, x2, y2, x3, y3):
        return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3)

    d1 = sign(px, py, ax, ay, bx, by)
    d2 = sign(px, py, bx, by, dx, dy)
    d3 = sign(px, py, dx, dy, ax, ay)
    has_neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
    has_pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
    inside = not (has_neg and has_pos)

    total = abs(sign(ax, ay, bx, by, dx, dy))
    w1 = abs(d1) / total if total else 0.0
    w2 = abs(d2) / total if total else 0.0
    w3 = abs(d3) / total if total else 0.0
    return inside, (w1, w2, w3)


def lerp(a, b, t):
    return a + (b - a) * t


def make_icon_pixel(bg=(0x1E, 0x1E, 0x22), c_top=(0x4F, 0xD1, 0xC5),
                    c_bl=(0x7C, 0x6B, 0xFF), c_br=(0xFF, 0x8A, 0x5C)):
    def fn(x, y, w, h):
        inside, (w1, w2, w3) = in_triangle(x, y, w)
        if inside:
            r = c_top[0] * w1 + c_bl[0] * w2 + c_br[0] * w3
            g = c_top[1] * w1 + c_bl[1] * w2 + c_br[1] * w3
            b = c_top[2] * w1 + c_bl[2] * w2 + c_br[2] * w3
            # 细白描边感: 靠近边缘处提亮
            edge = min(w1, w2, w3)
            k = 1.0 if edge > 0.01 else 0.55
            return (int(min(255, r * k + 40 * (1 - k))),
                    int(min(255, g * k + 40 * (1 - k))),
                    int(min(255, b * k + 40 * (1 - k))), 255)
        # 背景: 轻微径向渐变
        cx, cy = w / 2.0, h / 2.0
        d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 / (w * 0.71)
        t = min(1.0, d)
        return (int(lerp(bg[0], 0x0C, t)),
                int(lerp(bg[1], 0x0C, t)),
                int(lerp(bg[2], 0x10, t)), 255)
    return fn


def solid(color, alpha=255):
    def fn(x, y, w, h):
        return (color[0], color[1], color[2], alpha)
    return fn


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(outdir, exist_ok=True)
    icon = make_icon_pixel()

    write_png(os.path.join(outdir, 'app_icon.png'), 512, 512, icon)
    write_png(os.path.join(outdir, 'startIcon.png'), 512, 512, icon)
    write_png(os.path.join(outdir, 'foreground.png'), 512, 512, icon)
    write_png(os.path.join(outdir, 'background.png'), 512, 512, solid((0x1E, 0x1E, 0x22)))
    print('icons written to', outdir)


if __name__ == '__main__':
    main()
