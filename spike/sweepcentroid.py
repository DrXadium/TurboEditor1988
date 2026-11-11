#!/usr/bin/env python3
# TV-revision2 sweep orientation: centroid of the bright band in two PNGs.
# The band is the pixels above the median-luminance threshold. Prints
# "A cx cy" / "B cx cy" centroids; the shell check asserts cy moves and cx
# stays put (top→bottom sweep).
import sys, struct, zlib

def load(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    pos = 8; idat = b''; w = h = bd = ct = None
    while pos < len(data):
        ln = struct.unpack('>I', data[pos:pos+4])[0]
        typ = data[pos+4:pos+8]; body = data[pos+8:pos+8+ln]; pos += 12 + ln
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', body[:10])
        elif typ == b'IDAT':
            idat += body
        elif typ == b'IEND':
            break
    assert bd == 8, f'{path}: bitdepth {bd}'
    raw = zlib.decompress(idat)
    ch = {0: 1, 2: 3, 4: 2, 6: 4}[ct]
    stride = w * ch
    out = bytearray(); prev = bytearray(stride); i = 0
    for y in range(h):
        filt = raw[i]; i += 1
        line = bytearray(raw[i:i + stride]); i += stride
        if filt == 1:
            for x in range(ch, stride):
                line[x] = (line[x] + line[x - ch]) & 0xff
        elif filt == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xff
        elif filt == 3:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xff
        elif filt == 4:
            for x in range(stride):
                a = line[x - ch] if x >= ch else 0
                b = prev[x]; c = prev[x - ch] if x >= ch else 0
                p = a + b - c; pa = abs(p - a); pb = abs(p - b); pc = abs(p - c)
                r = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + r) & 0xff
        out += line; prev = line
    return w, h, ch, out

def centroid(path):
    w, h, ch, out = load(path)
    lum = []
    for y in range(h):
        row = y * w * ch
        for x in range(w):
            o = row + x * ch
            lum.append((0.299 * out[o] + 0.587 * out[o + 1] + 0.114 * out[o + 2], x, y))
    lum.sort()
    thr = lum[-1][0] * 0.85  # top ~6% of the dynamic range
    sx = sy = n = 0
    for v, x, y in lum:
        if v >= thr:
            sx += x; sy += y; n += 1
    if n == 0:
        raise SystemExit(f'{path}: no bright pixels')
    return sx / n, sy / n, w, h

if __name__ == '__main__':
    a = centroid(sys.argv[1])
    b = centroid(sys.argv[2])
    print(f'A {a[0]:.1f} {a[1]:.1f} {a[2]}x{a[3]}')
    print(f'B {b[0]:.1f} {b[1]:.1f} {b[2]}x{b[3]}')
