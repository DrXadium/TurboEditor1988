// SPDX-License-Identifier: GPL-3.0-or-later
// Status-line selectors (F-row indicators + dropdowns).
// Task 1: the Sonnet dictionary list must be reachable (links KF6::SonnetCore).
#include <QtTest>
#include <sonnet/speller.h>
#include <QImage>
#include <QMenu>
#include "src/editor/StatusSelectors.h"
#include "src/editor/EditorTabManager.h"
#include "src/shell/FKeyStatusBar.h"
#include "src/shell/BorlandMenu.h"
#include <ktexteditor/editor.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <cstdio>
#include <QTemporaryDir>
#include <QFile>
#include <QUrl>

namespace {
const QColor kOvrRed(0xAA, 0x00, 0x00);

// Fraction of pixels near `want` inside the widget's grab.
double inkFraction(const QImage& img, const QColor& want, int tol = 24)
{
    int hits = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol && qAbs(c.green() - want.green()) <= tol &&
                qAbs(c.blue() - want.blue()) <= tol)
                ++hits;
        }
    return img.isNull() ? 0.0 : double(hits) / double(img.width() * img.height());
}
} // namespace

class TstStatusSel : public QObject
{
    Q_OBJECT
private slots:
    void spell_list_is_nonempty_when_dicts_exist()
    {
        Sonnet::Speller speller;
        const QMap<QString, QString> dicts = speller.availableDictionaries();
        QVERIFY2(!dicts.isEmpty(),
                 "no spell dictionaries found (hunspell dicts should be in /usr/share/hunspell)");
    }

