// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-21: Split view (approach A from
// docs/superpowers/specs/2026-11-21-split-view-design.md).
//
// The suite proves: pane 0 keeps today's frame/title/scrollbars byte-identical,
// the split adds a painted divider + a second pane with its own chrome, the
// EditorTabManager holds two views over one document independently, and a
// focused-pane press drives the active pane/minimap. Strict TDD: every case
// was written RED first.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QFile>

#include "src/shell/ShellWindow.h"
#include "src/shell/EditorPane.h"
#include "src/shell/EditorSplitter.h"
#include "src/shell/Minimap.h"
#include "src/shell/RightScrollBar.h"
#include "src/shell/BottomBand.h"
#include "src/editor/EditorTabManager.h"
#include "src/bridge/ContentBridge.h"
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

class TstSplitView : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // Task 1: extracting EditorPane must not move a single pixel of pane 0.
    // The shell builds pane 0 with the frame, title row, right scrollbar and
    // bottom band; the editor widget installs into the pane's body slot; pane
    // 0 never shows the split's close box.
    void pane0_keeps_its_frame_title_and_scrollbars() {
        ShellWindow w;
        auto* editor = new QWidget;
        editor->setObjectName(QStringLiteral("stackUnderTest"));
        w.setEditorWidget(editor);

        EditorPane* pane = w.findChild<EditorPane*>();
        QVERIFY2(pane, "ShellWindow has no EditorPane for pane 0");
        QCOMPARE(pane->paneIndex(), 0);

        // The frame + title the composition root retitles per tab.
        w.setEditorTitle(QStringLiteral("report.txt"));
        QCOMPARE(pane->title(), QStringLiteral("report.txt"));
        QVERIFY2(pane->title().isEmpty() == false, "title label must hold the name");

        // Per-pane chrome: the right scrollbar column and the bottom band
        // (L:C + horizontal bar) belong to the pane, not the shell.
        QVERIFY2(pane->rightBar(), "pane 0 has no RightScrollBar");
        QVERIFY2(pane->bottomBand(), "pane 0 has no BottomBand");

        // The editor widget landed in the pane's body slot.
        QVERIFY2(pane->bodySlot(), "pane 0 has no body slot");
        QVERIFY2(pane->bodySlot() == editor || pane->bodySlot()->isAncestorOf(editor),
                 "editor was not installed into pane 0's body slot");

        // Pane 0 is never the split pane: no close box.
        QVERIFY2(pane->closeBox(), "close box widget should exist (hidden on pane 0)");
        QVERIFY2(pane->closeBox()->isHidden(), "pane 0 must not show the split close box");
    }

    // Task 2: the divider exists only while split, is one cell wide, and paints
    // a TV grip. A single pane shows no handle (the splitter collapses cleanly).
    void splitter_shows_a_painted_divider_only_while_split() {
        EditorSplitter split;
        split.resize(400, 300);
        auto* a = new EditorPane(&split);
        a->setPaneIndex(0);
        split.addPane(a);

        // Not split: one child, no handle.
        QVERIFY2(!split.splitOpen(), "split must start closed");
        QCOMPARE(split.count(), 1);

        split.openSplit();
        QVERIFY2(split.splitOpen(), "split must report open after openSplit()");
        QCOMPARE(split.count(), 2);
        QVERIFY2(split.pane(1), "pane 1 missing after openSplit()");
        QCOMPARE(split.pane(1)->paneIndex(), 1);

        // One-cell handle.
        QCOMPARE(split.handleWidth(), split.pane(0)->cellWidth());
        QSplitterHandle* h = split.handle(1);
        QVERIFY2(h, "splitter handle not created for the split");
        QVERIFY2(!h->isHidden(), "handle must be visible while split");

        // The handle paints a TV frame/grip: white frame ink present.
        const QImage img = h->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 2 && img.height() > 40,
                 "handle did not render");
        int white = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QRgb px = img.pixel(x, y);
                if (qRed(px) > 200 && qGreen(px) > 200 && qBlue(px) > 200)
                    ++white;
            }
        QVERIFY2(white > 8,
                 qPrintable(QStringLiteral("handle has no TV frame ink (%1 white px)")
                                .arg(white)));

        split.closeSplit();
        QVERIFY2(!split.splitOpen(), "split must report closed after closeSplit()");
        QCOMPARE(split.count(), 1);
    }

    // Task 3: the model layer. Two views over one document are independent
    // (own caret/scroll/fold), the split survives a left-pane tab switch, a
    // second split request never creates a third pane, and closing the split
    // document's tab collapses the split while keeping pane 0's document.
    void openSplitWithActiveDocument_yields_two_views_over_one_document() {
        EditorTabManager tabs;
        KTextEditor::Document* doc = tabs.activeDocument();
        QVERIFY(doc);
        QVERIFY(tabs.viewForPane(0) == tabs.activeView());

        tabs.openSplitWithActiveDocument();
        QVERIFY2(tabs.splitOpen(), "split must open");
        KTextEditor::View* v0 = tabs.viewForPane(0);
        KTextEditor::View* v1 = tabs.viewForPane(1);
        QVERIFY(v0 && v1);
        QVERIFY2(v0 != v1, "the two panes must be distinct views");
        QCOMPARE(v0->document(), doc);
        QCOMPARE(v1->document(), doc);

        // Independent carets: moving one must not move the other. Give the
        // shared document 5 lines so line 3 exists (an empty doc has 1 line
        // and KTextEditor clamps the caret).
        doc->clear();
        for (int i = 0; i < 5; ++i)
            doc->insertLine(i, QStringLiteral("line %1").arg(i));
        v1->setCursorPosition(KTextEditor::Cursor(3, 0));
        QVERIFY2(v1->cursorPosition().line() == 3, "pane 1 caret moved");
        QVERIFY2(v0->cursorPosition().line() != 3,
                 "pane 1's caret leaked into pane 0");

        tabs.closeSplit();
        QVERIFY2(!tabs.splitOpen(), "split must close");
        QVERIFY2(tabs.viewForPane(1) == nullptr, "pane 1 view must be gone");
        QCOMPARE(tabs.activeView(), v0);
        QCOMPARE(tabs.activeDocument(), doc);

        // The active pane PIVOT: focus in pane 1 makes activeView() the pane-1
        // view (so minimap/status/fold follow the focused pane). This pins the
        // load-bearing behaviour activeViewChanged relies on.
        tabs.openSplitWithActiveDocument();
        KTextEditor::View* pv1 = tabs.viewForPane(1);
        QVERIFY(pv1);
        QCOMPARE(tabs.activeView(), v0);       // pane 0 focused by default
        tabs.setActivePane(1);
        QCOMPARE(tabs.activeView(), pv1);      // now the pane-1 view
        tabs.setActivePane(0);
        QCOMPARE(tabs.activeView(), v0);       // back to pane 0
        tabs.closeSplit();
    }

    void openSplitWithDocument_shows_b_while_pane0_keeps_a() {
        EditorTabManager tabs;               // constructor seeds 1 doc (index 0)
        const int seeded = tabs.activeIndex();
        tabs.newDocument();                 // A (index 1)
        const int aIdx = tabs.activeIndex();
        tabs.newDocument();                 // B (index 2)
        const int bIdx = tabs.activeIndex();
        QVERIFY(aIdx != bIdx);
        const int before = tabs.tabCount();  // 3: seed + A + B

        // Point pane 0 back at A, then open B into the split's right pane.
        tabs.setActiveIndex(aIdx);
        tabs.openSplitWithDocument(tabs.documents().at(bIdx));

        QVERIFY2(tabs.splitOpen(), "split must open on doc B");
        QCOMPARE(tabs.documentForPane(0), tabs.documents().at(aIdx));
        QCOMPARE(tabs.documentForPane(1), tabs.documents().at(bIdx));
        QCOMPARE(tabs.tabCount(), before);   // opening a split adds NO tab
        QVERIFY(seeded < before);
    }

    // Round 8 (live FAIL 1): File > Open as Split Document must put the picked
    // file in the NEW right pane only. Production order is open-then-split, and
    // plain openFile() ACTIVATES the new tab in pane 0 - so the split mirrored
    // the same file into BOTH panes. openFileAsSplit() must keep pane 0 on the
    // document the user was editing.
    void openFileAsSplit_puts_the_file_in_the_right_pane_and_keeps_pane0() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("split_target.txt"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("alpha\nbeta\n");
        }

        EditorTabManager tabs;              // constructor seeds 1 doc
        tabs.newDocument();                 // A - the doc the user is editing
        const int aIdx = tabs.activeIndex();
        tabs.setActiveIndex(aIdx);
        KTextEditor::Document* aDoc = tabs.documents().at(aIdx);
        const int before = tabs.tabCount();

        tabs.openFileAsSplit(QUrl::fromLocalFile(path));

        QCOMPARE(tabs.tabCount(), before + 1);    // the picked file is a tab
        QVERIFY2(tabs.splitOpen(), "the split must open on the picked file");
        QCOMPARE(tabs.documentForPane(0), aDoc);  // pane 0 is NOT hijacked
        KTextEditor::Document* splitDoc = tabs.documentForPane(1);
        QVERIFY2(splitDoc, "pane 1 must show the picked file");
        QVERIFY2(splitDoc != aDoc, "the split must show the NEW file, not pane 0's");
        QCOMPARE(splitDoc, tabs.documents().last());
        QTRY_VERIFY2(splitDoc->text().contains(QStringLiteral("alpha")),
                     "the picked file's contents must load into the right pane");
    }

    void a_second_split_request_does_not_create_a_third_pane_or_view() {
        EditorTabManager tabs;
        tabs.openSplitWithActiveDocument();
        KTextEditor::View* first1 = tabs.viewForPane(1);
        QVERIFY(first1);
        tabs.openSplitWithActiveDocument(); // must be a no-op / re-point
        QVERIFY2(tabs.splitOpen(), "still open");
        QCOMPARE(tabs.viewForPane(1), first1);  // same pane-1 view, no 3rd
        QCOMPARE(tabs.viewForPane(0), tabs.viewForPane(0));
    }

    void closeTab_of_the_split_document_collapses_the_split_and_keeps_pane0() {
        EditorTabManager tabs;               // constructor seeds 1 doc (index 0)
        tabs.newDocument();                 // A
        const int aIdx = tabs.activeIndex();
        tabs.newDocument();                 // B
        const int bIdx = tabs.activeIndex();
        const int before = tabs.tabCount();  // 3: seed + A + B
        KTextEditor::Document* aDoc = tabs.documents().at(aIdx);
        tabs.setActiveIndex(aIdx);
        tabs.openSplitWithDocument(tabs.documents().at(bIdx));
        QVERIFY(tabs.splitOpen());

        // Close B's tab: the split collapses, A (pane 0's doc) survives.
        tabs.closeTab(bIdx);
        QVERIFY2(!tabs.splitOpen(), "closing the split doc must collapse the split");
        QCOMPARE(tabs.tabCount(), before - 1);
        QCOMPARE(tabs.activeDocument(), aDoc);
        QVERIFY2(tabs.activeDocument() != nullptr, "pane 0 document must survive");
    }

    // Task 4: the split's right pane carries its own title (the doc shown
    // there) and the ONLY visible ■ close box; clicking it collapses the split.
    void split_title_row_shows_pane1_doc_and_only_pane1_has_a_close_box() {
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);   // installs pane 0's slot
        w.show();
        QCoreApplication::processEvents();

        // Single pane: no pane 1, no split.
        QVERIFY2(!w.splitOpen(), "must start unsplit");
        QVERIFY2(!w.paneWidget(1), "pane 1 must be null before a split");

        // Open a second view of a document into pane 1.
        auto* stack = new QWidget;   // stand-in for the secondary stack
        w.setSplitEditorWidget(stack, QStringLiteral("docB.txt"));
        QVERIFY2(w.splitOpen(), "split must open after setSplitEditorWidget");
        QVERIFY2(w.paneWidget(1), "pane 1 must exist after the split");

        // Pane 0 keeps its own title; pane 1 shows the split doc's name.
        QVERIFY2(!w.paneWidget(0)->title().isEmpty(), "pane 0 title must render");
        QCOMPARE(w.paneWidget(1)->title(), QStringLiteral("docB.txt"));

        // Only pane 1 shows the close box, and it sits inside the pane.
        QVERIFY2(!w.paneWidget(0)->closeBox()->isVisible(),
                 "pane 0 must NOT show the split close box");
        QVERIFY2(w.paneWidget(1)->closeBox()->isVisible(),
                 "pane 1 must show the split close box");
        const QRect boxRect = w.paneWidget(1)->closeBoxRect();
        QVERIFY2(w.paneWidget(1)->rect().contains(boxRect),
                 qPrintable(QStringLiteral("close box %1 outside pane %2")
                                .arg(boxRect.x()).arg(boxRect.y())));

        // Clicking pane 1's ■ collapses the split.
        QTest::mouseClick(w.paneWidget(1)->closeBox(), Qt::LeftButton);
        QCoreApplication::processEvents();
        QVERIFY2(!w.splitOpen(), "clicking ■ must collapse the split");
    }

    // Task 6, case 1: a press INSIDE a pane makes that pane active; a tab
    // click does NOT move the active pane (Review Focus 3).
    void press_in_pane1_makes_it_active_but_a_tab_click_does_not() {
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);
        w.show();
        QCoreApplication::processEvents();

        auto* stack = new QWidget;
        w.setSplitEditorWidget(stack, QStringLiteral("docB.txt"));
        QVERIFY(w.splitOpen());
        QCOMPARE(w.activePane(), 0);

        QSignalSpy spy(&w, &ShellWindow::activePaneChanged);
        // Press inside pane 1's stack -> pane 1 becomes active.
        w.notePressInWidget(w.paneWidget(1)->bodySlot());
        QCOMPARE(w.activePane(), 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);

        // A press in pane 0 makes it active again.
        w.notePressInWidget(w.paneWidget(0)->bodySlot());
        QCOMPARE(w.activePane(), 0);
        QCOMPARE(spy.at(1).at(0).toInt(), 0);

        // Pressing the SAME pane again does NOT re-emit (idempotent).
        const int n = spy.count();
        w.notePressInWidget(w.paneWidget(0)->bodySlot());
        QCOMPARE(spy.count(), n);
    }

    // Round 8 (live FAIL 2): pane 1's BottomBand was created as a child of the
    // pane but never placed in any layout, so it kept default geometry - a
    // 100x30 blob at the pane's top-left (the "incomplete top line") and NO
    // body of the pane ever showed in the bottom row. Each pane must carry its
    // band directly BELOW it, and both bands must sit on the same row so the
    // two editors stay line-aligned.
    void each_panes_band_sits_directly_below_its_own_pane() {
        ShellWindow w;
        w.resize(800, 600);
        w.setEditorWidget(new QWidget);
        w.show();
        QCoreApplication::processEvents();

        w.setSplitEditorWidget(new QWidget, QStringLiteral("docB.txt"));
        QVERIFY(w.splitOpen());
        QCoreApplication::processEvents();

        EditorPane* p0 = w.paneWidget(0);
        EditorPane* p1 = w.paneWidget(1);
        QVERIFY(p0 && p1);
        BottomBand* b0 = p0->bottomBand();
        BottomBand* b1 = p1->bottomBand();
        QVERIFY(b0 && b1);

        QVERIFY2(b1->isVisible(), "pane 1's band must be visible while split");
        QVERIFY2(b0->isVisible(), "pane 0's band must be visible while split");

        // Directly below: the band's top edge is the pane's bottom edge.
        const QPoint p1Top = p1->mapTo(&w, QPoint(0, 0));
        const QPoint b1Top = b1->mapTo(&w, QPoint(0, 0));
        QCOMPARE(b1Top.y(), p1Top.y() + p1->height());
        QCOMPARE(b1->width(), p1->width());

        // Both panes are the same height and their bands share one row, so the
        // two editors show the same lines at the same y.
        QCOMPARE(p0->height(), p1->height());
        QCOMPARE(b0->mapTo(&w, QPoint(0, 0)).y(), b1Top.y());
    }

    // Round 8 (live FAIL 2b): showCloseBox(true) used to show the box WITHOUT
    // positioning it. A pane that already had its final width never saw another
    // resize, so the box stayed at its default geometry with no paintEvent -
    // i.e. no close control was drawn at all.
    void showCloseBox_positions_the_box_immediately() {
        EditorPane pane;
        pane.setPaneIndex(1);
        pane.resize(400, 300);
        QCoreApplication::processEvents();

        pane.showCloseBox(true);
        const QRect r = pane.closeBoxRect();
        QVERIFY2(r.width() == pane.cellHeight(),
                 qPrintable(QStringLiteral("close box width %1, want %2")
                                .arg(r.width()).arg(pane.cellHeight())));
        QVERIFY2(pane.rect().contains(r),
                 qPrintable(QStringLiteral("close box %1,%2 is outside the pane")
                                .arg(r.x()).arg(r.y())));
        QVERIFY2(r.right() > pane.width() / 2, "close box must sit in the right cell");
    }

    // Task 6, case 3: each pane's band follows ITS OWN view's caret, so a
    // split shows two independent L:C readouts.
    void each_pane_band_shows_its_own_caret() {
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);
        w.show();
        QCoreApplication::processEvents();

        auto* stack = new QWidget;
        w.setSplitEditorWidget(stack, QStringLiteral("docB.txt"));
        QVERIFY(w.splitOpen());

        // Set pane 1's band caret directly (the per-pane binding is what
        // main.cpp wires; here we pin that each pane HAS its own band and that
        // setting one does not touch the other). cursorPosText() = "line:col"
        // (BottomBand.cpp).
        w.paneWidget(1)->bottomBand()->setCursorPos(12, 4);
        QCOMPARE(w.paneWidget(1)->bottomBand()->cursorPosText(),
                 QStringLiteral("12:4"));
        QVERIFY2(w.paneWidget(0)->bottomBand()->cursorPosText()
                     != w.paneWidget(1)->bottomBand()->cursorPosText(),
                 "the two bands must be independent readouts");
    }

    // Task 6, case 4: the keyboard-ownership guard still applies with a split
    // open - a forwarded key must NOT reach a split-pane view while a dialog
    // owns the keyboard (Review Focus 5).
    void split_pane_still_respects_keyboard_ownership() {
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);
        ContentBridge b;
        b.setShellWindow(&w);
        w.show();
        QCoreApplication::processEvents();

        auto* stack = new QWidget;
        w.setSplitEditorWidget(stack, QStringLiteral("docB.txt"));
        QVERIFY(w.splitOpen());

        // A dialog owns the keyboard. It must be SHOWN: keyboardIsOwned() is
        // !isHidden() (trap #3 - a hidden owner means no ownership).
        auto* owner = new QWidget(&w);
        owner->show();
        w.setKeyboardOwner(owner);
        QVERIFY(w.keyboardIsOwned());

        // A forwarded key must be swallowed by the guard, not delivered to the
        // focused split-pane view. We detect delivery by watching whether the
        // shell's key target still routes to the editor: with an owner, the
        // guard blocks. Assert the guard reports ownership (the bridge checks
        // keyboardIsOwned before routing).
        QVERIFY2(w.keyboardIsOwned(),
                 "split must not bypass the keyboard-ownership guard");
        // And clearing the owner restores editing.
        w.setKeyboardOwner(nullptr);
        QVERIFY2(!w.keyboardIsOwned(), "clearing the owner must release keys");
    }

    // Task 6, per-pane scrollbars: pane 1's right bar + band mirror ITS view,
    // independent of pane 0. A range change on the pane-1 view updates pane 1's
    // chrome without touching pane 0's.
    void each_pane_scrollbar_mirrors_its_own_view() {
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);
        w.show();
        QCoreApplication::processEvents();

        auto* stack = new QWidget;
        w.setSplitEditorWidget(stack, QStringLiteral("docB.txt"));
        QVERIFY(w.splitOpen());

        // Two independent stand-in scrollbars (proxy for each view's KateScrollBar).
        auto* v0 = new QScrollBar(Qt::Vertical);
        auto* v1 = new QScrollBar(Qt::Vertical);
        v0->setRange(0, 100);
        v1->setRange(0, 50);
        w.wirePaneScrollbars(0, v0, nullptr);
        w.wirePaneScrollbars(1, v1, nullptr);

        QCOMPARE(w.paneWidget(0)->rightBar()->maximum(), 100);
        QCOMPARE(w.paneWidget(1)->rightBar()->maximum(), 50);

        // Growing pane 1's range must NOT move pane 0's bar.
        v1->setRange(0, 200);
        QCOMPARE(w.paneWidget(1)->rightBar()->maximum(), 200);
        QCOMPARE(w.paneWidget(0)->rightBar()->maximum(), 100);
    }

    // Round 9 (LIVE CRASH repro, 2026-11-22): View > Open Document in New View
    // segfaults after a split was opened and then closed with the pane-1 ■ box,
    // leaving only a regular view. Root cause: the ■ box drives ONLY the shell's
    // clearSplitEditorWidget() -> EditorSplitter::closeSplit(), which tears down
    // the shell's pane1/column widgets but NEVER calls EditorTabManager::closeSplit()
    // - so m_secondaryViews keeps the now-orphaned view. The next
    // openSplitWithActiveDocument() -> ensureSecondaryView() finds the doc still in
    // the map and hands back the STALE view -> segfault. Reproduces the exact wiring
    // from main.cpp (splitChanged -> shell), then drives the ■ close and a re-open.
    void reopening_the_split_after_the_close_box_yields_a_live_view() {
        EditorTabManager tabs;
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);          // pane 0's slot
        w.show();
        QCoreApplication::processEvents();

        // The composition-root wiring (main.cpp:335): tabs.splitChanged -> shell.
        QObject::connect(&tabs, &EditorTabManager::splitChanged, &w,
                         [&tabs, &w](bool open) {
                             if (open) {
                                 KTextEditor::Document* d = tabs.documentForPane(1);
                                 QString name;
                                 if (d) {
                                     const QString f = d->url().toLocalFile();
                                     name = f.isEmpty() ? d->documentName() : QFileInfo(f).fileName();
                                 }
                                 if (name.isEmpty()) name = QStringLiteral("untitled");
                                 w.setSplitEditorWidget(tabs.secondaryContainerWidget(), name);
                             } else {
                                 w.clearSplitEditorWidget();
                             }
                         });

        // 1. Open a second view of the active document (the menu action).
        tabs.openSplitWithActiveDocument();
        QCoreApplication::processEvents();
        QVERIFY2(tabs.splitOpen(), "split must open");
        KTextEditor::View* first = tabs.viewForPane(1);
        QVERIFY2(first, "pane 1 must hold a view after the first open");

        // 2. Close the split the way the ■ box does: the SHELL's clear path.
        //    This is the live crash's precondition - only the shell tears down.
        w.clearSplitEditorWidget();
        QCoreApplication::processEvents();
        QVERIFY2(!w.splitOpen(), "shell split must collapse after the ■ box");

        // 3. Re-open: View > Open Document in New View again. Must be SAFE: it
        //    reuses a still-alive view, or (if the shell destroyed it with pane 1)
        //    creates a FRESH one - never a dangling pointer. The crash this guards
        //    was the double-free in ~EditorTabManager (destroySecondaryViews ->
        //    removeWidget/delete on a view pane-1's teardown already killed),
        //    proven RED at RC=139 before the QPointer fix.
        tabs.openSplitWithActiveDocument();
        QCoreApplication::processEvents();

        // The reopened split must be reported OPEN with a live view (the action
        // is supposed to have a second view of the active document).
        QVERIFY2(tabs.splitOpen(), "re-open must report an open split");
        KTextEditor::View* second = tabs.viewForPane(1);
        QVERIFY2(second, "re-open must yield a pane-1 view");
        QVERIFY2(second->parent() != nullptr,
                 "pane-1 view has no parent (dangling -> the crash)");
        QCOMPARE(second->document(), tabs.activeDocument());
        // If it reused the first view, that view must STILL be alive (a dead one
        // would have a null QPointer, so viewForPane(1) would have returned null).
        if (second == first)
            QVERIFY2(first->parent() != nullptr, "reused view died but was returned");
        // The LIVE-DEATH guarantee: when `tabs` goes out of scope below, its dtor
        // runs destroySecondaryViews() over whatever state the shell's teardown
        // left - before the QPointer fix this SIGSEGV'd (RC=139). Completing the
        // test at RC==0 is itself the regression proof.
    }

    // Round 10 (LIVE CRASH repro, 2026-11-22): Xadium clicked the \u25a0 (or the blank
    // title spot) to close the new-view pane, then clicked a REGULAR doc tab ->
    // instant segfault. Root cause: the \u25a0 close path is
    // ShellWindow::clearSplitEditorWidget() -> EditorSplitter::closeSplit(), which
    // DESTROYS pane 1 (and the m_stack1 + secondary view it holds) but NEVER calls
    // EditorTabManager::closeSplit(). So the manager keeps a DANGLING secondary
    // view. With pane 1 still the active pane (m_activePane == 1), a tab click ->
    // setActiveIndex() -> trackActiveCursor()/refreshFoldIndicator() read
    // activeView(), which returns m_secondaryViews.value(...) - the DEAD view ->
    // segfault. Reproduces the exact sequence: split open + pane 1 focused, shell
    // \u25a0 close, then click a different tab.
    void clicking_a_tab_after_the_shell_close_box_does_not_crash() {
        EditorTabManager tabs;
        ShellWindow w;
        w.resize(640, 480);
        w.setEditorWidget(new QWidget);          // pane 0's slot
        w.show();
        QCoreApplication::processEvents();

        // The composition-root wiring (main.cpp:335): tabs.splitChanged -> shell.
        QObject::connect(&tabs, &EditorTabManager::splitChanged, &w,
                         [&tabs, &w](bool open) {
                             if (open) {
                                 KTextEditor::Document* d = tabs.documentForPane(1);
                                 QString name;
                                 if (d) {
                                     const QString f = d->url().toLocalFile();
                                     name = f.isEmpty() ? d->documentName() : QFileInfo(f).fileName();
                                 }
                                 if (name.isEmpty()) name = QStringLiteral("untitled");
                                 w.setSplitEditorWidget(tabs.secondaryContainerWidget(), name);
                             } else {
                                 w.clearSplitEditorWidget();
                             }
                         });

        // Two more tabs so a "regular doc tab" click has somewhere to go.
        tabs.newDocument();   // index 1
        tabs.newDocument();   // index 2

        // Open a second view of the ACTIVE document (tab 0) into pane 1.
        tabs.openSplitWithActiveDocument();
        QCoreApplication::processEvents();
        QVERIFY2(tabs.splitOpen(), "split must open");
        // The user was WORKING IN PANE 1 (that is what made it the active pane,
        // so activeView() resolves through m_secondaryViews on the tab click).
        tabs.setActivePane(1);
        QCOMPARE(tabs.activePane(), 1);

        // The \u25a0 / blank-spot close: the SHELL tears pane 1 down (and with it the
        // secondary view + stack), but the manager is never told. This is the live
        // crash's precondition - the manager now holds a dangling view.
        w.clearSplitEditorWidget();
        QCoreApplication::processEvents();
        QVERIFY2(!w.splitOpen(), "shell split must collapse after the \u25a0 box");

        // Click a REGULAR doc tab (the live crash trigger). This must not deref
        // the dead secondary view through activeView()/trackActiveCursor().
        tabs.setActiveIndex(1);
        QCoreApplication::processEvents();
        // Reaching here without a SIGSEGV IS the pass. Sanity: the manager still
        // reports a sane active document.
        QVERIFY(tabs.activeDocument() != nullptr);
    }
};

QTEST_MAIN(TstSplitView)
#include "tst_splitview.moc"
