// SPDX-License-Identifier: GPL-3.0-or-later
// Assertion helper for the AppImage container check: prove the captured frame
// shows a live CRT'd editor window:
//   - >= minDistinct distinct colors  (raw un-shader'd widgets are ~5-10)
//   - >= minLight pixels with lightness > 200 (KTextEditor's white surface)
// Usage: appimage_shot_assert <png> [minDistinct] [minLight]
#include <QGuiApplication>
#include <QImage>
#include <QSet>
#include <QColor>
#include <QString>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: appimage_shot_assert <png> [minDistinct] [minLight]\n");
        return 2;
    }
    const int minDistinct = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 200;
    const int minLight = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 1000;

    QGuiApplication app(argc, argv);
    QImage img(QString::fromLocal8Bit(argv[1]));
    if (img.isNull()) {
        std::fprintf(stderr, "ASSERT-FAIL: cannot load %s\n", argv[1]);
        return 1;
    }
    img = img.convertToFormat(QImage::Format_RGB32);
    QSet<QRgb> colors;
    int light = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            colors.insert(line[x]);
            if (QColor::fromRgb(line[x]).lightness() > 200)
                ++light;
        }
    }
    const int distinct = colors.size();
    const bool ok = distinct >= minDistinct && light >= minLight;
    std::printf("shot %dx%d distinct=%d light=%d (need >=%d / >=%d) -> %s\n",
                img.width(), img.height(), distinct, light,
                minDistinct, minLight, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}