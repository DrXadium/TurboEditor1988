// SPDX-License-Identifier: GPL-3.0-or-later
// Colour swatch inline notes (2026-11-23).
//
// The editor shows a small colour box right after every #rgb / #rrggbb token,
// the way Kate does. The note belongs to the TEXT, so it is a KTextEditor
// inline note rather than paint we lay over the view: wrapping, folding and
// horizontal scrolling then keep the box on its token for free.
//
// Proves:
//  1. one note sits at the column just AFTER each token;
//  2. a line with no token reports no notes;
//  3. a token ending the line still gets a note (column == line length);
//  4. the painted box carries the token's own colour;
//  5. the provider resolves a note back to its token range (the click path);
//  6. destroying the view with the provider alive is clean (no dangling).
#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QApplication>
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include "src/editor/HexSwatchProvider.h"
#include "src/editor/EditorTabManager.h"
#include "src/editor/HexColorScan.h"

namespace {

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

// A view over a fresh document, sized so several lines are visible.
KTextEditor::View* makeView(QObject* owner, const QString& text)
{
    auto* ed = KTextEditor::Editor::instance();
    auto* doc = ed->createDocument(owner);
    doc->setText(text);
    auto* view = doc->createView(nullptr);
    view->resize(640, 240);
    return view;
}

} // namespace

class TstHexSwatch : public QObject
{
    Q_OBJECT
private slots:
    void view_reports_a_note_after_each_token()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("a #ff0000 b"));
        HexSwatchProvider provider(view);
        view->registerInlineNoteProvider(&provider);

        // "a " puts '#' at 2; seven chars long -> the note sits at column 9.
        const QList<int> notes = provider.inlineNotes(0);
        QCOMPARE(notes.size(), 1);
        QCOMPARE(notes.at(0), 9);

        view->unregisterInlineNoteProvider(&provider);
        delete view;
    }

    void no_note_without_a_token()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("plain text here"));
        HexSwatchProvider provider(view);
        view->registerInlineNoteProvider(&provider);

        QVERIFY2(provider.inlineNotes(0).isEmpty(), "swatch on a line with no colour");

        view->unregisterInlineNoteProvider(&provider);
        delete view;
    }

    void note_survives_at_end_of_line()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("#00ff00"));
        HexSwatchProvider provider(view);
        view->registerInlineNoteProvider(&provider);

        const QList<int> notes = provider.inlineNotes(0);
        QCOMPARE(notes.size(), 1);
        QCOMPARE(notes.at(0), 7);   // == line length

        view->unregisterInlineNoteProvider(&provider);
        delete view;
    }

    void resolves_a_note_back_to_its_token()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("color: #00ff00;"));
        HexSwatchProvider provider(view);
        view->registerInlineNoteProvider(&provider);

        HexColorToken token;
        QVERIFY2(provider.tokenAt(0, 0, &token), "note did not resolve to a token");
        QCOMPARE(token.start, 7);
        QCOMPARE(token.length, 7);
        QCOMPARE(token.color, QColor(0x00, 0xff, 0x00));

        view->unregisterInlineNoteProvider(&provider);
        delete view;
    }

    void painted_swatch_carries_the_token_colour()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("#ff0000 and #0000ff"));
        HexSwatchProvider provider(view);
        view->registerInlineNoteProvider(&provider);

        // Draw the view the way the shell does, then look for each colour.
        QImage img(view->size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            view->render(&p);
        }
        const int reds = countColor(img, QColor(0xff, 0x00, 0x00), 40);
        const int blues = countColor(img, QColor(0x00, 0x00, 0xff), 40);
        QVERIFY2(reds >= 4, qPrintable(QStringLiteral("no red swatch ink (%1 px)").arg(reds)));
        QVERIFY2(blues >= 4, qPrintable(QStringLiteral("no blue swatch ink (%1 px)").arg(blues)));

        view->unregisterInlineNoteProvider(&provider);
        delete view;
    }

    // The swatches are only real if the manager attaches a provider to every
    // view it creates - the initial tab, every new document, and pane 1's view.
    // Without this the feature exists but nothing paints it.
    void manager_registers_a_provider_for_every_view()
    {
        EditorTabManager m;
        KTextEditor::View* v0 = m.activeView();
        QVERIFY2(v0, "no initial view");
        QVERIFY2(m.swatchProvider(v0), "initial view has no swatch provider");
        QCOMPARE(m.swatchProvider(v0)->inlineNotes(0).size(), 0);

        m.newDocument();
        KTextEditor::View* v1 = m.activeView();
        QVERIFY(v1 && v1 != v0);
        QVERIFY2(m.swatchProvider(v1), "a new tab's view has no swatch provider");

        v1->document()->setText(QStringLiteral("x #123456"));
        QCOMPARE(m.swatchProvider(v1)->inlineNotes(0).size(), 1);
    }

    void destroying_the_view_with_the_provider_alive_is_clean()
    {
        KTextEditor::View* view = makeView(this, QStringLiteral("#123456"));
        auto* provider = new HexSwatchProvider(view, view);
        view->registerInlineNoteProvider(provider);
        QCOMPARE(provider->inlineNotes(0).size(), 1);

        // The view owns the provider here; deleting the view must not leave the
        // view pointing at freed memory.
        delete view;
        QCoreApplication::processEvents();
    }
};

QTEST_MAIN(TstHexSwatch)
#include "tst_hexswatch.moc"
