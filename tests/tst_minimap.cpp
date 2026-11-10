// SPDX-License-Identifier: GPL-3.0-or-later
// Task 9A: Minimap contract test.
// Proves: (1) a doc with N lines paints >= N distinct line strips; (2) the
// viewport-rect Y tracks the scroll offset (moving the viewport down moves the
// highlight down).
#include <QtTest>
#include <QImage>
#include <QPainter>
#include "src/shell/Minimap.h"

namespace {
// Render the minimap to an image and return it.
QImage render(const Minimap& mm, const QSize& sz)
{
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const_cast<Minimap&>(mm).render(&p, QRect(QPoint(0, 0), sz));
    p.end();
    return img;
}
} // namespace

class TstMinimap : public QObject
{
    Q_OBJECT
private slots:
    void paints_one_strip_per_line() {
        Minimap mm;
        mm.setLines(QStringList() << "line one" << "line two" << "line three"
                                  << "line four" << "line five");
        mm.resize(40, 400);
        const QImage img = render(mm, QSize(40, 400));

        // Count horizontal "strips": rows that contain any non-transparent ink.
        // With 5 lines we expect >= 5 separate inked bands.
        QVector<bool> inked(400, false);
        for (int y = 0; y < 400; ++y) {
            for (int x = 0; x < 40; ++x) {
                if (img.pixelColor(x, y).alpha() > 20) { inked[y] = true; break; }
            }
        }
        int strips = 0;
        for (int y = 0; y < 400; ++y)
            if (inked[y] && (y == 0 || !inked[y - 1]))
                ++strips;
        QVERIFY2(strips >= 5,
                 qPrintable(QStringLiteral("strips=%1, expected >= 5").arg(strips)));
    }

    void viewport_rect_tracks_scroll() {
        Minimap mm;
        QStringList lines;
        for (int i = 0; i < 50; ++i)
            lines << QStringLiteral("line %1").arg(i);
        mm.setLines(lines);
        mm.resize(40, 400);

        mm.setViewport(0, 10);              // first 10 lines visible
        const int yTop0 = mm.viewportRect().top();
        mm.setViewport(30, 10);             // scrolled down
        const int yTop1 = mm.viewportRect().top();

        QVERIFY2(yTop1 > yTop0,
                 qPrintable(QStringLiteral("viewport top did not move down: %1 -> %2")
                                .arg(yTop0).arg(yTop1)));
        // Highlight must stay inside the minimap bounds.
        QVERIFY(mm.viewportRect().bottom() <= 400);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstMinimap tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_minimap.moc"