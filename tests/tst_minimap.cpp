// SPDX-License-Identifier: GPL-3.0-or-later
// Minimap contract test (2026-11-21 rework).
//
// The minimap is a Kate-style "scrollbar minimap": a code preview in the
// scroll track, a viewport thumb over it, and a scroll button at each end.
// House style for the buttons is the TV scrollbar's: cyan (#00AAAA) cells
// with window-blue (#0000AA) arrow glyphs.
//
// Proves:
//  1. both end rows are cyan and carry a blue up/down triangle;
//  2. the preview body is the editor's window blue (no pure black anywhere)
//     and paints the document's real text in editor yellow;
//  3. a long line paints a wider strip than a short one (real text, not
//     random bars);
//  4. the viewport thumb tracks the scroll offset;
//  5. clicking an end row asks for a page scroll; clicking the body asks to
//     jump to that line.
#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QMouseEvent>
#include <QWheelEvent>
#include "src/shell/Minimap.h"

namespace {
const QColor kCyan(0x00, 0xAA, 0xAA);
const QColor kBlue(0x00, 0x00, 0xAA);
const QColor kYellow(0xFF, 0xFF, 0x55);
const QColor kGray(0xAA, 0xAA, 0xAA);

QImage render(const Minimap& mm, const QSize& sz)
{
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const_cast<Minimap&>(mm).render(&p, QRect(QPoint(0, 0), sz));
    p.end();
    return img;
}

int countColor(const QImage& img, const QRect& r, const QColor& c, int tol = 0)
{
    int n = 0;
    for (int y = qMax(0, r.top()); y <= qMin(img.height() - 1, r.bottom()); ++y)
        for (int x = qMax(0, r.left()); x <= qMin(img.width() - 1, r.right()); ++x) {
            const QColor p = img.pixelColor(x, y);
            if (qAbs(p.red() - c.red()) <= tol && qAbs(p.green() - c.green()) <= tol
                && qAbs(p.blue() - c.blue()) <= tol)
                ++n;
        }
    return n;
}

// Rows (inclusive y range) that contain any ink matching `ink`.
QVector<QPair<int, int>> inkBands(const QImage& img, const QRect& area, const QColor& ink,
                                  int tol = 40)
{
    QVector<QPair<int, int>> bands;
    int start = -1;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        bool hit = false;
        for (int x = area.left(); x <= area.right(); ++x) {
            const QColor p = img.pixelColor(x, y);
            if (qAbs(p.red() - ink.red()) <= tol && qAbs(p.green() - ink.green()) <= tol
                && qAbs(p.blue() - ink.blue()) <= tol) {
                hit = true;
                break;
            }
        }
        if (hit && start < 0)
            start = y;
        else if (!hit && start >= 0) {
            bands.append(qMakePair(start, y - 1));
            start = -1;
        }
    }
    if (start >= 0)
        bands.append(qMakePair(start, area.bottom()));
    return bands;
}

int bandWidth(const QImage& img, const QPair<int, int>& band, const QColor& ink, int tol = 40)
{
    int left = img.width(), right = -1;
    for (int y = band.first; y <= band.second; ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor p = img.pixelColor(x, y);
            if (qAbs(p.red() - ink.red()) <= tol && qAbs(p.green() - ink.green()) <= tol
                && qAbs(p.blue() - ink.blue()) <= tol) {
                left = qMin(left, x);
                right = qMax(right, x);
            }
        }
    return right >= left ? right - left + 1 : 0;
}
} // namespace

