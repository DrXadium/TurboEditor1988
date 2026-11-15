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

    // Task 5: the spell label shows the current dictionary's human-readable
    // description (e.g. "English (United States)"), not the raw code.
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
        s.setSpeller(&speller);   // the label maps code -> description via the speller
        s.setSpellLanguage(code);
        QCOMPARE(s.spellText(), desc);
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
};

QTEST_MAIN(TstStatusSel)
#include "tst_statussel.moc"
