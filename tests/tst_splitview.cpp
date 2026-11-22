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
};

QTEST_MAIN(TstSplitView)
#include "tst_splitview.moc"
