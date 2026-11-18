// SPDX-License-Identifier: GPL-3.0-or-later
// GUI pass 2026-11-17, Task 3: the spell section of the editor right-click
// menu (user, verbatim: "add a section up top (1) where the misspelled word is
// shown in bold with a divider underneath for its section and if you hover over
// it a popout appears in the same style as the right click menu with
// suggestions first, which if clicked can subsitute the word right clicked on,
// and then underneath separated by a divider, the option to add word to
// dictionary, or ignore the word").
//
// Everything here is observable without a pointer: the shell exposes the
// popout directly, and the hover path is driven through ContentBridge exactly
// as main.qml drives it.
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QSignalSpy>
#include <sonnet/speller.h>
#include <ktexteditor/editor.h>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include "src/shell/ShellWindow.h"
#include "src/shell/BorlandMenu.h"
#include "src/shell/MenuBar.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"

namespace {
// Count pixels within `tol` of `want` over a rectangle (or the whole image).
int countColor(const QImage& img, const QRect& r, const QColor& want, int tol)
{
    int n = 0;
    for (int y = qMax(0, r.top()); y <= qMin(img.height() - 1, r.bottom()); ++y)
        for (int x = qMax(0, r.left()); x <= qMin(img.width() - 1, r.right()); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol
                && qAbs(c.green() - want.green()) <= tol
                && qAbs(c.blue() - want.blue()) <= tol)
                ++n;
        }
    return n;
}

// A dictionary Sonnet actually has (tests must not invent a language).
QString firstDictionary()
{
    Sonnet::Speller s;
    return s.availableDictionaries().isEmpty() ? QString()
                                               : s.availableDictionaries().constBegin().value();
}

// Render a menu whose FIRST non-separator row is \p a, through BorlandMenu's
// own paintEvent, and count the ink the row draws. This is what proves the PAINT
// PATH honours the row's bold/plain flags - a helper that re-implements the
// drawing would test itself, not BorlandMenu.
int rowInk(BorlandMenu* menu, QAction* a)
{
    menu->resize(menu->sizeHint().width() + 8, 64);
    menu->show();
    const QImage img = menu->grab().toImage().convertToFormat(QImage::Format_RGB32);
    const QRect r = menu->actionGeometry(a);
    int ink = 0;
    for (int y = r.top(); y <= r.bottom() && y < img.height(); ++y)
        for (int x = r.left(); x <= r.right() && x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            // Black glyph ink on the gray panel (row 0 is never the green bar).
            if (c.red() < 90 && c.green() < 90 && c.blue() < 90)
                ++ink;
        }
    return ink;
}

// Build a throwaway menu with ONE row of \p text, flagged bold or not, rendered
// by BorlandMenu, so bold and plain are compared through the identical path.
int paintInk(BorlandMenu* menu, const QString& text, bool bold)
{
    BorlandMenu probe(menu->parentWidget());
    probe.setObjectName(QStringLiteral("inkProbe"));
    QAction* a = probe.addAction(text);
    BorlandMenu::setRowNoMnemonic(a, true);
    BorlandMenu::setRowBold(a, bold);
    if (bold) {
        QFont f = probe.font();
        f.setBold(true);
        BorlandMenu::setRowFont(a, f);
    }
    return rowInk(&probe, a);
}
} // namespace

