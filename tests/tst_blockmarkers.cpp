#include <QtTest/QtTest>
#include <QAction>
#include <KTextEditor/Document>
#include <KTextEditor/View>

#include "src/editor/EditorTabManager.h"
#include "src/shell/MenuBar.h"
#include "src/shell/ShellWindow.h"

// Block markers (2026-11-20): one View toggle for all block decoration, hidden
// by default. The lever is Kate's OWN per-view action `view_folding_markers`,
// not View::setConfigValue: measured, a config write combined with a fold
// segfaults the app, while triggering this action does not.
class TstBlockMarkers : public QObject
{
    Q_OBJECT
private slots:
    void the_block_markers_item_is_present_and_off_by_default()
    {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);

        QAction* a = mb->findChild<QAction*>(QStringLiteral("actionViewFoldBar"));
        QVERIFY2(a, "block markers toggle missing");
        QVERIFY2(a->isCheckable(), "block markers must be a checkable toggle");
        QVERIFY2(!a->isChecked(), "block markers must be HIDDEN by default");
        QCOMPARE(a->text().remove(QLatin1Char('&')), QStringLiteral("Show Block Markers"));
    }

    void the_toggle_drives_kates_folding_marker_action()
    {
        EditorTabManager tabs;
        QVERIFY(tabs.activeView());
        QAction* markers = tabs.viewActionByName(QStringLiteral("view_folding_markers"));
        QVERIFY2(markers, "Kate's view_folding_markers action is missing");
        QVERIFY(markers->isCheckable());

        tabs.setBlockMarkersVisible(true);
        QVERIFY2(markers->isChecked(), "markers were not turned on");

        tabs.setBlockMarkersVisible(false);
        QVERIFY2(!markers->isChecked(), "markers were not turned off");
    }

    void a_fresh_view_starts_with_markers_hidden()
    {
        EditorTabManager tabs;
        QAction* markers = tabs.viewActionByName(QStringLiteral("view_folding_markers"));
        QVERIFY(markers);
        QVERIFY2(!markers->isChecked(),
                 "a new view must start with block markers hidden");
        QVERIFY(!tabs.blockMarkersVisible());
    }

    // The crash the design doc records: a config write combined with a fold
    // killed the app twice. Toggling after a fold must survive.
    void toggling_after_a_fold_does_not_crash()
    {
        EditorTabManager tabs;
        KTextEditor::View* v = tabs.activeView();
        QVERIFY(v && v->document());
        v->document()->setText(QStringLiteral("<header class=\"x\">\n  <p>a</p>\n</header>\n"));
        v->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY2(tabs.canActOnBlock(), "no block on the opening line");
        tabs.collapseBlock();

        tabs.setBlockMarkersVisible(true);
        tabs.setBlockMarkersVisible(false);
        QVERIFY(!tabs.blockMarkersVisible());
    }

    // Hiding the markers must not disable folding itself: the right-click block
    // commands still have to work.
    void markers_hidden_still_folds()
    {
        EditorTabManager tabs;
        KTextEditor::View* v = tabs.activeView();
        QVERIFY(v && v->document());
        v->document()->setText(QStringLiteral("<main>\n  <p>a</p>\n</main>\n"));
        tabs.setBlockMarkersVisible(false);

        // CONTROL: folding must work with the markers VISIBLE too. If this
        // fails, the cause is the test environment (an unshown view cannot
        // fold), not the marker toggle.
        tabs.setBlockMarkersVisible(true);
        v->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY2(tabs.canActOnBlock(), "no block on the opening line");
        QVERIFY(!tabs.currentBlockIsFolded());
        tabs.collapseBlock();
        const bool foldsWithMarkersOn = tabs.currentBlockIsFolded();
        tabs.expandBlock();

        tabs.setBlockMarkersVisible(false);
        v->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY2(tabs.canActOnBlock(), "no block on the opening line");
        QVERIFY(!tabs.currentBlockIsFolded());
        tabs.collapseBlock();
        QVERIFY2(tabs.currentBlockIsFolded() == foldsWithMarkersOn,
                 "hiding the markers changed whether folding works");
        tabs.expandBlock();
        QVERIFY2(!tabs.currentBlockIsFolded(), "expand did nothing with markers hidden");
    }
};

QTEST_MAIN(TstBlockMarkers)
#include "tst_blockmarkers.moc"