class TstMinimap : public QObject
{
    Q_OBJECT
private slots:
    // 1. Stock Kate's minimap has a scroll button at each end. Ours must be a
    //    cyan row with the window-blue triangle the other scrollbars use.
    void control_rows_are_cyan_with_blue_arrows() {
        Minimap mm;
        mm.setLines(QStringList() << "int main(void) {");
        mm.resize(96, 400);
        const QImage img = render(mm, QSize(96, 400));

        const QRect top = mm.topButtonRect();
        const QRect bottom = mm.bottomButtonRect();
        QVERIFY2(top.height() >= 6, "top control row missing");
        QVERIFY2(bottom.height() >= 6, "bottom control row missing");

        // The row itself is cyan ...
        const int topCyan = countColor(img, top, kCyan);
        QVERIFY2(topCyan > top.width() * top.height() / 2,
                 qPrintable(QStringLiteral("top row not cyan (%1 px)").arg(topCyan)));
        const int bottomCyan = countColor(img, bottom, kCyan);
        QVERIFY2(bottomCyan > bottom.width() * bottom.height() / 2,
                 qPrintable(QStringLiteral("bottom row not cyan (%1 px)").arg(bottomCyan)));

        // ... and carries a blue triangle glyph.
        const int topInk = countColor(img, top, kBlue);
        const int bottomInk = countColor(img, bottom, kBlue);
        QVERIFY2(topInk >= 6, qPrintable(QStringLiteral("no up-arrow ink (%1 px)").arg(topInk)));
        QVERIFY2(bottomInk >= 6,
                 qPrintable(QStringLiteral("no down-arrow ink (%1 px)").arg(bottomInk)));

        // Up triangle is wider at its base (bottom of the glyph) than at its
        // apex; the down triangle the reverse. Measured on the ink pixels.
        auto inkWidthAt = [&img](const QRect& r, int y) {
            int n = 0;
            for (int x = r.left(); x <= r.right(); ++x) {
                const QColor p = img.pixelColor(x, y);
                if (qAbs(p.red() - kBlue.red()) <= 40 && qAbs(p.green() - kBlue.green()) <= 40
                    && qAbs(p.blue() - kBlue.blue()) <= 40)
                    ++n;
            }
            return n;
        };
        auto inkRows = [&](const QRect& r) {
            int first = -1, last = -1;
            for (int y = r.top(); y <= r.bottom(); ++y)
                if (inkWidthAt(r, y) > 0) {
                    if (first < 0)
                        first = y;
                    last = y;
                }
            return qMakePair(first, last);
        };
        const auto upRows = inkRows(top);
        QVERIFY2(upRows.first >= 0, "top row has no glyph rows");
        const int upTop = inkWidthAt(top, upRows.first);
        const int upBase = inkWidthAt(top, upRows.second);
        QVERIFY2(upBase >= upTop,
                 qPrintable(QStringLiteral("top glyph is not an up triangle (%1 vs %2)")
                                .arg(upTop).arg(upBase)));
        const auto downRows = inkRows(bottom);
        QVERIFY2(downRows.first >= 0, "bottom row has no glyph rows");
        const int downTop = inkWidthAt(bottom, downRows.first);
        const int downTip = inkWidthAt(bottom, downRows.second);
        QVERIFY2(downTop >= downTip,
                 qPrintable(QStringLiteral("bottom glyph is not a down triangle (%1 vs %2)")
                                .arg(downTop).arg(downTip)));
    }

