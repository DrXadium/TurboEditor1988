// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (TV revision): the editor field must be a FLAT blue TV canvas with
// YELLOW text and NO black — no black line-number gutter, no black current-line
// band, and a lavender selection block (spec sections 1 + 4).
#include <QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QImage>
#include <QClipboard>
#include <QTemporaryDir>
#include <QHash>
#include <QFontInfo>
#include <QFontDatabase>
#include <QSet>
#include <algorithm>
#include <climits>
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

    // Task 21: a tab chip's green square closes that tab. The active index
    // follows the removal, and closing the LAST tab leaves one fresh untitled
    // document - the strip and the editor never go empty.
    // Task 25 (user): Kate's untitled-tab behaviour. As soon as there is text in
    // a document that has no file, the tab names itself from the first TEN
    // characters of what was typed - and keeps doing so until a real filename
    // exists, at which point the filename wins.
    void untitled_tab_names_itself_from_the_typed_text()
    {
        EditorTabManager m;          // the constructor creates one document
        QSignalSpy spy(&m, &EditorTabManager::tabsChanged);
        KTextEditor::Document* doc = m.activeDocument();
        QVERIFY(doc);

        auto title = [&spy] {
            return spy.isEmpty() ? QString()
                                 : spy.last().at(0).toStringList().value(0);
        };
        QCOMPARE(m.tabCount(), 1);

        // Typing gives the tab a name: exactly the first ten characters.
        doc->setText(QStringLiteral("hello world and more"));
        QVERIFY2(!spy.isEmpty(), "typing did not refresh the tab titles");
        QCOMPARE(title(), QStringLiteral("hello worl"));

        // The name follows the buffer while it is still untitled.
        doc->setText(QStringLiteral("goodbye"));
        QCOMPARE(title(), QStringLiteral("goodbye"));

        // A short first line is the whole name - the name never runs past the
        // line break into the next line's text.
        doc->setText(QStringLiteral("short\nsecond line"));
        QCOMPARE(title(), QStringLiteral("short"));

        // Leading whitespace is not part of the name.
        doc->setText(QStringLiteral("   indented"));
        QCOMPARE(title(), QStringLiteral("indented"));

        // An empty buffer is untitled again.
        doc->setText(QString());
        QCOMPARE(title(), QStringLiteral("untitled"));

        // Saving gives it a real filename, which then wins.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QUrl target = QUrl::fromLocalFile(dir.filePath(QStringLiteral("notes.txt")));
        QVERIFY(doc->saveAs(target));
        QCOMPARE(title(), QStringLiteral("notes.txt"));
    }

    void closing_a_tab_removes_it_and_keeps_one_document()
    {
        EditorTabManager m;          // the constructor creates one document
        m.newDocument();             // 2
        m.newDocument();             // 3
        QCOMPARE(m.tabCount(), 3);
        QCOMPARE(m.containerWidget()->count(), 3);
        QCOMPARE(m.activeIndex(), 2);

        QSignalSpy spy(&m, &EditorTabManager::tabsChanged);
        m.closeTab(1);               // a tab BEFORE the active one
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.tabCount(), 2);
        QCOMPARE(m.containerWidget()->count(), 2);
        QCOMPARE(m.activeIndex(), 1);
        QVERIFY(m.activeView() && m.activeDocument());

        m.closeTab(1);               // the ACTIVE tab: the next takes its place
        QCOMPARE(m.tabCount(), 1);
        QCOMPARE(m.activeIndex(), 0);
        QVERIFY2(m.activeView() && m.activeDocument(), "no active document after closing");

        m.closeTab(0);               // the LAST tab: replaced, not left empty
        QCOMPARE(m.tabCount(), 1);
        QCOMPARE(m.containerWidget()->count(), 1);
        QCOMPARE(m.activeIndex(), 0);
        QVERIFY2(m.activeView() && m.activeDocument(), "closing the last tab left no editor");

        // Out-of-range indices are a no-op, not a crash.
        m.closeTab(-1);
        m.closeTab(99);
        QCOMPARE(m.tabCount(), 1);
    }

    // Edit menu must offer standard Cut / Copy / Paste. This KTextEditor build
    // ships no ClipboardInterface and no action hooks, so the functions are
    // implemented over Document::text/removeText/insertText + QClipboard.
    void cut_copy_paste_round_trip()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        KTextEditor::Document* doc = v->document();

        doc->setText(QStringLiteral("alpha beta\nsecond\n"));
        // Select "beta" on line 0 (columns 6..10).
        const KTextEditor::Range sel(0, 6, 0, 10);
        QVERIFY(v->setSelection(sel));

        // COPY: selection survives, clipboard now holds "beta".
        m.copy();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("beta"));
        QCOMPARE(doc->text(sel), QStringLiteral("beta"));

        // CUT: selection is removed, clipboard still holds "beta".
        m.cut();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("beta"));
        QCOMPARE(doc->text().left(6), QStringLiteral("alpha "));

        // PASTE: the clipboard text lands at the cursor.
        doc->setText(QStringLiteral("alpha second\n"));
        v->setCursorPosition(KTextEditor::Cursor(0, 6));
        m.paste();
        QCOMPARE(doc->text().left(10), QStringLiteral("alpha beta"));
    }

    // Paste must work when the clipboard was filled by ANOTHER application -
    // that is the user's actual path ("can't paste into the app to test").
    void paste_accepts_external_clipboard_text()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QGuiApplication::clipboard()->setText(QStringLiteral("from elsewhere"));
        v->document()->setText(QString());
        v->setCursorPosition(KTextEditor::Cursor(0, 0));
        m.paste();
        QVERIFY2(v->document()->text().startsWith(QStringLiteral("from elsewhere")),
                 qPrintable(v->document()->text()));
    }

    // Undo / redo. This KTextEditor build exposes NO undo API at all - no
    // UndoInterface, no action hooks (checked against the installed headers) -
    // so the history is maintained here over Document::setText.
    void undo_redo_round_trip()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        KTextEditor::Document* doc = v->document();

        // A fresh document has no history: the buttons start inert.
        QCOMPARE(m.canUndo(), false);
        QCOMPARE(m.canRedo(), false);
        m.undo();                        // safe no-op, must not lose text
        QCOMPARE(doc->text(), QString());

        doc->setText(QStringLiteral("alpha\n"));
        doc->setText(QStringLiteral("alpha beta\n"));
        QCOMPARE(m.canUndo(), true);
        QCOMPARE(m.canRedo(), false);

        m.undo();
        QCOMPARE(doc->text(), QStringLiteral("alpha\n"));
        QCOMPARE(m.canRedo(), true);

        m.redo();
        QCOMPARE(doc->text(), QStringLiteral("alpha beta\n"));
        QCOMPARE(m.canRedo(), false);
    }

    // A new edit after an undo must discard the redo branch, as every editor
    // does - otherwise Redo would replay a line the user already replaced.
    void new_edit_after_undo_clears_redo()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::Document* doc = m.activeView()->document();

        doc->setText(QStringLiteral("one\n"));
        doc->setText(QStringLiteral("two\n"));
        m.undo();
        QCOMPARE(doc->text(), QStringLiteral("one\n"));
        QCOMPARE(m.canRedo(), true);

        doc->setText(QStringLiteral("three\n"));
        QCOMPARE(m.canRedo(), false);
        m.redo();                        // must do nothing
        QCOMPARE(doc->text(), QStringLiteral("three\n"));
    }

    // User bug: Ctrl+Z does nothing when the QWidget editor has focus. The
    // undo machinery works (round-trip above), and the bridge dispatches the
    // menu shortcut when IT receives the key - but a real user's keypress lands
    // on the focused QWidget editor, which KTextEditor does not route to our
    // undo. Sending a Ctrl+Z QKeyEvent straight at the view must still revert
    // the last edit: that is what the user's keystroke does.
    void ctrl_z_reaches_undo_while_the_view_has_focus()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        KTextEditor::Document* doc = v->document();
        QVERIFY(v);

        doc->setText(QStringLiteral("alpha\n"));
        doc->setText(QStringLiteral("alpha beta\n"));
        QVERIFY(m.canUndo());

        // Focus the widget the way a click does, then deliver the keystroke to
        // it directly - not through ContentBridge (the QML Keys handler never
        // runs once the QWidget owns focus).
        v->setFocus(Qt::OtherFocusReason);
        QKeyEvent ctrlZ(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier,
                        QStringLiteral("z"));
        QApplication::sendEvent(v, &ctrlZ);

        QCOMPARE(doc->text(), QStringLiteral("alpha\n"));
    }

    // Spec 1: editor area is ONE flat #0000AA — zero black anywhere (catches a
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

        // Interior background must be TV window blue #0000AA.
        const QColor blue(0x00, 0x00, 0xAA);
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

    // The editor text must use the bundled Turbo Vision face: the rest of the
    // chrome (menu, tabs, borders, scrollbars) is drawn with PxPlus IBM VGA8 on
    // an 8 px character grid, and a different editor face made the middle of
    // the window look narrower than the chrome around it.
    void editor_uses_the_pxplus_face()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);

        const QFont f = v->configValue(QStringLiteral("font")).value<QFont>();
        const QString family = QFontInfo(f).family();
        QVERIFY2(family == QStringLiteral("PxPlus IBM VGA8"),
                 qPrintable(QStringLiteral("editor font is '%1', expected the bundled "
                                           "PxPlus IBM VGA8 face").arg(family)));
        QVERIFY2(f.pixelSize() > 0 && f.pixelSize() == 16,
                 qPrintable(QStringLiteral("editor font pixel size is %1, expected 16 "
                                           "(the chrome cell height)")
                                .arg(f.pixelSize())));

        // The config value is only half of it: the WIDGET must actually use the
        // face, or the editor still renders with the system font.
        const QString widgetFamily = QFontInfo(v->font()).family();
        QVERIFY2(widgetFamily == QStringLiteral("PxPlus IBM VGA8"),
                 qPrintable(QStringLiteral("editor widget renders with '%1' although its "
                                           "font config says '%2'")
                                .arg(widgetFamily, family)));
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
    // The bottom band's "L:C" must follow the ACTIVE view. main.cpp connected
    // it to the FIRST view only, so with a restored session (two files opened
    // on top of the initial untitled document) the indicator sat at 1:1
    // forever while the user edited a different document.
    void active_cursor_signal_follows_the_active_view()
    {
        EditorTabManager m;
        QSignalSpy spy(&m, &EditorTabManager::activeCursorPositionChanged);
        KTextEditor::View* first = m.activeView();
        QVERIFY2(first, "no initial view");

        // A fresh document is EMPTY: setCursorPosition clamps the column to the
        // line length and refuses a line past doc()->lines(), so with no text
        // the caret does not move and cursorPositionChanged never fires. Seed
        // real content first (a fresh untitled tab has at least the user's first
        // line before the indicator means anything).
        first->document()->setText(QStringLiteral("first line\nsecond line\nthird line"));
        first->setCursorPosition(KTextEditor::Cursor(0, 3));
        QVERIFY2(spy.count() >= 1, "moving the cursor emitted nothing");
        QCOMPARE(spy.last().at(0).toInt(), 0);
        QCOMPARE(spy.last().at(1).toInt(), 3);

        // A new tab becomes the active view: the signal must follow it.
        m.newDocument();
        KTextEditor::View* second = m.activeView();
        QVERIFY2(second && second != first, "newDocument did not switch views");
        spy.clear();
        second->document()->setText(QStringLiteral("one\ntwo\nthree\nfour\nfive"));
        second->setCursorPosition(KTextEditor::Cursor(2, 3));
        QVERIFY2(spy.count() >= 1, "the new active view's cursor emitted nothing");
        QCOMPARE(spy.last().at(0).toInt(), 2);
        QCOMPARE(spy.last().at(1).toInt(), 3);

        // The tab that is no longer active must not drive the indicator.
        spy.clear();
        first->setCursorPosition(KTextEditor::Cursor(2, 7));   // "third line", col 7 < 10
        QCOMPARE(spy.count(), 0);
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

    // Task 6: the editor caret must BLINK like a real terminal cursor. Kate
    // starts its blink timer only in KateViewInternal::focusInEvent - which
    // never fires on this shell (the window is never mapped, so Qt never
    // delivers FocusIn). Without a delivered FocusIn the caret is painted solid
    // and never toggles. Prove the blink by grabbing the caret's pixels twice
    // across more than one flash interval: they must DIFFER.
    void editor_caret_blinks()
    {
        const int saved = QApplication::cursorFlashTime();
        QApplication::setCursorFlashTime(200);

        EditorTabManager m;
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("blink me"));
        v->setCursorPosition(KTextEditor::Cursor(0, 0));
        // The shell is never mapped, so the view is never "exposed" - grab()
        // renders it offscreen regardless. Wait for a layout pass instead.
        v->resize(400, 200);
        QCoreApplication::processEvents();

        // Isolate the CARET on its own ROW (Kate's cursorPositionCoordinates
        // gives the caret's y). The text is static (no typing), so the only
        // thing that can change that row's ink over time is the caret blinking
        // on/off. Count non-background pixels on the caret's row across flash
        // intervals; a blinking caret makes the count toggle, a solid one does
        // not. Sampling the whole view is useless (unrelated repaints change
        // hundreds of pixels: measured 755 over 100x98).
        const QPoint caret = v->cursorPositionCoordinates();
        const int row = caret.y();
        const QColor bg(0x00, 0x00, 0xAA);   // window blue

        // Warm-up grab first: the very first render can differ from steady
        // state (unpainted -> painted), which is not a blink. Discard it.
        v->grab();
        QCoreApplication::processEvents();

        // Count how many DISTINCT ink states that cell takes over STEADY-STATE
        // time. A blinking caret takes two (on/off); a solid one takes one.
        QSet<QRgb> states;
        for (int attempt = 0; attempt < 6; ++attempt) {
            const QImage img = v->grab().toImage().convertToFormat(QImage::Format_RGB32);
            // Count non-background pixels on the caret's row (text + caret).
            int ink = 0;
            const int y = qBound(0, row, img.height() - 1);
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (!(c.red() < 16 && c.green() < 16 && c.blue() > 0x80))   // not blue bg
                    ++ink;
            }
            states.insert(QRgb(ink));   // distinct ink COUNTS over time
            QTest::qWait(QApplication::cursorFlashTime() + 40);
            QCoreApplication::processEvents();
        }

        QApplication::setCursorFlashTime(saved);
        QVERIFY2(states.size() >= 2,
                 qPrintable(QStringLiteral("caret row took only %1 distinct ink count(s) over time - "
                                            "it is solid, not blinking")
                                .arg(states.size())));
    }
};

QTEST_MAIN(TstTveditor)
#include "tst_tveditor.moc"
