import math
import struct
from pathlib import Path


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def mix(a, b, t):
    return a + (b - a) * t


def sdf_round_rect(px, py, cx, cy, hw, hh, radius):
    dx = abs(px - cx) - hw + radius
    dy = abs(py - cy) - hh + radius
    ax = max(dx, 0.0)
    ay = max(dy, 0.0)
    return math.hypot(ax, ay) + min(max(dx, dy), 0.0) - radius


def cover(dist):
    return clamp(0.5 - dist, 0.0, 1.0)


def render(size):
    bg = (18, 18, 18)
    accent = (196, 165, 116)
    pixels = []
    s = float(size)
    cx = (s - 1.0) * 0.5
    cy = (s - 1.0) * 0.5
    phone_hw = s * 0.2109375
    phone_hh = s * 0.359375
    phone_r = s * 0.08984375
    stroke = max(1.15, s * 0.027)
    island_hw = s * 0.055
    island_hh = s * 0.018
    bar_hw = s * 0.07
    bar_hh = s * 0.01
    island_y = cy - phone_hh + s * 0.07
    bar_y = cy + phone_hh - s * 0.07
    for y in range(size):
        py = float(y)
        for x in range(size):
            px = float(x)
            phone = sdf_round_rect(px, py, cx, cy, phone_hw, phone_hh, phone_r)
            if size >= 24:
                ring = abs(phone) - stroke * 0.5
                a_phone = cover(ring)
                island = sdf_round_rect(px, py, cx, island_y, island_hw, island_hh, island_hh)
                bar = sdf_round_rect(px, py, cx, bar_y, bar_hw, bar_hh, bar_hh)
                a_phone = max(a_phone, cover(island), cover(bar))
            else:
                a_phone = cover(phone)
            r = mix(bg[0], accent[0], a_phone)
            g = mix(bg[1], accent[1], a_phone)
            b = mix(bg[2], accent[2], a_phone)
            pixels.append((int(b + 0.5), int(g + 0.5), int(r + 0.5), 255))
    return pixels


def dib32(pixels, size):
    xor_stride = size * 4
    and_stride = ((size + 31) // 32) * 4
    header = struct.pack(
        "<IIIHHIIIIII",
        40,
        size,
        size * 2,
        1,
        32,
        0,
        xor_stride * size + and_stride * size,
        0,
        0,
        0,
        0,
    )
    xor = bytearray()
    and_mask = bytearray(and_stride * size)
    for y in range(size - 1, -1, -1):
        row = y * size
        for x in range(size):
            xor.extend(pixels[row + x])
        and_y = (size - 1 - y) * and_stride
        for x in range(size):
            if pixels[row + x][3] < 128:
                and_mask[and_y + (x >> 3)] |= 0x80 >> (x & 7)
    return header + xor + and_mask


def write_ico(path, sizes):
    images = []
    for size in sizes:
        images.append(dib32(render(size), size))
    offset = 6 + 16 * len(images)
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    for size, data in zip(sizes, images):
        w = 0 if size >= 256 else size
        h = 0 if size >= 256 else size
        out.extend(struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    for data in images:
        out.extend(data)
    path.write_bytes(out)


def main():
    root = Path(__file__).resolve().parents[1]
    dest = root / "assets" / "mirror.ico"
    write_ico(dest, (16, 24, 32, 48, 256))


if __name__ == "__main__":
    main()
