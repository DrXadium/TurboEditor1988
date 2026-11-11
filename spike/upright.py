#!/usr/bin/env python3
# TV-revision2: prove the composed CRT frame is NOT rotated.
# The test source paints a RED block in its top-left quarter and a BLUE block
# in its bottom-right quarter. If the pipeline rotates the image (the
# SweepPatch defect), the red lands in the top-RIGHT and the blue in the
# bottom-LEFT. Prints the centroid of each colour; the shell check asserts
# red is in the left half + upper half and blue in the right half + lower half.
import sys

from sweepcentroid import load  # same PNG decoder


def centroid(path, match):
    w, h, ch, px = load(path)
    sx = sy = n = 0
    for y in range(h):
        row = y * w * ch
        for x in range(w):
            o = row + x * ch
            r, g, b = px[o], px[o + 1], px[o + 2]
            if match(r, g, b):
                sx += x
                sy += y
                n += 1
    if n == 0:
        return None
    return sx / n, sy / n, n


def main(path):
    w, h, _ch, _px = load(path)
    red = centroid(path, lambda r, g, b: r > 150 and g < 90 and b < 90)
    blue = centroid(path, lambda r, g, b: b > 150 and r < 90 and g < 90)
    print(f"size {w}x{h}")
    for name, c in (("red", red), ("blue", blue)):
        if c is None:
            print(f"{name} none")
        else:
            print(f"{name} cx={c[0]:.1f} cy={c[1]:.1f} n={c[2]}")


if __name__ == "__main__":
    main(sys.argv[1])
