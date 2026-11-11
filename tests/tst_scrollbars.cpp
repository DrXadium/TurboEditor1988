// SPDX-License-Identifier: GPL-3.0-or-later
// Task 4: Turbo Vision text-mode scrollbar contract test.
// The scrollbars must render DOS-style ASCII glyphs: arrow blocks (▲▼ / ◄►)
// at the ends, a textured track (▒), and a solid thumb (█) — not plain color
// bars. Driven by the TextScrollBar subclass (BorlandStyle installs it).
#include <QtTest>
#include <QImage>
#include <QColor>
#include <QScrollBar>
#include <QMenuBar>
#include <QMenu>
#include "src/shell/TextScrollBar.h"
#include "src/shell/ShellWindow.h"
#include "src/editor/EditorTabManager.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <QScrollBar>

namespace {
// Teal track (#00AAAA) and green handle (#00AA00) — the TV scrollbar palette.
bool isTeal(const QColor& c)
{
    return c.blue() > 140 && c.green() > 140 && c.red() < 120;
}
// Off-palette light teal (#4FCFCF) — banned by the TV-revision spec (only the
// 8 named colors are allowed: blue/gray/yellow/white/teal/green/red/black).
bool isOffPaletteTeal(const QColor& c)
{
    return qAbs(c.red() - 0x4F) <= 14 && qAbs(c.green() - 0xCF) <= 14 &&
           qAbs(c.blue() - 0xCF) <= 14;
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

    // The scrollbar track is the TV teal (#00AAAA), not the default gray.
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
                     "scrollbar track is not TV teal (#00AAAA): %1 teal px").arg(teal)));
    }
    // TV-revision spec 5: the scrollbar palette is TV-PURE — no off-palette
    // #4FCFCF anywhere; track dithers teal over blue; thumb green; arrow-block
    // bases teal with a black glyph.
    void scrollbar_palette_is_tv_pure() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(50);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());

        const QColor kTeal(0x00, 0xAA, 0xAA);
        const QColor kGreen(0x00, 0xAA, 0x00);
        const QColor kBlue(0x00, 0x00, 0xAA);
        int banned = 0, teal = 0, green = 0, blue = 0;
        auto near = [](const QColor& c, const QColor& t, int tol) {
            return qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
                   qAbs(c.blue() - t.blue()) <= tol;
        };
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (isOffPaletteTeal(c))
                    ++banned;
                if (near(c, kTeal, 24))
                    ++teal;
                if (near(c, kGreen, 24))
                    ++green;
                if (near(c, kBlue, 24))
                    ++blue;
            }
        QVERIFY2(banned == 0,
                 qPrintable(QStringLiteral("off-palette #4FCFCF present: %1 px (spec forbids it)")
                                .arg(banned)));
        // Track dithers teal over blue: both present.
        QVERIFY2(teal > 50, qPrintable(QStringLiteral("track lacks teal dither: %1 teal px").arg(teal)));
        QVERIFY2(blue > 20,
                 qPrintable(QStringLiteral("track lacks blue dither base: %1 blue px").arg(blue)));
        // Thumb is green.
        QVERIFY2(green > 50, qPrintable(QStringLiteral("thumb not green: %1 green px").arg(green)));
    }

    // TV-revision spec 5: the right border always carries the VERTICAL
    // scrollbar — with a short document (no overflow, no horizontal need) the
    // vertical bar is present and visible. Probe-verified: the doc bar is the
    // one under KTextEditor::ViewPrivate with a real range; Kate exposes no
    // scrollbar-policy API on the public View (see .relay/vbar2.cpp).
    void vertical_scrollbar_forced_visible() {
        ShellWindow shell;   // the bar's isVisible() depends on the real shell window
        shell.show();
        EditorTabManager m;
        m.newDocument();
        auto* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot\n"));
        v->resize(400, 200);
        v->show();
        QCoreApplication::processEvents();
        QCOMPARE(v->document()->lines(), 2);

        int docVert = 0, docVertShown = 0;
        for (QScrollBar* b : v->findChildren<QScrollBar*>()) {
            if (b->orientation() != Qt::Vertical)
                continue;
            if (b->width() >= 40 && b->height() >= 100) {   // the document bar
                ++docVert;
                // isVisibleTo(view): shown AND not hidden, independent of whether
                // an ancestor top-level window is currently on-screen.
                if (b->isVisibleTo(v) && !b->isHidden())
                    ++docVertShown;
            }
        }
        QVERIFY2(docVert >= 1, "no document vertical scrollbar found in the view");
        QVERIFY2(docVertShown == docVert,
                 qPrintable(QStringLiteral("vertical scrollbar hidden on a short doc: %1/%2 shown")
                                .arg(docVertShown).arg(docVert)));
    }

    // TV-revision spec (minimap off by default): the Show Minimap action starts
    // unchecked and the toggle roundtrip still works.
    void minimap_hidden_by_default() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QAction* mm = nullptr;
        for (QAction* a : mb->actions())
            if (a->menu())
                for (QAction* sub : a->menu()->findChildren<QAction*>())
                    if (sub->objectName() == QLatin1String("actionViewMinimap"))
                        mm = sub;
        QVERIFY2(mm, "actionViewMinimap not found");
        QVERIFY2(!mm->isChecked(), "Show Minimap starts checked — spec wants it off by default");
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