    // 2. The body sits on the editor's window blue; the spec bans pure black
    //    anywhere inside the window, so the minimap must not introduce any.
    void body_is_window_blue_and_never_black() {
        Minimap mm;
        mm.setLines(QStringList() << "line one" << "line two");
        mm.resize(96, 400);
        const QImage img = render(mm, QSize(96, 400));

        const QRect body = mm.bodyRect();
        QVERIFY(body.isValid());
        const QColor bg = img.pixelColor(body.center());
        QVERIFY2(bg == kBlue,
                 qPrintable(QStringLiteral("body background %1 is not #0000AA")
                                .arg(bg.name())));

        int black = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor p = img.pixelColor(x, y);
                if (p.alpha() > 0 && p.red() < 16 && p.green() < 16 && p.blue() < 16)
                    ++black;
            }
        QVERIFY2(black == 0, qPrintable(QStringLiteral("%1 pure-black pixels").arg(black)));
    }

    // 3. The preview paints the document's REAL lines in editor yellow: five
    //    lines give five ink bands.
    void paints_one_text_band_per_line() {
        Minimap mm;
        mm.setLines(QStringList() << "line one" << "line two" << "line three"
                                  << "line four" << "line five");
        mm.resize(96, 400);
        const QImage img = render(mm, QSize(96, 400));

        const auto bands = inkBands(img, mm.bodyRect(), kYellow);
        QVERIFY2(bands.size() >= 5,
                 qPrintable(QStringLiteral("text bands=%1, expected >= 5").arg(bands.size())));
    }

    // 4. A long line paints a wider strip than a short one - the preview is
    //    the document, not decorative noise.
    void long_line_paints_wider_than_short_line() {
        Minimap mm;
        mm.setLines(QStringList()
                    << QStringLiteral("x")
                    << QStringLiteral("    const char* s = \"a much longer line of code\";"));
        mm.resize(96, 400);
        const QImage img = render(mm, QSize(96, 400));

        const auto bands = inkBands(img, mm.bodyRect(), kYellow);
        QCOMPARE(bands.size(), 2);
        const int shortW = bandWidth(img, bands.at(0), kYellow);
        const int longW = bandWidth(img, bands.at(1), kYellow);
        QVERIFY2(longW > shortW * 2,
                 qPrintable(QStringLiteral("long=%1 short=%2").arg(longW).arg(shortW)));
    }

    // 5. The viewport thumb tracks the scroll offset and stays in bounds.
    void viewport_thumb_tracks_scroll() {
        Minimap mm;
        QStringList lines;
        for (int i = 0; i < 200; ++i)
            lines << QStringLiteral("line %1").arg(i);
        mm.setLines(lines);
        mm.resize(96, 400);

        mm.setViewport(0, 20);
        const int yTop0 = mm.viewportRect().top();
        const QImage img0 = render(mm, QSize(96, 400));

        mm.setViewport(120, 20);
        const int yTop1 = mm.viewportRect().top();
        const QImage img1 = render(mm, QSize(96, 400));

        QVERIFY2(yTop1 > yTop0,
                 qPrintable(QStringLiteral("viewport did not move: %1 -> %2")
                                .arg(yTop0).arg(yTop1)));
        QVERIFY(mm.viewportRect().bottom() <= mm.bodyRect().bottom());

        // The thumb is visible where the rect says it is: gray at its top edge
        // after scrolling, and not at the same row before.
        QVERIFY2(img1.pixelColor(mm.bodyRect().center().x(), yTop1) == kGray,
                 "thumb border missing at the reported top");
        QVERIFY2(img0.pixelColor(mm.bodyRect().center().x(), yTop1) != kGray,
                 "thumb drawn away from the reported position");
    }

    // 6. The end rows are scroll buttons: top = page up, bottom = page down.
    void end_rows_request_page_scrolls() {
        Minimap mm;
        mm.resize(96, 400);
        QSignalSpy pages(&mm, &Minimap::pageRequested);

        QTest::mouseClick(&mm, Qt::LeftButton, Qt::NoModifier,
                          mm.topButtonRect().center());
        QTest::mouseClick(&mm, Qt::LeftButton, Qt::NoModifier,
                          mm.bottomButtonRect().center());

        QCOMPARE(pages.count(), 2);
        QCOMPARE(pages.at(0).at(0).toInt(), -1);
        QCOMPARE(pages.at(1).at(0).toInt(), 1);
    }

    // 7. Clicking the preview asks for the line under the pointer.
    void clicking_body_activates_that_line() {
        Minimap mm;
        QStringList lines;
        for (int i = 0; i < 100; ++i)
            lines << QStringLiteral("line %1").arg(i);
        mm.setLines(lines);
        mm.resize(96, 400);
        QSignalSpy activated(&mm, &Minimap::lineActivated);

        const QRect body = mm.bodyRect();
        const int want = 40;
        const int y = body.top() + (want * body.height()) / lines.size() + 1;
        QTest::mouseClick(&mm, Qt::LeftButton, Qt::NoModifier, QPoint(body.center().x(), y));

        QCOMPARE(activated.count(), 1);
        const int got = activated.at(0).at(0).toInt();
        QVERIFY2(qAbs(got - want) <= 1,
                 qPrintable(QStringLiteral("clicked line %1, expected %2").arg(got).arg(want)));
    }

    // 8. The wheel over the preview scrolls the editor, not nothing.
    void wheel_over_preview_requests_line_scroll() {
        Minimap mm;
        mm.resize(96, 400);
        QSignalSpy lines(&mm, &Minimap::scrollLinesRequested);

        const QPoint p = mm.bodyRect().center();
        QWheelEvent ev(QPointF(p), mm.mapToGlobal(p), QPoint(), QPoint(0, -120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&mm, &ev);

        QCOMPARE(lines.count(), 1);
        QVERIFY(lines.at(0).at(0).toInt() > 0);   // wheel down -> scroll down
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstMinimap tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_minimap.moc"
