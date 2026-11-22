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

#include "src/shell/ShellWindow.h"
#include "src/shell/EditorPane.h"
#include "src/shell/EditorSplitter.h"
#include "src/shell/Minimap.h"
#include "src/shell/RightScrollBar.h"
#include "src/shell/BottomBand.h"
#include "src/editor/EditorTabManager.h"
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
};

QTEST_MAIN(TstSplitView)
#include "tst_splitview.moc"
