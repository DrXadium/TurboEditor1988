#!/usr/bin/env python3
# Print "r g b" for the pixel at (x,y) of a PNG. Used by run_qml_tests.sh.
import sys, struct, zlib
p, X, Y = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
data = open(p, 'rb').read()
assert data[:8] == b'\x89PNG\r\n\x1a\n'
pos = 8; idat = b''
w = h = bd = ct = None
while pos < len(data):
    ln = struct.unpack('>I', data[pos:pos+4])[0]
    typ = data[pos+4:pos+8]; body = data[pos+8:pos+8+ln]; pos += 12 + ln
    if typ == b'IHDR': w, h, bd, ct = struct.unpack('>IIBB', body[:10])
    elif typ == b'IDAT': idat += body
    elif typ == b'IEND': break
raw = zlib.decompress(idat)
ch = {0:1, 2:3, 4:2, 6:4}[ct]
stride = w * ch
out = bytearray(); prev = bytearray(stride); i = 0
for y in range(h):
    filt = raw[i]; i += 1
    line = bytearray(raw[i:i+stride]); i += stride
    if filt == 1:
        for x in range(ch, stride): line[x] = (line[x] + line[x-ch]) & 0xff
    elif filt == 2:
        for x in range(stride): line[x] = (line[x] + prev[x]) & 0xff
    elif filt == 3:
        for x in range(stride):
            a = line[x-ch] if x >= ch else 0
            line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xff
    elif filt == 4:
        for x in range(stride):
            a = line[x-ch] if x >= ch else 0; b = prev[x]; c = prev[x-ch] if x >= ch else 0
            pr = a + b - c; pa = abs(pr-a); pb = abs(pr-b); pc = abs(pr-c)
            r = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            line[x] = (line[x] + r) & 0xff
    out += line; prev = line
X = min(X, w-1); Y = min(Y, h-1)
o = (Y*w + X)*ch
print(out[o], out[o+1], out[o+2])
