// SPDX-License-Identifier: GPL-3.0-or-later
// Task 4: Turbo Vision text-mode scrollbar contract test.
// The scrollbars must render DOS-style ASCII glyphs: arrow blocks (▲▼ / ◄►)
// at the ends, a textured track (▒), and a solid thumb (█) — not plain color
// bars. Driven by the TextScrollBar subclass (BorlandStyle installs it).
#include <QtTest>
#include <QImage>
#include <QColor>
#include <QScrollBar>
#include "src/shell/TextScrollBar.h"
#include "src/shell/ShellWindow.h"

namespace {
// Teal track (#1AAFAF) and green handle (#19A819) — the TV scrollbar palette.
bool isTeal(const QColor& c)
{
    return c.blue() > 140 && c.green() > 140 && c.red() < 120;
}
// Dark glyph pixel (arrow/body drawn in black over teal/green).
bool isDark(const QColor& c)
{
    return c.red() < 60 && c.green() < 60 && c.blue() < 60;
}
} // namespace

class TstScrollbars : public QObject
{
    Q_OBJECT
private slots:
    // A vertical TextScrollBar renders a down-arrow glyph in its add-line
    // (bottom) button region — dark pixels forming the ▼ arrow on the teal bg.
    void vertical_scrollbar_draws_arrow_glyph() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(0);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());

        // add-line (bottom button) region: bottom ~14px.
        const int btnH = 14;
        int darkInBottom = 0;
        for (int y = v.height() - btnH; y < v.height(); ++y)
            for (int x = 2; x < v.width() - 2; ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                    continue;
                if (isDark(img.pixelColor(x, y)))
                    ++darkInBottom;
            }
        QVERIFY2(darkInBottom >= 4,
                 qPrintable(QStringLiteral(
                     "vertical scrollbar add-line has no arrow glyph (%1 dark px)")
                                .arg(darkInBottom)));
    }

    // A horizontal TextScrollBar renders a right-arrow glyph in its add-line.
    void horizontal_scrollbar_draws_arrow_glyph() {
        TextScrollBar h(Qt::Horizontal);
        h.setRange(0, 100);
        h.setValue(0);
        h.resize(300, 14);
        h.show();
        QCoreApplication::processEvents();
        const QImage img = h.grab().toImage();
        QVERIFY(!img.isNull());

        const int btnW = 14;
        int darkInRight = 0;
        for (int y = 2; y < h.height() - 2; ++y)
            for (int x = h.width() - btnW; x < h.width(); ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                    continue;
                if (isDark(img.pixelColor(x, y)))
                    ++darkInRight;
            }
        QVERIFY2(darkInRight >= 4,
                 qPrintable(QStringLiteral(
                     "horizontal scrollbar add-line has no arrow glyph (%1 dark px)")
                                .arg(darkInRight)));
    }

    // The scrollbar track is the TV teal (#1AAFAF), not the default gray.
    void scrollbar_track_is_tv_teal() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(50);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());
        // Sample the track just above the thumb (mid-upper region).
        int teal = 0;
        for (int y = 40; y < 60; ++y)
            for (int x = 1; x < v.width() - 1; ++x)
                if (isTeal(img.pixelColor(x, y)))
                    ++teal;
        QVERIFY2(teal > 20,
                 qPrintable(QStringLiteral(
                     "scrollbar track is not TV teal (#1AAFAF): %1 teal px").arg(teal)));
    }
    // App-wide wiring: a plain QScrollBar (as KTextEditor/QScrollArea create)
    // is converted to a TextScrollBar so the DOS glyphs appear everywhere.
    void plain_qscrollbar_is_converted_to_text_scrollbar() {
        ShellWindow w;   // installing BorlandStyle installs the converter
        w.show();
        QCoreApplication::processEvents();
        auto* sb = new QScrollBar(Qt::Vertical, &w);
        sb->resize(14, 60);
        sb->show();
        QCoreApplication::processEvents();
        // The installer (event filter) should have swapped it for a
        // TextScrollBar, or reparented one in its place.
        bool found = false;
        const auto texts = w.findChildren<TextScrollBar*>();
        for (TextScrollBar* t : texts)
            if (t != sb)
                found = true;
        QVERIFY2(found || qobject_cast<TextScrollBar*>(sb),
                 "plain QScrollBar was not converted to TextScrollBar");
        delete sb;
    }
};

QTEST_MAIN(TstScrollbars)
#include "tst_scrollbars.moc"