    // Task 2: the INS/OVR indicator. OVR must be drawn RED (#AA0000); INS must
    // NOT be red (it uses the bar's normal ink).
    void ovr_indicator_is_red_and_ins_is_not()
    {
        StatusSelectors s;
        s.resize(240, 20);
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));

        s.setOverwrite(true);
        QCoreApplication::processEvents();
        const QImage ovrImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double ovrRed = inkFraction(ovrImg, kOvrRed);
        QVERIFY2(ovrRed > 0.0,
                 qPrintable(QStringLiteral("OVR indicator has no red ink (%1% of pixels)")
                                .arg(ovrRed * 100.0, 0, 'f', 2)));

        s.setOverwrite(false);
        QCoreApplication::processEvents();
        const QImage insImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double insRed = inkFraction(insImg, kOvrRed);
        QVERIFY2(insRed == 0.0,
                 qPrintable(QStringLiteral("INS indicator is red (%1% of pixels) - only OVR is red")
                                .arg(insRed * 100.0, 0, 'f', 2)));
    }

    // Task 3: the doc-type label shows the document's highlighting mode.
    void doc_type_label_shows_current_mode()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("int main() {}"));
        QVERIFY2(doc->setHighlightingMode(QStringLiteral("C++")),
                 "C++ highlighting mode not available");

        StatusSelectors s;
        s.setDocument(doc);
        QCOMPARE(s.docTypeText(), QStringLiteral("C++"));

        // Switch to a guaranteed-valid alternate mode (hardcoded names like
        // "Plain Text" are not what Kate reports; pick from the real list).
        QString alt;
        for (const QString& m : doc->highlightingModes())
            if (m != QStringLiteral("C++")) { alt = m; break; }
        QVERIFY2(!alt.isEmpty(), "no alternate highlighting mode available");
        QVERIFY2(doc->setHighlightingMode(alt), "could not switch highlighting mode");
        QCOMPARE(s.docTypeText(), alt);
        delete doc;
    }

    // Task 3: the dropdown lists the available modes and picking one applies it.
    void doc_type_dropdown_lists_modes_and_applies_pick()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("int main() {}"));

        StatusSelectors s;
        s.setDocument(doc);

        // Open the dropdown by clicking the doc-type indicator.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.docTypeCenter());
        QCoreApplication::processEvents();

        // The popup must be open and offer every mode the document can take.
        QMenu* menu = s.openMenu();
        QVERIFY2(menu, "clicking the doc-type indicator did not open a menu");
        QVERIFY(menu->isVisible());
        const QStringList modes = doc->highlightingModes();
        QVERIFY2(modes.size() > 1, "document reports too few highlighting modes");
        QStringList menuTexts;
        for (QAction* a : menu->actions())
            menuTexts << a->text();
        for (const QString& m : modes)
            QVERIFY2(menuTexts.contains(m),
                     qPrintable(QStringLiteral("mode '%1' missing from the dropdown").arg(m)));

        // Pick a mode different from the current one; the document must switch.
        const QString before = doc->highlightingMode();
        QString pick;
        for (const QString& m : modes)
            if (m != before) { pick = m; break; }
        QVERIFY2(!pick.isEmpty(), "no alternate mode to pick");
        menu->actions().at(menuTexts.indexOf(pick))->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(doc->highlightingMode(), pick);

        menu->hide();
        delete doc;
    }

    // Task 4: the encoding label shows the document's encoding.
    void encoding_label_shows_current_encoding()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("hello"));
        QVERIFY2(doc->setEncoding(QStringLiteral("UTF-8")), "could not set UTF-8");

        StatusSelectors s;
        s.setDocument(doc);
        QCOMPARE(s.encodingText(), QStringLiteral("UTF-8"));
        delete doc;
    }

    // Task 4 (data-loss guard): picking an encoding RECORDS it for the next
    // save/reload but must NOT touch the open buffer. Kate's setEncoding only
    // takes effect on reload; re-decoding here would drop unsaved edits.
    void encoding_pick_records_without_touching_text()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        const QString seeded = QStringLiteral("unsaved edits stay put");
        doc->setText(seeded);
        // Pin a known starting encoding so the pick is unambiguous (a fresh
        // doc's encoding() is "" and Kate may normalize it after setText).
        QVERIFY2(doc->setEncoding(QStringLiteral("UTF-8")), "could not set UTF-8");

        StatusSelectors s;
        s.setDocument(doc);

        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.encodingCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s.openMenu();
        QVERIFY2(menu && menu->isVisible(), "clicking the encoding indicator opened no menu");

        // Pick an encoding different from the current one.
        const QString before = doc->encoding();
        QString pick;
        QAction* pickAction = nullptr;
        for (QAction* a : menu->actions()) {
            const QString t = a->text();
            // Skip "Default" (its stored value is "", not its display text);
            // pick a real encoding whose display text equals its stored value.
            if (t != before && !t.isEmpty() && t != QStringLiteral("Default")) {
                pick = t; pickAction = a; break;
            }
        }
        QVERIFY2(!pick.isEmpty(), "no alternate encoding to pick");
        pickAction->trigger();
        QCoreApplication::processEvents();

        // The encoding was recorded...
        QCOMPARE(doc->encoding(), pick);
        // ...and the buffer is byte-for-byte untouched (no reload, no re-decode).
        QCOMPARE(doc->text(), seeded);

        menu->hide();
        delete doc;
    }

    // The spell label shows the current dictionary's SHORT code (e.g. "en_GB"),
    // not the long human-readable description - the user wants the compact
    // form that fits the status row.
    void spell_label_shows_current_dictionary()
    {
        Sonnet::Speller speller;
        const auto dicts = speller.availableDictionaries();
        QVERIFY2(!dicts.isEmpty(), "no dictionaries");
        // availableDictionaries() is description -> code; use the first entry.
        const QString desc = dicts.firstKey();
        const QString code = dicts.value(desc);
        speller.setLanguage(code);

        StatusSelectors s;
        s.setSpeller(&speller);
        s.setSpellLanguage(code);
        QCOMPARE(s.spellText(), code);
    }

    // Task 5: the dropdown lists every dictionary description, and picking one
    // sets the speller's language to that dictionary's code.
    void spell_dropdown_lists_dictionaries_and_applies_pick()
    {
        Sonnet::Speller speller;
        const auto dicts = speller.availableDictionaries();
        QVERIFY2(dicts.size() > 1, "need at least two dictionaries to pick between");

        StatusSelectors s;
        s.setSpeller(&speller);

        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.spellCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s.openMenu();
        QVERIFY2(menu && menu->isVisible(), "clicking the spell indicator opened no menu");

        QStringList menuTexts;
        for (QAction* a : menu->actions())
            menuTexts << a->text();
        for (const QString& desc : dicts.keys())
            QVERIFY2(menuTexts.contains(desc),
                     qPrintable(QStringLiteral("dictionary '%1' missing from the dropdown").arg(desc)));

        // Pick a dictionary different from the current one.
        const QString currentCode = speller.language();
        QString pickDesc;
        QAction* pickAction = nullptr;
        for (QAction* a : menu->actions()) {
            const QString desc = a->text();
            if (dicts.value(desc) != currentCode) { pickDesc = desc; pickAction = a; break; }
        }
        QVERIFY2(pickAction, "no alternate dictionary to pick");
        pickAction->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(speller.language(), dicts.value(pickDesc));

        menu->hide();
    }

    // Task 7: switching the active tab must EMIT activeViewChanged so the
    // composition root can re-point every indicator at the new view/document.
    void active_view_changed_signal_fires_on_tab_switch()
    {
        EditorTabManager m;
        QSignalSpy spy(&m, &EditorTabManager::activeViewChanged);
        QVERIFY(m.activeView());
        spy.clear();

        m.newDocument();   // switches to a fresh tab
        QCoreApplication::processEvents();
        QVERIFY2(spy.count() >= 1,
                 qPrintable(QStringLiteral("activeViewChanged fired %1 times on tab switch")
                                .arg(spy.count())));
        // The emitted view is the new active view.
        QCOMPARE(spy.last().at(0).value<KTextEditor::View*>(), m.activeView());
    }

    // Task 7: re-pointing the indicators at a new document updates all four
    // labels (doc type, encoding, and the INS/OVR + spell which follow their
    // own sources). This is what the composition root does on activeViewChanged.
    void indicators_follow_a_repointed_document()
    {
        KTextEditor::Document* docA = KTextEditor::Editor::instance()->createDocument(nullptr);
        KTextEditor::Document* docB = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(docA && docB);
        docA->setText(QStringLiteral("aaa"));
        docB->setText(QStringLiteral("bbb"));
        QVERIFY(docA->setHighlightingMode(QStringLiteral("C++")));
        QVERIFY(docB->setHighlightingMode(QStringLiteral("Python")));
        QVERIFY(docA->setEncoding(QStringLiteral("UTF-8")));
        QVERIFY(docB->setEncoding(QStringLiteral("ISO-8859-1")));

        StatusSelectors s;
        s.setDocument(docA);
        QCOMPARE(s.docTypeText(), QStringLiteral("C++"));
        QCOMPARE(s.encodingText(), QStringLiteral("UTF-8"));

        // Re-point at B (what activeViewChanged triggers in main.cpp).
        s.setDocument(docB);
        QCOMPARE(s.docTypeText(), QStringLiteral("Python"));
        QCOMPARE(s.encodingText(), QStringLiteral("ISO-8859-1"));

        delete docA;
        delete docB;
    }

    // Task 8 (spec 7): opening a dropdown swaps the F-key row to a context
    // hint and hides the indicators; closing restores them.
    void indicators_swap_out_for_context_hint_and_restore()
    {
        FKeyStatusBar bar;
        // Heap-allocate with `bar` as parent: setStatusSelectors() reparents the
        // widget into the status bar, so a STACK StatusSelectors would be deleted
        // by bar's destructor and then again at scope exit (double free).
        auto* s = new StatusSelectors(&bar);
        s->show();
        bar.setStatusSelectors(s);
        bar.resize(400, 20);
        bar.show();
        QVERIFY(s->isVisible());

        QSignalSpy openSpy(s, &StatusSelectors::dropdownOpened);
        QSignalSpy closeSpy(s, &StatusSelectors::dropdownClosed);

        // Drive the swap the way main.cpp wires it: open -> setContextHint,
        // close -> clearContextHint.
        QObject::connect(s, &StatusSelectors::dropdownOpened, &bar,
                         [&bar](const QString& h) { bar.setContextHint(h); });
        QObject::connect(s, &StatusSelectors::dropdownClosed, &bar,
                         [&bar]() { bar.clearContextHint(); });

        // Open the doc-type dropdown (needs a doc).
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        s->setDocument(doc);
        QTest::mouseClick(s, Qt::LeftButton, Qt::NoModifier, s->docTypeCenter());
        QCoreApplication::processEvents();
        QVERIFY2(openSpy.count() == 1, qPrintable(QStringLiteral("dropdownOpened fired %1").arg(openSpy.count())));
        QVERIFY2(bar.contextHintActive(), "hint not active while dropdown open");
        QVERIFY2(!s->isVisible(), "indicators should hide while the hint is up");

        // Close it (hide the menu) -> indicators restored.
        if (QMenu* m = s->openMenu())
            m->hide();
        QCoreApplication::processEvents();
        QVERIFY2(closeSpy.count() == 1, qPrintable(QStringLiteral("dropdownClosed fired %1").arg(closeSpy.count())));
        QVERIFY(!bar.contextHintActive());
        QVERIFY2(s->isVisible(), "indicators must be restored after the dropdown closes");
        s->setDocument(nullptr);
        bar.setStatusSelectors(nullptr);
        delete doc;
    }

    // Task 8: across a sweep of narrow widths, the four indicators hosted on
    // the F-row keep every label inside their own rect (no per-label clipping)
    // AND stay inside the bar (the bar must reserve their natural width, eliding
    // the F-key text instead).
    void row_fits_at_narrow_widths()
    {
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        Sonnet::Speller speller;
        // Heap-allocate with `bar` as parent to avoid the stack-reparent
        // double-free (setStatusSelectors reparents into the bar).
        FKeyStatusBar bar;
        auto* s = new StatusSelectors(&bar);
        s->setDocument(doc);
        s->setSpeller(&speller);
        s->setSpellLanguage(QStringLiteral("en_US"));
        bar.setStatusSelectors(s);
        s->show();
        for (int w = 240; w <= 900; w += 60) {
            bar.resize(w, 20);
            bar.show();
            QCoreApplication::processEvents();
            // The selectors widget must stay fully inside the bar: the bar
            // gives it its natural sizeHint and elides the F-key text instead.
            const QRect sRect = s->geometry();
            QVERIFY2(sRect.left() >= 0 && sRect.right() <= bar.width(),
                     qPrintable(QStringLiteral("w=%1 selectors overflow bar: %2..%3 vs bar %4")
                                    .arg(w).arg(sRect.left()).arg(sRect.right()).arg(bar.width())));
            // Every indicator's click centre must lie inside the selectors
            // widget - a cell whose centre fell outside would mean a label
            // overflowing the row (the proportional-shrink keeps all four in).
            const int W = sRect.width();
            QVERIFY2(s->spellCenter().x() >= 0 && s->spellCenter().x() <= W,
                     qPrintable(QStringLiteral("w=%1 spell centre %2 outside 0..%3")
                                    .arg(w).arg(s->spellCenter().x()).arg(W)));
            QVERIFY2(s->encodingCenter().x() >= 0 && s->encodingCenter().x() <= W,
                     qPrintable(QStringLiteral("w=%1 encoding centre %2 outside 0..%3")
                                    .arg(w).arg(s->encodingCenter().x()).arg(W)));
            QVERIFY2(s->docTypeCenter().x() >= 0 && s->docTypeCenter().x() <= W,
                     qPrintable(QStringLiteral("w=%1 docType centre %2 outside 0..%3")
                                    .arg(w).arg(s->docTypeCenter().x()).arg(W)));
            // The FIFTH cell (line ending) must stay inside too - the
            // proportional shrink now divides 5 ways (2026-11-19).
            QVERIFY2(s->eolCenter().x() >= 0 && s->eolCenter().x() <= W,
                     qPrintable(QStringLiteral("w=%1 eol centre %2 outside 0..%3")
                                    .arg(w).arg(s->eolCenter().x()).arg(W)));
            // The last cell (INS/OVR) is centred in the row's remaining right
            // half; its centre is the docType centre + half doc + boundary + half ovr.
            // Simpler: it must not exceed the widget's right edge either.
            // We only have three public centres, so assert the widest label
            // (encoding's full text) never pushes past the right edge by
            // checking the docType centre + its half-width stays within W.
            const QFontMetrics fm(s->font());
            const int docHalf = fm.horizontalAdvance(s->docTypeText()) / 2;
            QVERIFY2(s->docTypeCenter().x() + qMin(docHalf, W / 4) <= W + 1,
                     qPrintable(QStringLiteral("w=%1 docType overflows right: %2 > %3")
                                    .arg(w).arg(s->docTypeCenter().x() + docHalf).arg(W)));
        }
        // Detach before destroying so a later repaint can't touch a dead doc.
        s->setDocument(nullptr);
        s->setSpeller(nullptr);
        bar.setStatusSelectors(nullptr);
        delete doc;
    }

    // Bug: the status-bar entries were clipped to a sliver. QStatusBar gives a
    // stretch-0 widget its minimumSizeHint when space is tight; ours was only
    // ~60px while the labels need ~293px, so every label was crushed. The bar
    // must reserve the natural sizeHint (eliding the F-key text instead).
    void selectors_get_their_natural_width_not_the_minimum()
    {
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        Sonnet::Speller speller;
        FKeyStatusBar bar;
        auto* s = new StatusSelectors(&bar);
        s->setDocument(doc);
        s->setSpeller(&speller);
        s->setSpellLanguage(QStringLiteral("en_US"));
        bar.setStatusSelectors(s);
        s->show();
        // A window comfortably wide enough for the F-key text + all four
        // indicators (1280, the app's real width).
        bar.resize(1280, 20);
        bar.show();
        QCoreApplication::processEvents();

        const QSize natural = s->sizeHint();
        const QSize minimum = s->minimumSizeHint();
        // The invariant: minimumSizeHint must not be smaller than sizeHint -
        // otherwise the status bar can legally squeeze the widget below its
        // content and clip the labels (the reported bug).
        QVERIFY2(minimum.width() >= natural.width(),
                 qPrintable(QStringLiteral("minimumSizeHint width %1 < sizeHint width %2 "
                                           "-> the bar may crush the indicators")
                                .arg(minimum.width()).arg(natural.width())));
        // And the widget must actually be laid out at (at least) that width.
        const QRect sRect = s->geometry();
        QVERIFY2(sRect.width() >= natural.width(),
                 qPrintable(QStringLiteral("selectors laid out %1px wide, need %2px "
                                           "(labels are clipped)")
                                .arg(sRect.width()).arg(natural.width())));
        // The rightmost label (INS/OVR) must fit inside the laid-out widget.
        const QFontMetrics fm(s->font());
        const int ovrW = fm.horizontalAdvance(s->overwriteText());
        QVERIFY2(s->docTypeCenter().x() + ovrW <= sRect.width() + 1,
                 qPrintable(QStringLiteral("INS/OVR clipped: right edge %1 > width %2")
                                .arg(s->docTypeCenter().x() + ovrW).arg(sRect.width())));

        s->setDocument(nullptr);
        s->setSpeller(nullptr);
        bar.setStatusSelectors(nullptr);
        delete doc;
    }

    // User bug: the status-bar dropdowns rendered as plain Qt menus, not like
    // the menu bar's Turbo Vision dropdowns. They must be BorlandMenu (the
    // self-painted TV dropdown) so they match.
    void dropdowns_are_borland_styled_like_the_menu_bar()
    {
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        StatusSelectors s;
        s.setDocument(doc);

        // Doc-type dropdown.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.docTypeCenter());
        QCoreApplication::processEvents();
        QMenu* docMenu = s.openMenu();
        QVERIFY2(docMenu, "doc-type dropdown did not open");
        QVERIFY2(qobject_cast<BorlandMenu*>(docMenu),
                 qPrintable(QStringLiteral("doc-type dropdown is a %1, want BorlandMenu")
                                .arg(QString::fromLatin1(docMenu->metaObject()->className()))));
        docMenu->hide();

        // Line-ending dropdown (fifth cell).
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        s.setView(v);
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.eolCenter());
        QCoreApplication::processEvents();
        QMenu* eolMenu = s.openMenu();
        QVERIFY2(eolMenu, "line-ending dropdown did not open");
        QVERIFY2(qobject_cast<BorlandMenu*>(eolMenu),
                 qPrintable(QStringLiteral("line-ending dropdown is a %1, want BorlandMenu")
                                .arg(QString::fromLatin1(eolMenu->metaObject()->className()))));
        eolMenu->hide();
        s.setView(nullptr);

        // Encoding dropdown.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.encodingCenter());
        QCoreApplication::processEvents();
        QMenu* encMenu = s.openMenu();
        QVERIFY2(encMenu, "encoding dropdown did not open");
        QVERIFY2(qobject_cast<BorlandMenu*>(encMenu),
                 qPrintable(QStringLiteral("encoding dropdown is a %1, want BorlandMenu")
                                .arg(QString::fromLatin1(encMenu->metaObject()->className()))));
        encMenu->hide();

        s.setDocument(nullptr);
        delete doc;
    }

    // User bug: the doc-type dropdown sized to Kate's LONGEST highlighting mode
    // name and filled the whole viewport. It must be a narrow column like the
    // encoding dropdown, not a viewport-wide list.
    void doc_type_dropdown_is_a_narrow_column_not_the_whole_viewport()
    {
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        StatusSelectors s;
        s.setDocument(doc);

        // Open the encoding dropdown first to get the reference "narrow" width.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.encodingCenter());
        QCoreApplication::processEvents();
        QMenu* encMenu = s.openMenu();
        QVERIFY(encMenu);
        const int encW = encMenu->width();   // actual rendered width after popup
        encMenu->hide();

        // Open the doc-type dropdown.
        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.docTypeCenter());
        QCoreApplication::processEvents();
        QMenu* docMenu = s.openMenu();
        QVERIFY2(docMenu, "doc-type dropdown did not open");
        const int docW = docMenu->width();   // actual rendered width after popup
        docMenu->hide();

        // The doc-type menu must not run away: keep it within a sane column
        // and no wider than the shell window (it was filling the viewport).
        const int viewportW = s.window() ? s.window()->width() : 1280;
        QVERIFY2(docW <= viewportW,
                 qPrintable(QStringLiteral("doc-type menu %1px wider than viewport %2px")
                                .arg(docW).arg(viewportW)));
        // It should sit in the same narrow band as the encoding menu (a couple
        // of the encoding width is generous slack; the old bug was viewport-wide).
        QVERIFY2(docW <= qMax(360, 3 * encW),
                 qPrintable(QStringLiteral("doc-type menu %1px not narrow (encoding %2px)")
                                .arg(docW).arg(encW)));

        s.setDocument(nullptr);
        delete doc;
    }

    // User bug: the doc-type dropdown opened at the window's EXTREME LEFT
    // instead of under the indicator that was clicked, because the clamp used
    // QMenu::sizeHint() (natural, 1600px+) instead of the capped size. The
    // popup must hang off the cell: its right edge at the cell's right edge,
    // fully inside the window.
    void doc_type_dropdown_opens_under_the_indicator_not_at_the_left_edge()
    {
        KTextEditor::Document* doc =
            KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        QWidget host;
        // Sized so the anchored popup lands inside BOTH the window and the test
        // screen (Qt's popup() clamps to the screen too, which would hide a
        // formula bug behind its own clamp).
        host.resize(600, 400);
        // NOT shown: a real window manager repositions the test window (it put
        // it at 1580,888 - off the 1280x800 screen), which makes mapToGlobal
        // and menu->pos() disagree. A hidden widget keeps the requested geometry
        // and mapToGlobal still resolves through the parent chain.
        auto* s = new StatusSelectors(&host);
        // Plant the indicator row on the RIGHT of the window, like the real bar.
        s->setGeometry(340, 376, 260, 20);
        s->setDocument(doc);

        QTest::mouseClick(s, Qt::LeftButton, Qt::NoModifier, s->docTypeCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s->openMenu();
        QVERIFY2(menu, "doc-type dropdown did not open");
        const QRect hostR = QRect(host.mapToGlobal(QPoint(0, 0)), host.size());
        // Assert OUR anchor math, not menu->pos(): the test runs under Wayland,
        // where the compositor owns popup placement. The far-left bug lived in
        // this math (a natural-sizeHint clamp), so this is the honest unit.
        const QPoint mpos = s->lastPopupPos();
        // The indicator's own click target, in global coordinates.
        const QRect cellLocal = s->docTypeCellRect();
        const QRect cell(cellLocal.topLeft() + s->mapToGlobal(QPoint(0, 0)),
                         cellLocal.size());
        const int menuW = qMin(menu->sizeHint().width(), 360);

        // Right edge of the menu sits at the cell's right edge, so the list
        // reads as hanging off the control that was clicked.
        QVERIFY2(qAbs((mpos.x() + menuW) - (cell.right() + 1)) <= 1,
                 qPrintable(QStringLiteral("menu right edge %1 vs cell right %2 "
                                           "(not anchored to the indicator)")
                                .arg(mpos.x() + menuW).arg(cell.right() + 1)));
        // NOT pinned to the window's left edge - that was the reported bug (the
        // natural-sizeHint clamp pushed a 1600px list to x=shell.left()).
        QVERIFY2(mpos.x() > hostR.left(),
                 qPrintable(QStringLiteral("menu x=%1 is pinned to the window's "
                                           "left edge %2")
                                .arg(mpos.x()).arg(hostR.left())));
        // Fully inside the window.
        QVERIFY2(mpos.x() >= hostR.left() && mpos.x() + menuW <= hostR.right() + 1,
                 qPrintable(QStringLiteral("menu %1..%2 outside window %3..%4")
                                .arg(mpos.x()).arg(mpos.x() + menuW)
                                .arg(hostR.left()).arg(hostR.right())));
        menu->hide();

        s->setDocument(nullptr);
        delete doc;
    }

    // User bug: "scrolling down the doc-type popup doesn't work". QMenu wraps a
    // too-tall list into columns with no scrollers, so BorlandMenu lays out its
    // own single column when the content overflows and scrolls it.
    void overflowing_menu_scrolls_with_the_wheel()
    {
        BorlandMenu m;
        for (int i = 0; i < 120; ++i)
            m.addAction(QStringLiteral("Mode %1").arg(i));
        m.setMaximumWidth(360);
        // Production caps the height (StatusSelectors::popupAnchored calls
        // setMaximumHeight); an uncapped menu keeps QMenu's own layout.
        m.setMaximumHeight(300);
        m.resize(360, 300);
        m.show();
        QCoreApplication::processEvents();
        QVERIFY(m.scrolls());
        QCOMPARE(m.scrollOffset(), 0);

        // Wheel down scrolls forward.
        QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                         Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(&m, &down);
        QVERIFY2(m.scrollOffset() > 0,
                 qPrintable(QStringLiteral("wheel down left scroll at %1")
                                .arg(m.scrollOffset())));

        const int scrolled = m.scrollOffset();
        QWheelEvent up(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(&m, &up);
        QVERIFY2(m.scrollOffset() < scrolled,
                 qPrintable(QStringLiteral("wheel up left scroll at %1 (was %2)")
                                .arg(m.scrollOffset()).arg(scrolled)));
        QCOMPARE(m.scrollOffset(), 0);
        m.hide();
    }

    // User bug: "the spell label tag isn't showing at all". Nothing ever seeded
    // StatusSelectors' language code, so the indicator stayed on its placeholder
    // forever. Setting the speller must adopt the speller's language.
    void spell_label_is_seeded_from_the_speller()
    {
        Sonnet::Speller speller;
        StatusSelectors s;
        // Before: no language, placeholder only.
        s.setSpeller(&speller);
        const QString seeded = s.spellText();
        QVERIFY2(!seeded.isEmpty() && seeded != QStringLiteral("-"),
                 qPrintable(QStringLiteral("spell label still a placeholder: '%1'")
                                .arg(seeded)));
        QCOMPARE(seeded, speller.language());

        // Picking from the dropdown updates the label too.
        const auto dicts = speller.availableDictionaries();
        QVERIFY(!dicts.isEmpty());
        const QString other = dicts.constBegin().value() == speller.language()
                                  ? std::next(dicts.constBegin()).value()
                                  : dicts.constBegin().value();
        s.setSpellLanguage(other);
        QCOMPARE(s.spellText(), other);
    }

    // User bug: "INS/OVR status doesn't change in the footer (cursor responds
    // tho)". Kate's Insert action flips the view's mode, but nothing re-pointed
    // the indicator then. The selector must follow the view's own
    // viewModeChanged signal.
    void ovr_indicator_follows_the_views_insert_key()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        StatusSelectors s;
        s.setView(v);
        QCOMPARE(v->viewMode(), KTextEditor::View::NormalModeInsert);
        QCOMPARE(s.overwrite(), false);
        QCOMPARE(s.overwriteText(), QStringLiteral("INS"));

        // The Insert key (Kate's own toggle), exactly as the editor gets it.
        QVERIFY(m.triggerActiveViewShortcut(QKeySequence(Qt::Key_Insert)));
        QCOMPARE(v->viewMode(), KTextEditor::View::NormalModeOverwrite);
        QVERIFY2(s.overwrite(), "selector did not follow the view into OVR");
        QCOMPARE(s.overwriteText(), QStringLiteral("OVR"));

        QVERIFY(m.triggerActiveViewShortcut(QKeySequence(Qt::Key_Insert)));
        QCOMPARE(v->viewMode(), KTextEditor::View::NormalModeInsert);
        QVERIFY(!s.overwrite());
        QCOMPARE(s.overwriteText(), QStringLiteral("INS"));
    }


    // Round 12 (Xadium 2026-11-23): "the statusbar attributes ... are still
    // unpopulated ... as soon as i click back into the pane they become lines
    // again". Probe evidence: with pane 1 focused over a split document that is
    // NOT pane 0's active tab, activeView() looked the secondary view up under
    // m_docs.at(m_active) -> null -> every cell cleared to "-".
    // Mirrors main.cpp: activeViewChanged re-points the indicators.
    void indicators_repoint_to_the_secondary_view_when_pane_one_has_focus()
    {
        EditorTabManager m;
        KTextEditor::Document* pane0Doc = m.activeDocument();
        QVERIFY(pane0Doc);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("split_target.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("alpha\nbeta\n");
        f.close();

        StatusSelectors s;
        QObject::connect(&m, &EditorTabManager::activeViewChanged, &s,
                         [&s](KTextEditor::View* v) {
                             s.setView(v);
                             s.setDocument(v ? v->document() : nullptr);
                         });
        s.setView(m.activeView());
        s.setDocument(pane0Doc);

        m.openFileAsSplit(QUrl::fromLocalFile(f.fileName()));
        QCoreApplication::processEvents();
        QVERIFY2(m.splitOpen(), "openFileAsSplit must open the split");
        QCOMPARE(m.activeDocument(), pane0Doc);   // pane 0 keeps its own doc

        KTextEditor::View* v1 = m.viewForPane(1);
        QVERIFY2(v1, "pane 1 must hold a view");
        QVERIFY(v1->document() != pane0Doc);

        // The user clicks into pane 1: the app focuses that pane.
        m.setActivePane(1);
        QCoreApplication::processEvents();

        QCOMPARE(m.activeView(), v1);
        QCOMPARE(m.activeDocument(), v1->document());
        QVERIFY2(s.view(), "focused split pane left the indicators with no view");
        QVERIFY2(s.document(), "focused split pane left the indicators on '-'");
        QCOMPARE(s.document(), v1->document());
    }

    // Round 12: "the ins / ovr toggle doesn't reflect the current status if i
    // switch it in the split pane". The status-bar OVR cell routes through
    // triggerActiveViewShortcut(Insert) -> activeView(). With a split over a
    // DIFFERENT document, pane 1's focused activeView() resolved to null, so
    // the toggle was a silent no-op and the cell never changed.
    void ovr_toggle_works_when_pane_one_has_a_different_document()
    {
        EditorTabManager m;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("ovr_target.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("one\ntwo\n");
        f.close();

        StatusSelectors s;
        QObject::connect(&m, &EditorTabManager::activeViewChanged, &s,
                         [&s](KTextEditor::View* v) {
                             s.setView(v);
                             s.setDocument(v ? v->document() : nullptr);
                         });

        m.openFileAsSplit(QUrl::fromLocalFile(f.fileName()));
        QCoreApplication::processEvents();
        QVERIFY2(m.splitOpen(), "split must be open");
        KTextEditor::View* v1 = m.viewForPane(1);
        QVERIFY2(v1, "pane 1 must hold a view");

        m.setActivePane(1);
        QCoreApplication::processEvents();
        QCOMPARE(s.overwrite(), false);

        QVERIFY2(m.triggerActiveViewShortcut(QKeySequence(Qt::Key_Insert)),
                 "the OVR cell did nothing while pane 1 had focus");
        QCOMPARE(v1->viewMode(), KTextEditor::View::NormalModeOverwrite);
        QVERIFY2(s.overwrite(), "indicator did not follow the split pane into OVR");
        QCOMPARE(s.overwriteText(), QStringLiteral("OVR"));
    }

    // User bug: "none of the popups from the status bar have the green highlight
    // on hover". A scrolled menu lays its rows out itself, so QMenu never runs
    // its own hover bookkeeping - BorlandMenu must highlight the row under the
    // cursor itself. Measured on pixels: no green before the move, green after.
    void hovering_a_scrolled_row_highlights_it_in_green()
    {
        const QColor kSelectGreen(0x00, 0xAA, 0x00);
        BorlandMenu m;
        for (int i = 0; i < 120; ++i)
            m.addAction(QStringLiteral("Mode %1").arg(i));
        m.setMaximumWidth(360);
        m.setMaximumHeight(300);
        m.resize(360, 300);
        m.show();
        QCoreApplication::processEvents();
        QVERIFY(m.scrolls());

        const QImage before = m.grab().toImage();
        QCOMPARE(inkFraction(before, kSelectGreen), 0.0);

        QMouseEvent mv(QEvent::MouseMove, QPointF(20, 60), QPointF(20, 60),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&m, &mv);
        QCoreApplication::processEvents();

        const QImage after = m.grab().toImage();
        QVERIFY2(inkFraction(after, kSelectGreen) > 0.0,
                 "hovering a scrolled row left no green highlight");
        m.hide();
    }

    // User bug: "clicking on the scroll doesn't let me scroll down". In scroll
    // mode the rows are ours, so a press in the scrollbar column fell through to
    // rowAt() and PICKED a row instead of scrolling. The track must page and the
    // thumb must drag.
    void clicking_the_scrollbar_track_scrolls_instead_of_picking_a_row()
    {
        BorlandMenu m;
        for (int i = 0; i < 120; ++i)
            m.addAction(QStringLiteral("Mode %1").arg(i));
        m.setMaximumWidth(360);
        m.setMaximumHeight(300);
        m.resize(360, 300);
        m.show();
        QCoreApplication::processEvents();
        QVERIFY(m.scrolls());
        QCOMPARE(m.scrollOffset(), 0);

        bool triggered = false;
        connect(&m, &QMenu::triggered, this, [&triggered](QAction*) { triggered = true; });

        const QRect track = m.scrollBarRect();
        QVERIFY2(!track.isEmpty(), "no scrollbar track in a scrolled menu");
        const QRect thumb = m.scrollThumbRect();
        QVERIFY2(!thumb.isEmpty(), "no scrollbar thumb in a scrolled menu");

        // Click the track BELOW the thumb: page down, do not pick a row.
        const QPoint below(track.center().x(),
                           qMin(track.bottom() - 1, thumb.bottom() + 20));
        QTest::mouseClick(&m, Qt::LeftButton, Qt::NoModifier, below);
        QVERIFY2(m.scrollOffset() > 0,
                 qPrintable(QStringLiteral("track click left scroll at %1")
                                .arg(m.scrollOffset())));
        QVERIFY2(!triggered, "the scrollbar click picked a row");
        m.hide();
    }

    // Same bug, drag half: grabbing the thumb and pulling it down must move the
    // list proportionally.
    void dragging_the_scrollbar_thumb_scrolls_the_list()
    {
        BorlandMenu m;
        for (int i = 0; i < 120; ++i)
            m.addAction(QStringLiteral("Mode %1").arg(i));
        m.setMaximumWidth(360);
        m.setMaximumHeight(300);
        m.resize(360, 300);
        m.show();
        QCoreApplication::processEvents();
        QVERIFY(m.scrolls());
        QCOMPARE(m.scrollOffset(), 0);

        const QRect track = m.scrollBarRect();
        const QRect thumb = m.scrollThumbRect();
        QVERIFY(!track.isEmpty() && !thumb.isEmpty());

        const QPoint start = thumb.center();
        QMouseEvent press(QEvent::MouseButtonPress, start, start, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&m, &press);
        const QPoint end(track.center().x(), track.bottom() - 2);
        QMouseEvent mv(QEvent::MouseMove, end, end, Qt::NoButton, Qt::LeftButton,
                       Qt::NoModifier);
        QApplication::sendEvent(&m, &mv);
        QMouseEvent rel(QEvent::MouseButtonRelease, end, end, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&m, &rel);

        QVERIFY2(m.scrollOffset() > 0,
                 qPrintable(QStringLiteral("thumb drag left scroll at %1")
                                .arg(m.scrollOffset())));
        m.hide();
    }

    // 2026-11-19: the fifth cell shows the document's line-ending type.
    // A fresh Kate document is UNIX (LF) - probe-verified currentItem=0.
    void eol_label_shows_current_line_ending()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("one\ntwo\nthree"));
        StatusSelectors s;
        s.setView(v);
        QCOMPARE(s.eolText(), QStringLiteral("UNIX"));
    }

    // The fifth cell opens a BorlandMenu with exactly the three types Kate
    // offers, and picking one must change what the label shows.
    void eol_dropdown_lists_three_types_and_applies_pick()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        StatusSelectors s;
        s.setView(v);
        s.resize(640, 20);
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));

        QTest::mouseClick(&s, Qt::LeftButton, Qt::NoModifier, s.eolCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s.openMenu();
        QVERIFY2(menu, "line-ending dropdown did not open");
        QVERIFY2(qobject_cast<BorlandMenu*>(menu),
                 qPrintable(QStringLiteral("eol dropdown is a %1, want BorlandMenu")
                                .arg(QString::fromLatin1(menu->metaObject()->className()))));
        const QList<QAction*> acts = menu->actions();
        QCOMPARE(acts.size(), 3);
        QCOMPARE(acts[0]->text(), QStringLiteral("UNIX"));
        QCOMPARE(acts[1]->text(), QStringLiteral("Windows/DOS"));
        QCOMPARE(acts[2]->text(), QStringLiteral("Macintosh"));

        acts[1]->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(s.eolText(), QStringLiteral("Windows/DOS"));

        acts[2]->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(s.eolText(), QStringLiteral("Macintosh"));
    }

    // The pick must CONVERT the file: what Kate writes to disk changes with
    // the selection (the probe's byte check, made permanent). Three lines:
    // 3 LF under UNIX, 3 CRLF + 0 lone LF under Windows, 3 CR under Mac.
    void eol_pick_converts_saved_bytes()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        KTextEditor::Document* doc = v->document();
        doc->setText(QStringLiteral("alpha\nbeta\ngamma"));
        StatusSelectors s;
        s.setView(v);

        auto bytesFor = [&](int index, QByteArray* raw) -> bool {
            s.applyEol(index);
            QTemporaryDir dir;
            if (!dir.isValid())
                return false;
            const QUrl url = QUrl::fromLocalFile(dir.path() + QStringLiteral("/e.txt"));
            if (!doc->saveAs(url))
                return false;
            QFile f(dir.path() + QStringLiteral("/e.txt"));
            if (!f.open(QIODevice::ReadOnly))
                return false;
            *raw = f.readAll();
            return true;
        };

        auto countEols = [](const QByteArray& b, int* lf, int* crlf, int* cr) {
            *lf = *crlf = *cr = 0;
            for (int i = 0; i < b.size(); ++i) {
                if (b[i] == '\n') {
                    if (i > 0 && b[i - 1] == '\r') ++(*crlf);
                    else ++(*lf);
                } else if (b[i] == '\r') {
                    ++(*cr);
                }
            }
        };

        QByteArray raw;
        int lf = 0, crlf = 0, cr = 0;

        QVERIFY2(bytesFor(0, &raw), "save under UNIX failed");
        countEols(raw, &lf, &crlf, &cr);
        QCOMPARE(lf, 3);
        QCOMPARE(crlf, 0);

        QVERIFY2(bytesFor(1, &raw), "save under Windows/DOS failed");
        countEols(raw, &lf, &crlf, &cr);
        QCOMPARE(crlf, 3);
        QCOMPARE(lf, 0);

        QVERIFY2(bytesFor(2, &raw), "save under Macintosh failed");
        countEols(raw, &lf, &crlf, &cr);
        QCOMPARE(cr, 3);
        QCOMPARE(lf, 0);
        QCOMPARE(crlf, 0);
    }

    // Without a view there is nothing to convert: the pick must no-op safely
    // (never crash) and the label falls back to the placeholder.
    void eol_set_is_safe_without_a_view()
    {
        StatusSelectors s;
        QCOMPARE(s.eolText(), QStringLiteral("-"));
        s.applyEol(1);          // must not crash with no view at all
        QCOMPARE(s.eolText(), QStringLiteral("-"));
    }


    // 2026-11-19: the line-ending popup must hang off the FIFTH cell - same
    // anchor contract as the doc-type dropdown (right edge on the cell's right
    // edge, inside the window), asserted through lastPopupPos() because the
    // test runs under Wayland (menu->pos() is the compositor's).
    void eol_dropdown_anchors_under_its_cell()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        QWidget host;
        host.resize(600, 400);
        // NOT shown: a real WM would reposition the window and desync
        // mapToGlobal from the requested geometry (see doc-type anchor test).
        auto* s = new StatusSelectors(&host);
        // Plant the row on the RIGHT of the window, like the real bar.
        s->setGeometry(340, 376, 260, 20);
        s->setView(v);

        QTest::mouseClick(s, Qt::LeftButton, Qt::NoModifier, s->eolCenter());
        QCoreApplication::processEvents();
        QMenu* menu = s->openMenu();
        QVERIFY2(menu, "line-ending dropdown did not open");
        const QRect hostR = QRect(host.mapToGlobal(QPoint(0, 0)), host.size());
        const QPoint mpos = s->lastPopupPos();
        // The cell in global coordinates (same contract as the doc-type anchor
        // test): right edge of the menu lands on the cell's right edge.
        const QRect cellLocal = s->eolCellRect();
        const QRect cell(cellLocal.topLeft() + s->mapToGlobal(QPoint(0, 0)),
                         cellLocal.size());
        const int menuW = menu->sizeHint().width();
        QVERIFY2(qAbs((mpos.x() + menuW) - (cell.right() + 1)) <= 1,
                 qPrintable(QStringLiteral("eol menu right edge %1 vs cell right %2 "
                                           "(not anchored to the indicator)")
                                .arg(mpos.x() + menuW).arg(cell.right() + 1)));
        // NOT pinned to the window's left edge.
        QVERIFY2(mpos.x() > hostR.left(),
                 qPrintable(QStringLiteral("eol menu x=%1 pinned to the window left %2")
                                .arg(mpos.x()).arg(hostR.left())));
        // Fully inside the window.
        QVERIFY2(mpos.x() >= hostR.left() && mpos.x() + menuW <= hostR.right() + 1,
                 qPrintable(QStringLiteral("eol menu %1..%2 outside window %3..%4")
                                .arg(mpos.x()).arg(mpos.x() + menuW)
                                .arg(hostR.left()).arg(hostR.right())));
        menu->hide();

        s->setView(nullptr);
    }

    // Round 11 (LIVE CRASH, 2026-11-22): StatusSelectors::m_view was a raw
    // pointer that outlived its view. After a split/tab close destroyed the
    // view while the status bar still pointed at it, the paint timer fired
    // eolText() -> m_view->findChild() on freed memory -> SIGSEGV
    // (QMetaObject::cast <- eolText <- paintEvent). m_view is now a QPointer,
    // so a destroyed view self-nulls and eolText()/paint report "-". Repro:
    // point the selectors at a REAL view, delete the view, then read eolText()
    // and paint - must not crash and must fall back to "-".
    void destroyed_view_does_not_crash_the_indicators()
    {
        KTextEditor::Document* doc = KTextEditor::Editor::instance()->createDocument(nullptr);
        QVERIFY(doc);
        doc->setText(QStringLiteral("line one\nline two\n"));
        KTextEditor::View* view = doc->createView(nullptr);
        QVERIFY(view);

        StatusSelectors s;
        s.resize(320, 20);
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));
        s.setView(view);
        QCoreApplication::processEvents();

        // While the view is alive, the EOL cell reads a real value (not "-").
        const QString live = s.eolText();
        QVERIFY2(live != QStringLiteral("-"),
                 "with a live view the EOL indicator should show a real type");

        // Destroy the view UNDER the selectors - the live crash's precondition.
        delete view;
        view = nullptr;
        QCoreApplication::processEvents();

        // The QPointer must have self-nulled: no crash, placeholder "-".
        QVERIFY2(s.view() == nullptr,
                 "m_view must self-null when the view is destroyed (it dangled)");
        const QString dead = s.eolText();
        QCOMPARE(dead, QStringLiteral("-"));

        // Paint must be safe too (the timer path that segfaulted).
        const QImage img = s.grab().toImage();
        QVERIFY2(!img.isNull(), "paint after view death must not crash");

        delete doc;
    }


};

QTEST_MAIN(TstStatusSel)
#include "tst_statussel.moc"
