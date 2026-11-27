#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Decode the frames saved by tests/tst_effectchain.qml and assert each shader
# behaviour from the 2026-11-26 CRT pipeline spec. Qt 6 QML has no pixel-read
# API, so this is the pixel half of the probe.
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "spike"))
from sweepcentroid import load, centroid  # noqa: E402

TMP = "/tmp"
failures = []


def img(name):
    path = os.path.join(TMP, name)
    if not os.path.exists(path):
        raise SystemExit(f"{name}: missing frame (did the QML probe fail?)")
    w, h, ch, px = load(path)
    return w, h, ch, px


def px(name, x, y):
    w, h, ch, data = img(name)
    x = max(0, min(x, w - 1))
    y = max(0, min(y, h - 1))
    o = (y * w + x) * ch
    return data[o], data[o + 1], data[o + 2]


def lum(rgb):
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def check(label, ok, detail):
    if ok:
        print(f"  PASS {label}: {detail}")
    else:
        print(f"  FAIL {label}: {detail}")
        failures.append(label)


def check_identity():
    w, h, ch, data = img("crt_identity.png")
    red = blue = 0
    for o in range(0, len(data), ch):
        r, g, b = data[o], data[o + 1], data[o + 2]
        if r == 255 and g == 0 and b == 0:
            red += 1
        elif r == 0 and g == 0 and b == 255:
            blue += 1
    check("identity", red > 1000 and blue > 1000, f"red={red} blue={blue}")


def check_curvature():
    corner = px("crt_curvature.png", 2, 2)
    check("curvature", lum(corner) < 10, f"corner={corner}")


def check_bloom():
    inside = lum(px("crt_bloom.png", 160, 120))
    left = lum(px("crt_bloom.png", 135, 120))
    right = lum(px("crt_bloom.png", 185, 120))
    far = lum(px("crt_bloom.png", 300, 220))
    # bloom divides the source by (1 + bloom) while adding the blurred copy, so
    # the block itself sits near 109 at the default 2.5x bloom pass. What proves
    # a SPATIAL blur is ink outside the block's bounds.
    check("bloom halo", inside > 80 and max(left, right) > 8 and max(left, right) > far + 5,
          f"inside={inside:.0f} left={left:.0f} right={right:.0f} far={far:.0f}")


def check_burn_in():
    a = lum(px("crt_burnin_a.png", 160, 120))
    b = lum(px("crt_burnin_b.png", 160, 120))
    control = lum(px("crt_burnin_a.png", 40, 40))
    check("burn-in ghost", a > 20 and a > b + 8 and control < 5,
          f"ghost_a={a:.0f} ghost_b={b:.0f} control={control:.0f}")


def check_raster():
    w, h, ch, d0 = img("crt_raster0.png")

    def row_values(name, x, y0, y1):
        return [lum(px(name, x, y)) for y in range(y0, y1)]

    base = row_values("crt_raster0.png", 100, 100, 116)
    scan = row_values("crt_raster1.png", 100, 100, 116)
    base_span = max(base) - min(base)
    scan_span = max(scan) - min(scan)
    check("raster mode 0 identity", base_span < 2.0, f"span={base_span:.1f}")
    check("raster mode 1 scanline band", scan_span > 25, f"row span={scan_span:.1f}")

    grid = []
    for y in (100, 101, 102, 103):
        for x in (100, 101, 102, 103):
            grid.append(lum(px("crt_raster2.png", x, y)))
    check("raster mode 2 pixel grid", max(grid) - min(grid) > 20,
          f"grid span={max(grid) - min(grid):.1f}")

    stripes = 0
    for x in range(20, 300):
        r, g, b = px("crt_raster3.png", x, 120)
        if max(abs(r - g), abs(g - b), abs(r - b)) > 15:
            stripes += 1
    check("raster mode 3 subpixel stripes", stripes > 20, f"colour-split columns={stripes}")


def check_rgb_shift():
    def first_channel(xs, channel, threshold=40):
        for x in xs:
            rgb = px("crt_rgbshift.png", x, 120)
            if rgb[channel] > threshold:
                return x
        return None

    red_x = first_channel(range(140, 200), 0)
    blue_x = first_channel(range(140, 200), 2)
    ok = red_x is not None and blue_x is not None and red_x <= blue_x - 2
    check("RGB shift", ok, f"red edge x={red_x} blue edge x={blue_x}")


def check_glow():
    ax, ay, _w, _h = centroid(os.path.join(TMP, "crt_glow_a.png"))
    bx, by, _w2, _h2 = centroid(os.path.join(TMP, "crt_glow_b.png"))
    check("glow line moves in Y", abs(by - ay) > 10 and abs(bx - ax) < 8,
          f"A=({ax:.0f},{ay:.0f}) B=({bx:.0f},{by:.0f})")


def check_phosphor():
    off = px("crt_phosphor_off.png", 160, 120)
    on = px("crt_phosphor_on.png", 160, 120)
    check("phosphor off preserves hue", off[1] > 200 and off[0] < 40, f"off={off}")
    check("phosphor on converges to fontColor", on[0] > on[1] + 40 and on[2] < 90,
          f"on={on}")


def check_radius():
    corner = px("crt_radius.png", 2, 2)
    centre = px("crt_radius.png", 160, 120)
    check("screen radius masks corners", lum(corner) < 10 and lum(centre) > 200,
          f"corner={corner} centre={centre}")


def check_inset():
    corner = px("crt_inset.png", 2, 2)
    edge = px("crt_inset.png", 160, 2)
    centre = px("crt_inset.png", 160, 120)
    check("screen inset masks border", lum(corner) < 10 and lum(edge) < 10
          and lum(centre) > 200, f"corner={corner} edge={edge} centre={centre}")


def check_ambient():
    off = lum(px("crt_ambient_off.png", 2, 120))
    on = lum(px("crt_ambient_on.png", 2, 120))
    off_c = lum(px("crt_ambient_off.png", 160, 120))
    on_c = lum(px("crt_ambient_on.png", 160, 120))
    check("ambient light lifts the edge", on > off + 20 and abs(on_c - off_c) < 6,
          f"edge {off:.0f}->{on:.0f} centre {off_c:.0f}->{on_c:.0f}")


def check_sheen():
    off_br = px("crt_sheen_off.png", 260, 190)
    on_br = px("crt_sheen_on.png", 260, 190)
    off_tl = px("crt_sheen_off.png", 60, 50)
    on_tl = px("crt_sheen_on.png", 60, 50)
    d_br = lum(on_br) - lum(off_br)
    d_tl = lum(on_tl) - lum(off_tl)
    check("glass sheen is diagonal", d_br > 8 and d_br > d_tl + 4,
          f"bottom-right +{d_br:.0f}, top-left +{d_tl:.0f}")


def main():
    check_identity()
    check_curvature()
    check_bloom()
    check_burn_in()
    check_raster()
    check_rgb_shift()
    check_glow()
    check_phosphor()
    check_radius()
    check_inset()
    check_ambient()
    check_sheen()
    if failures:
        raise SystemExit("FX PIXEL FAIL: " + ", ".join(failures))
    print("FX PIXELS OK")


if __name__ == "__main__":
    main()