class TstSpellmenu : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY2(KTextEditor::Editor::instance(), "no KTextEditor");
        QVERIFY2(!firstDictionary().isEmpty(),
                 "no Sonnet dictionaries - hunspell dicts missing from the image");
    }

    // The section is built ONLY when the right-clicked word is actually
    // misspelled, so a correct word leaves the menu exactly as Task 2 left it.
    void correct_word_adds_no_spell_section()
    {
        ShellWindow shell;
        shell.setSpellLanguage(firstDictionary());
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60),
                                    { QStringLiteral("hello"), 0, 0, 5 });
        QCoreApplication::processEvents();
        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu, "context menu missing");

        QStringList names;
        for (QAction* a : menu->actions())
            names << (a->isSeparator() ? QStringLiteral("---") : a->objectName());
        QVERIFY2(!names.contains(QStringLiteral("spellPopout")),
                 qPrintable(QStringLiteral("a correct word must not open a spell section (has: %1)")
                                .arg(names.join(QStringLiteral(", ")))));
        // Still exactly one separator: Undo/Redo --- Cut/Copy/Paste.
        int seps = 0;
        for (QAction* a : menu->actions())
            if (a->isSeparator())
                ++seps;
        QCOMPARE(seps, 1);
    }

    // The headline: a misspelled word puts the WORD on top, BOLD (the TV bold
    // face is a fixed wider advance - PxPlus IBM VGA8 renders bold by
    // double-strike, so the bold label is measurably WIDER than the same string
    // in the regular face), with a divider under its section.
    void misspelled_word_is_headline_and_bold_with_a_divider()
    {
        ShellWindow shell;
        shell.setSpellLanguage(firstDictionary());
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60),
                                    { QStringLiteral("reibot"), 0, 0, 6 });
        QCoreApplication::processEvents();
        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu, "context menu missing");

        // Top section = the word, first row.
        QVERIFY2(menu->actions().size() >= 2, "spell section did not add rows");
        QAction* head = menu->actions().first();
        QCOMPARE(head->objectName(), QStringLiteral("spellWord"));
        QVERIFY2(head->text().contains(QStringLiteral("reibot")),
                 qPrintable(QStringLiteral("headline text is '%1'").arg(head->text())));

        // Bold is not styleable through QSS (borland.qss carries no QMenu rules
        // on purpose) and NOT reachable through setBold()'s advance: the TV
        // face is a fixed-pitch bitmap, so bold is a DOUBLE-STRIKE (drawn 1px
        // right on top of itself). Prove it by RENDERING the headline row and
        // measuring ink density: a double-struck word puts noticeably more
        // dark pixels in the same cell run than a single-strike one.
        QVERIFY2(BorlandMenu::rowBold(head),
                 "the headline row is not flagged bold");
        QVERIFY2(BorlandMenu::rowNoMnemonic(head),
                 "the headline row still parses hotkeys (a word is not a command)");

        // Render the SAME string twice - bold and not - through BorlandMenu's
        // own paintEvent (not a helper that re-draws it), and require the bold
        // one to put strictly more ink down. The TV face is fixed-pitch bitmap,
        // so bold is a double-strike; on thin strokes that overlaps, so the bar
        // is "fatter", not "twice as fat" - 1.40x was measured.
        const QString word = QStringLiteral("reibot");
        const int inkBold = paintInk(menu, word, true);
        const int inkPlain = paintInk(menu, word, false);
        QVERIFY2(inkBold > inkPlain * 12 / 10,
                 qPrintable(QStringLiteral("double-strike gave no extra ink: bold=%1 plain=%2 "
                                           "(expected bold >= 1.2x plain)")
                                .arg(inkBold).arg(inkPlain)));

        // Divider directly under the section: one full-width separator after
        // the headline, then the suggestion rows.
        const auto acts = menu->actions();
        QVERIFY2(acts.size() >= 3, "spell section has no room for a divider");
        QStringList shape;
        for (int i = 0; i < acts.size(); ++i)
            shape << (acts.at(i)->isSeparator() ? QStringLiteral("---") : acts.at(i)->objectName());
        QVERIFY2(acts.at(1)->isSeparator(),
                 qPrintable(QStringLiteral("row 1 should be the divider under the word, shape: %1")
                                .arg(shape.join(QStringLiteral(", ")))));

        // The popout is a BorlandMenu, so it is literally "in the same style as
        // the right click menu".
        QVERIFY2(qobject_cast<BorlandMenu*>(shell.spellPopout()),
                 "spell popout is not a BorlandMenu");
    }

    // Suggestions come FIRST (above the divider), add-to-dictionary and ignore
    // come BELOW a second divider.
    void popout_layout_is_suggestions_then_dictionary_then_ignore()
    {
        ShellWindow shell;
        shell.setSpellLanguage(firstDictionary());
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60),
                                    { QStringLiteral("reibot"), 0, 0, 6 });
        QCoreApplication::processEvents();
        auto* popout = shell.spellPopout();
        QVERIFY2(popout, "no spell popout for a misspelled word");

        const auto acts = popout->actions();
        QVERIFY2(!acts.isEmpty(), "spell popout has no actions");

        // First divider under the headline section.
        QVERIFY2(acts.first()->isSeparator(),
                 qPrintable(QStringLiteral("expected a divider as row 0, got '%1'")
                                .arg(acts.first()->objectName())));

        int firstSep = -1, secondSep = -1;
        for (int i = 0; i < acts.size(); ++i) {
            if (!acts.at(i)->isSeparator())
                continue;
            if (firstSep < 0)
                firstSep = i;
            else if (secondSep < 0)
                secondSep = i;
        }
        QVERIFY2(firstSep == 0,
                 qPrintable(QStringLiteral("first divider is at row %1, expected 0").arg(firstSep)));
        QVERIFY2(secondSep > 1,
                 qPrintable(QStringLiteral("second divider is at row %1, expected after the "
                                           "suggestions").arg(secondSep)));

        // Below the second divider: exactly the two dictionary commands.
        QStringList tail;
        for (int i = secondSep + 1; i < acts.size(); ++i)
            tail << acts.at(i)->objectName();
        QCOMPARE(tail, QStringList({ QStringLiteral("spellAddToDictionary"),
                                     QStringLiteral("spellIgnoreWord") }));

        // Above it: real Sonnet suggestions for the word (may be empty in a
        // dry dictionary - then only the dividers show).
        int suggestionRows = 0;
        for (int i = firstSep + 1; i < secondSep; ++i)
            if (!acts.at(i)->isSeparator())
                ++suggestionRows;
        Sonnet::Speller sp;
        sp.setLanguage(firstDictionary());
        const QStringList expected = sp.suggest(QStringLiteral("reibot"));
        QVERIFY2(suggestionRows == expected.size(),
                 qPrintable(QStringLiteral("expected %1 suggestions, menu has %2")
                                .arg(expected.size()).arg(suggestionRows)));
        if (suggestionRows > 0)
            QVERIFY2(acts.at(firstSep + 1)->text().contains(expected.first(), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("first suggestion should be '%1', it is '%2'")
                                    .arg(expected.first(), acts.at(firstSep + 1)->text())));
    }

    // Clicking a suggestion REPLACES the word the user right-clicked on.
    void picking_a_suggestion_replaces_the_right_clicked_word()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot second\n"));
        v->document()->setHighlightingMode(QStringLiteral("Plain Text"));

        ShellWindow shell;
        shell.setSpellLanguage(firstDictionary());
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        // Right-click the WORD at (0,0)-(0,6), the way the editor reports it.
        shell.showEditorContextMenu(QPoint(60, 60),
                                    { QStringLiteral("reibot"), 0, 0, 6 });
        QCoreApplication::processEvents();
        auto* popout = shell.spellPopout();
        QVERIFY2(popout, "no spell popout");

        // suggest() returns an UNORDERED set (verified: it returned 'freeboot'
        // while suggest().first() was 'robotize'), so pick the row by matching
        // ANY row against the dictionary, never by index.
        Sonnet::Speller sp;
        sp.setLanguage(firstDictionary());
        const QStringList sugg = sp.suggest(QStringLiteral("reibot"));
        QVERIFY2(!sugg.isEmpty(),
                 "dictionary gave no suggestions for 'reibot' - cannot test replacement");

        // The shell emits the decision; the composition root (crt-editor, which
        // links KTextEditor) performs it. Emulate that link here - the shell
        // deliberately does not link KTextEditor.
        QSignalSpy spy(&shell, &ShellWindow::spellReplacementRequested);
        QObject::connect(&shell, &ShellWindow::spellReplacementRequested, &shell,
                         [v](int line, int startCol, int endCol,
                             const QString& repl) {
                             v->document()->replaceText(
                                 KTextEditor::Range(line, startCol, line, endCol),
                                 repl);
                         });

        // Fire the first suggestion row through the popout's action list (the
        // same QAction the menu row is bound to).
        QAction* pick = nullptr;
        for (QAction* a : popout->actions())
            if (!a->isSeparator()
                && sugg.contains(a->text(), Qt::CaseInsensitive)) {
                pick = a;
                break;
            }
        QVERIFY2(pick, "no suggestion row in the popout matched the dictionary");
        const QString replacement = pick->text();
        pick->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(v->document()->text(), QStringLiteral("%1 second\n").arg(replacement));
    }

    // Add-to-dictionary makes Sonnet accept the word (it is the reason the
    // personal word list exists), and ignore suppresses it in THIS document
    // only.
    void add_to_dictionary_makes_sonnet_accept_the_word()
    {
        // A variant of the word the rest of the suite already proves misspelled
        // ('reibot'), so this test is not silently skipped by a nonce hunspell
        // accepts (that is what made 'xzxzqword' and 'tehquux' skip). It must be
        // a DIFFERENT word from 'reibot', or adding it here would make the
        // hover test below find 'reibot' correct and build no popout.
        const QString word = QStringLiteral("reibott");
        Sonnet::Speller sp;
        sp.setLanguage(firstDictionary());
        if (!sp.isMisspelled(word))
            QSKIP(qPrintable(QStringLiteral("'%1' is not flagged misspelled by this dictionary")
                                 .arg(word)));
        QVERIFY2(sp.addToPersonal(word),
                 qPrintable(QStringLiteral("Sonnet refused to add '%1' to the personal word list")
                                .arg(word)));
        QVERIFY2(sp.isCorrect(word),
                 qPrintable(QStringLiteral("'%1' still not correct after addToPersonal").arg(word)));
        // Put the dictionary back the way we found it.
        Q_UNUSED(sp.save());
    }

    // The popout must be reachable the way the app reaches everything: a
    // synthesized hover through the bridge. No pointer exists on this host, so
    // the bridge IS the input device.
    void hovering_the_spell_row_opens_the_popout_through_the_bridge()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot\n"));
        m.applyTvStyle(v);

        ShellWindow shell;
        // Exactly what main.cpp hands the shell: the TabManager's STACKED
        // container (the raw view is a hidden stacked child until its
        // container is shown - passing `v` directly left it invisible and
        // childAt() fell through to the row's background widget).
        shell.setEditorWidget(m.containerWidget());
        shell.setSpellLanguage(firstDictionary());
        shell.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&shell);
        shell.show();
        QCoreApplication::processEvents();
        shell.setSpellPopupHost(&b);
        // The composition root normally installs this (main.cpp); the test
        // must too, or the bridge has no word to hand the shell and no section
        // is built. Resolve the word under the click from the live document.
        shell.setSpellWordResolver([&shell, v](const QPoint& shellLocal) -> SpellWord {
            const QPoint inView = v->mapFromGlobal(shell.mapToGlobal(shellLocal));
            const KTextEditor::Cursor cur = v->coordinatesToCursor(inView);
            if (!cur.isValid())
                return SpellWord();
            const QString line = v->document()->line(cur.line());
            auto isWordChar = [](QChar c) {
                return c.isLetterOrNumber() || c == QLatin1Char('\'');
            };
            int start = cur.column(), end = cur.column();
            while (start > 0 && isWordChar(line.at(start - 1)))
                --start;
            while (end < line.size() && isWordChar(line.at(end)))
                ++end;
            if (start >= end)
                return SpellWord();
            return SpellWord{ line.mid(start, end - start), cur.line(), start, end };
        });

        // Right-click over the editor through the bridge - the real path.
        // Aim at the WORD itself (line 0, middle of 'reibot'), not the view
        // centre - the centre maps to an empty line where there is no word.
        const QPoint wordView = v->cursorToCoordinate(KTextEditor::Cursor(0, 3));
        QVERIFY2(wordView.x() >= 0 && wordView.y() >= 0,
                 qPrintable(QStringLiteral("word cursor has no coordinate (%1,%2)")
                                .arg(wordView.x()).arg(wordView.y())));
        const QPoint centre = v->mapTo(&shell, wordView);
        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::RightButton,
                            Qt::RightButton, Qt::NoModifier, centre.x(), centre.y());
        QCoreApplication::processEvents();

        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu && menu->isVisible(), "right-click did not open the context menu");
        QVERIFY2(shell.spellPopout(),
                 "the right-clicked word 'reibot' is misspelled, so a spell popout must exist");
        // Register the menu with the bridge so popupAt() resolves the hover to
        // it (main.cpp's PopupRegistrar does this on Show; the test has no
        // registrar, so a hover fell through to the editor and never reached
        // the menu - the same class of delivery bug as the fourth bug pass).
        b.registerPopup(menu);

        // Hover the headline row. forwardMouseEvent takes SHELL-LOCAL coords;
        // the same conversion the working menu tests use (menu->pos() - shell
        // ->pos() + actionGeometry().center()), NOT mapTo(&shell) which would
        // reproduce the parent-chain offset bug.
        QAction* head = nullptr;
        for (QAction* a : menu->actions())
            if (a->objectName() == QLatin1String("spellWord"))
                head = a;
        QVERIFY2(head, "no headline row to hover");
        const QPoint rowCentre =
            menu->pos() - shell.pos() + menu->actionGeometry(head).center();
        b.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                            Qt::NoModifier, rowCentre.x(), rowCentre.y());
        QCoreApplication::processEvents();
        QTRY_VERIFY(shell.spellPopoutVisible());
    }

private:
    QObject* m_unused = nullptr;
};

QTEST_MAIN(TstSpellmenu)
#include "tst_spellmenu.moc"
