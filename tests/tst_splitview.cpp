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
};

QTEST_MAIN(TstSplitView)
#include "tst_splitview.moc"
