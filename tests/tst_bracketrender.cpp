// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: Coloured Brackets, the RENDERER verification. Confirms the
// MovingRange attributes actually paint (measured: a red-attributed bracket
// produced red pixels; lineAttributes() returned 0 and is NOT the render path).
#include <QtTest>
#include <QApplication>
#include <QImage>
#include "src/editor/ColourWiring.h"
#include "src/editor/BracketColorizer.h"
#include "src/editor/StringColorizer.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/editor.h>

namespace {
// Count pixels of a colour within tolerance.
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

// KTextEditor caches each line's attribute layout, so a grab taken right after
// the ranges change can still show the OLD colours (measured: the signal said 0
// spans while the image still had 161 green pixels). Re-setting the text forces
// a FULL relayout - nudging one line is not enough, because only that line's
// cache is dropped and the coloured lines keep their stale attributes.
void forceRelayout(KTextEditor::View* view)
{
    KTextEditor::Document* doc = view->document();
    doc->setText(doc->text());
    view->repaint();
    QCoreApplication::sendPostedEvents();
}
} // namespace

class TstBracketRender : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Deterministic, same as the other renderer suites.
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void a_bracket_pair_paints_its_colour()
    {
        auto* ed = KTextEditor::Editor::instance();
        auto* doc = ed->createDocument(this);
        doc->setText(QStringLiteral("if (aaa) {\n  return bbb;\n}\n"));
        auto* view = doc->createView(nullptr);
        view->resize(500, 300);
        view->show();
        QCoreApplication::processEvents();

        ColourWiring wiring;
        wiring.setView(view);
        QSignalSpy spy(&wiring, &ColourWiring::painted);
        wiring.repaint();
        QVERIFY2(spy.count() >= 1, "repaint never signalled");
        QVERIFY2(spy.last().at(0).toInt() >= 2,
                 qPrintable(QStringLiteral("expected >=2 pairs, painted %1")
                                .arg(spy.last().at(0).toInt())));
        // Force a real paint: grab() re-renders the widget, and the moving-range
        // attributes are applied in the view's paint pass.
        view->repaint();               // synchronous paint pass
        QCoreApplication::sendPostedEvents();

        // Kate palette: index 0 blue (first pair), index 1 orange-red (nested).
        const QImage img = view->grab().toImage();
        const QVector<QColor> palette = BracketColorizer::defaultColors();
        int total = 0;
        for (const QColor& c : palette)
            total += countColor(img, c, 60);   // anti-aliased glyph edges blend
        QVERIFY2(total >= 4,
                 qPrintable(QStringLiteral("no bracket colour pixels found (got %1)")
                                .arg(total)));

        // Off -> no coloured pixels remain (the ranges are dropped).
        wiring.setEnabled(false);
        forceRelayout(view);          // the view caches line attributes
        const QImage off = view->grab().toImage();
        int totalOff = 0;
        for (const QColor& c : palette)
            // Tight tolerance on purpose: at 60 some unrelated theme colour falls
            // inside a palette colour's cube (~149 px of false positives), so a
            // loose match could never reach zero.
            totalOff += countColor(off, c, 12);
        QVERIFY2(totalOff == 0,
                 qPrintable(QStringLiteral("disabling did not remove colours (%1 -> %2)")
                                .arg(total).arg(totalOff)));
        wiring.setEnabled(true);
        wiring.setView(nullptr);
        delete doc;
    }

    void nested_pairs_get_distinct_colours_on_screen()
    {
        auto* ed = KTextEditor::Editor::instance();
        auto* doc = ed->createDocument(this);
        doc->setText(QStringLiteral("a({ [x] })\n"));
        auto* view = doc->createView(nullptr);
        view->resize(500, 300);
        view->show();
        QCoreApplication::processEvents();

        ColourWiring wiring;
        wiring.setView(view);
        wiring.repaint();

        const QImage img = view->grab().toImage();
        const QVector<QColor> palette = BracketColorizer::defaultColors();
        // "a({ [x] })" nests: outer {} inner () and [] - at least 2 distinct
        // palette colours must be on screen simultaneously.
        int distinct = 0;
        for (const QColor& c : palette)
            if (countColor(img, c, 60) > 0)   // anti-aliased glyph edges blend
                ++distinct;
        QVERIFY2(distinct >= 2,
                 qPrintable(QStringLiteral("only %1 distinct bracket colours on screen")
                                .arg(distinct)));
        wiring.setView(nullptr);
        delete doc;
    }

    // 2026-11-20: string colouring independent of the syntax mode. The whole
    // point is a document the theme cannot colour: mode None (a .txt, an
    // extension-less file, a fresh untitled tab). Before the string pass this
    // document painted ZERO green; the span pass must paint the quoted text.
    void quoted_text_paints_on_a_document_with_no_syntax_mode()
    {
        auto* ed = KTextEditor::Editor::instance();
        auto* doc = ed->createDocument(this);
        doc->setHighlightingMode(QStringLiteral("None"));   // the key condition
        doc->setText(QStringLiteral("a = \"alpha\"\nb = 'beta'\n"));
        auto* view = doc->createView(nullptr);
        view->resize(500, 300);
        view->show();
        QCoreApplication::processEvents();

        const QColor green = StringColorizer::defaultColor();

        // Baseline: nothing coloured before the pass runs.
        const QImage before = view->grab().toImage();
        const int beforePx = countColor(before, green, 60);

        ColourWiring wiring;
        wiring.setView(view);
        QSignalSpy spy(&wiring, &ColourWiring::stringsPainted);
        wiring.repaint();
        QVERIFY2(spy.count() >= 1, "string pass never signalled");
        QCOMPARE(spy.last().at(0).toInt(), 2);   // two quoted spans

        view->repaint();                          // synchronous paint pass
        QCoreApplication::sendPostedEvents();
        const QImage after = view->grab().toImage();
        const int afterPx = countColor(after, green, 60);

        QVERIFY2(afterPx > beforePx + 20,
                 qPrintable(QStringLiteral("quoted text did not paint green "
                                           "(before %1 px, after %2 px)")
                                .arg(beforePx).arg(afterPx)));

        // Turning the string pass off drops those ranges again.
        wiring.setStringsEnabled(false);
        forceRelayout(view);          // the view caches line attributes
        QCOMPARE(spy.last().at(0).toInt(), 0);
        const int offPx = countColor(view->grab().toImage(), green, 60);
        QVERIFY2(offPx <= beforePx + 5,
                 qPrintable(QStringLiteral("strings still painted after disabling "
                                           "(%1 px vs baseline %2)")
                                .arg(offPx).arg(beforePx)));

        wiring.setView(nullptr);
        delete doc;
    }

    // A bracket inside a quoted span must stay string-coloured, not get a
    // bracket colour: in a mode-None document nothing else tells us it is text.
    void a_bracket_inside_a_quoted_span_gets_no_bracket_colour()
    {
        auto* ed = KTextEditor::Editor::instance();
        auto* doc = ed->createDocument(this);
        doc->setHighlightingMode(QStringLiteral("None"));
        doc->setText(QStringLiteral("s = \"{a}\"\n"));   // only a quoted pair
        auto* view = doc->createView(nullptr);
        view->resize(500, 300);
        view->show();
        QCoreApplication::processEvents();

        ColourWiring wiring;
        wiring.setView(view);
        QSignalSpy strings(&wiring, &ColourWiring::stringsPainted);
        QSignalSpy pairs(&wiring, &ColourWiring::painted);
        wiring.repaint();
        QCOMPARE(strings.last().at(0).toInt(), 1);   // the quoted span
        QCOMPARE(pairs.last().at(0).toInt(), 0);     // no bracket pair

        wiring.setView(nullptr);
        delete doc;
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstBracketRender tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_bracketrender.moc"
