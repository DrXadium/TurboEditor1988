// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (TV revision): the editor field must be a FLAT blue TV canvas with
// YELLOW text and NO black — no black line-number gutter, no black current-line
// band, and a lavender selection block (spec sections 1 + 4).
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QHash>
#include <algorithm>
#include <ktexteditor/editor.h>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include "src/editor/EditorTabManager.h"

namespace {
QColor near(const QImage& img, int x, int y)
{
    return img.pixelColor(x, y);
}

// Fraction of pixels in the whole image that are "pure black" (all channels
// near 0), split into GUTTER (x<60: line-number strip) and REST (everything
// else: current-line band / margins), so a failure says WHERE black lives.
struct BlackStats {
    double fraction = 1.0;
    QString detail;
};
BlackStats blackStats(const QImage& img)
{
    qint64 gutter = 0, rest = 0, total = qint64(img.width()) * img.height();
    QHash<int, int> rows; // rest-black per row (top offenders)
    QStringList coords; // first black pixel coordinates anywhere
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() < 16 && c.green() < 16 && c.blue() < 16) {
                if (coords.size() < 8)
                    coords << QStringLiteral("(%1,%2)").arg(x).arg(y);
                if (x < 60)
                    ++gutter;
                else {
                    ++rest;
                    rows[y]++;
                }
            }
        }
    BlackStats s;
    s.fraction = total ? double(gutter + rest) / total : 0.0;
    QList<QPair<int, int>> top;
    for (auto it = rows.cbegin(); it != rows.cend(); ++it)
        top.append({ it.value(), it.key() });
    std::sort(top.begin(), top.end(), std::greater<>());
    for (int i = 0; i < top.size() && i < 3; ++i)
        s.detail += QStringLiteral("[row %1: %2 black] ").arg(top[i].second).arg(top[i].first);
    s.detail += QStringLiteral("gutter=%1 rest=%2 first=%3")
                    .arg(gutter)
                    .arg(rest)
                    .arg(coords.join(QLatin1Char(' ')));
    return s;
}

// Count pixels whose color is within `tol` of `want`.
int countColor(const QImage& img, const QColor& want, int tol)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol
                && qAbs(c.green() - want.green()) <= tol
                && qAbs(c.blue() - want.blue()) <= tol)
                ++n;
        }
    return n;
}
} // namespace

class TstTveditor : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY2(KTextEditor::Editor::instance(), "no KTextEditor");
    }

    // Spec 1: editor area is ONE flat #1A1AAA — zero black anywhere (catches a
    // black gutter or black current-line band).
    void editor_field_is_flat_tv_blue()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot\nsecond line\nthird\n"));
        v->resize(700, 400);
        QImage img = v->grab().toImage().convertToFormat(QImage::Format_RGB32);
        QVERIFY(img.width() > 0);

        const BlackStats bs = blackStats(img);
        QVERIFY2(bs.fraction < 0.001,
                 qPrintable(QStringLiteral("editor [%1x%2] has %3% black px (expected <0.1%): %4")
                                .arg(img.width())
                                .arg(img.height())
                                .arg(bs.fraction * 100, 0, 'f', 3)
                                .arg(bs.detail)));

        // Interior background must be TV window blue #1A1AAA.
        const QColor blue(0x1A, 0x1A, 0xAA);
        const int bg = countColor(img, blue, 12);
        QVERIFY2(bg > (img.width() * img.height()) / 3,
                 qPrintable(QStringLiteral("only %1 px of window-blue (expected a large flat field)").arg(bg)));
    }

    // Spec 4: editor text is yellow #FFFF55 (not gray).
    void editor_text_is_yellow()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot\n"));
        v->resize(700, 400);
        QImage img = v->grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor yellow(0xFF, 0xFF, 0x55);
        const int n = countColor(img, yellow, 40);
        QVERIFY2(n > 20,
                 qPrintable(QStringLiteral("only %1 yellow text pixels (expected yellow editor text, text is probably still gray)").arg(n)));
    }

    // Spec 4: selection is a light gray-lavender block #C8C8D8.
    void selection_is_lavender()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot reibot reibot\n"));
        v->setSelections({ KTextEditor::Range(0, 0, 0, 18) });
        v->resize(700, 400);
        QImage img = v->grab().toImage().convertToFormat(QImage::Format_RGB32);
        const QColor lav(0xC8, 0xC8, 0xD8);
        const int n = countColor(img, lav, 14);
        QVERIFY2(n > 100,
                 qPrintable(QStringLiteral("only %1 lavender selection px (expected a #C8C8D8 selection block)").arg(n)));
    }
    // Spec 6 addendum: KTextEditor's status chip strip is hidden entirely.
    void chips_status_bar_hidden()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        bool foundBar = false, barHidden = false;
        for (QWidget* w : v->findChildren<QWidget*>()) {
            const char* cls = w->metaObject()->className();
            if (qstrcmp(cls, "KateViewBar") == 0 || qstrcmp(cls, "KateStatusBar") == 0) {
                foundBar = true;
                barHidden = !w->isVisibleTo(v);
            }
        }
        QVERIFY2(foundBar, "KateViewBar not found (KTE layout changed?)");
        QVERIFY2(barHidden, "KateViewBar (green chip row) is visible — spec says omit");
    }
};

QTEST_MAIN(TstTveditor)
#include "tst_tveditor.moc"
