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
#include <QTemporaryDir>
#include <QFile>
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include "src/editor/HexSwatchProvider.h"
#include "src/editor/EditorTabManager.h"
#include "src/shell/HexColorScan.h"

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

    // Xadium 2026-11-23: "I do not see the box" - on a file he had OPENED.
    // addTabForUrl() (File > Open, and every session-restored file) built its
    // view with its own copy of the setup sequence, which never attached a
    // provider: only the blank initial tab and File > New had swatches.
    void opened_files_get_swatches_too()
    {
        EditorTabManager m;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("styled.html")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("body { background-color: #0f0f0f; }\n");
        f.close();

        m.openFile(QUrl::fromLocalFile(f.fileName()));
        // openUrl() on a local file completes through the event loop.
        QTRY_VERIFY_WITH_TIMEOUT(m.activeDocument()
                                     && m.activeDocument()->url().toLocalFile() == f.fileName(),
                                 3000);

        KTextEditor::View* view = m.activeView();
        QVERIFY2(view, "opened file has no view");
        QVERIFY2(m.swatchProvider(view),
                 "a file opened from disk has no swatch provider (no box on screen)");
        QCOMPARE(m.swatchProvider(view)->inlineNotes(0).size(), 1);
    }

    // The app's own view is STYLED (applyTvStyle) and lives inside the
    // manager's stack, unlike the bare views above. Render that one, so "no box
    // on screen" cannot come back through a styling or parenting difference.
    void the_managers_styled_view_paints_the_swatch()
    {
        EditorTabManager m;
        QVERIFY(m.activeDocument());
        m.activeDocument()->setText(QStringLiteral("body { background-color: #0f0f0f; }"));
        KTextEditor::View* view = m.activeView();
        QVERIFY(view);
        view->resize(700, 120);
        QCoreApplication::processEvents();

        QImage img(view->size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            view->render(&p);
        }
        // #0f0f0f is a near-black swatch on window blue (#0000AA): count dark
        // neutral pixels, which the text and the blue field never produce.
        int dark = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 40 && c.green() < 40 && c.blue() < 60)
                    ++dark;
            }
        QVERIFY2(dark >= 4,
                 qPrintable(QStringLiteral("no swatch ink in the styled view (%1 px)").arg(dark)));
    }

    // Xadium 2026-11-23: "while the dialog pops up you can't click in it".
    //
    // The chain from QML is SYNCHRONOUS: QML onPressed -> simulateMousePress()
    // -> ContentBridge -> KTextEditor::InlineNoteProvider::inlineNoteActivated
    // -> swatchClicked. The picker was then opened INSIDE that stack frame via
    // TvModal::exec(), so QML stayed blocked on the pressed() handler and could
    // not emit another press or release for the whole life of the dialog -
    // every click inside it was simply never delivered.
    //
    // The click must therefore survive past its own handler: swatchClicked has
    // to be a QUEUED handoff, so the pressing frame returns and the dialog is
    // opened on a later turn of the event loop.
    void the_click_handoff_is_queued_not_synchronous()
    {
        EditorTabManager m;
        QVERIFY(m.activeDocument());
        m.activeDocument()->setText(QStringLiteral("a #123456"));
        KTextEditor::View* view = m.activeView();
        QVERIFY(view);
        HexSwatchProvider* provider = m.swatchProvider(view);
        QVERIFY(provider);

        bool sawSignal = false;
        // DIRECT, deliberately: this observer must run the instant the signal
        // is emitted, so that "the flag is still false when activateToken()
        // returns" really means the handoff was deferred rather than merely
        // observed late.
        QObject::connect(&m, &EditorTabManager::swatchClicked, &m,
                         [&sawSignal]() { sawSignal = true; },
                         Qt::DirectConnection);
        QVERIFY(!sawSignal);

        // Drive the provider's click path (KTextEditor builds InlineNote itself,
        // so this seam stands in for the click that reaches it).
        HexColorToken token;
        token.start = 2;
        token.length = 7;
        token.color = QColor(0x12, 0x34, 0x56);
        provider->activateToken(token, 0, QRect(10, 10, 8, 16));
        // The pressing frame has returned. A DIRECT handoff would already have
        // fired here - that is the bug. A queued one cannot have.
        QVERIFY2(!sawSignal,
                 "swatchClicked fired inside the press handler; the picker's modal loop "
                 "would then run inside QML's onPressed and block all further input");
        QTRY_VERIFY_WITH_TIMEOUT(sawSignal, 1000);
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
