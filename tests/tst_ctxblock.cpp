#include <QtTest/QtTest>

#include "src/editor/EditorTabManager.h"
#include "src/shell/BorlandMenu.h"
#include "src/shell/ShellWindow.h"

// The editor right-click menu is built by ShellWindow, NOT by Kate: the shell
// is never mapped and ContentBridge synthesizes the press, so
// KTextEditor::View::contextMenuAboutToShow is never emitted. These pin the
// block commands onto the menu that actually appears.
class TstCtxBlock : public QObject
{
    Q_OBJECT
private slots:
    void the_block_commands_are_on_the_right_click_menu()
    {
        EditorTabManager tabs;
        ShellWindow shell;
        shell.setBlockMenuActions(tabs.contextBlockAction(QStringLiteral("actionCtxCollapseBlock")),
                                  tabs.contextBlockAction(QStringLiteral("actionCtxExpandBlock")),
                                  tabs.contextBlockAction(QStringLiteral("actionCtxHighlightBlock")),
                                  [&tabs] { tabs.refreshBlockMenuState(); });
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60));
        QCoreApplication::processEvents();

        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu, "editor context menu missing");

        QStringList have;
        for (QAction* a : menu->actions()) {
            if (a->isSeparator())
                continue;
            have << a->objectName();
        }
        const QStringList wanted{
            QStringLiteral("actionCtxCollapseBlock"), QStringLiteral("actionCtxExpandBlock"),
            QStringLiteral("actionCtxHighlightBlock")};
        for (const QString& name : wanted)
            QVERIFY2(have.contains(name),
                     qPrintable(QStringLiteral("right-click menu missing %1 (has: %2)")
                                    .arg(name, have.join(QLatin1Char(',')))));
        QVERIFY2(menu->isVisible(), "editor context menu did not open");
    }

    void the_block_commands_are_one_group_after_paste()
    {
        EditorTabManager tabs;
        ShellWindow shell;
        shell.setBlockMenuActions(tabs.contextBlockAction(QStringLiteral("actionCtxCollapseBlock")),
                                  tabs.contextBlockAction(QStringLiteral("actionCtxExpandBlock")),
                                  tabs.contextBlockAction(QStringLiteral("actionCtxHighlightBlock")),
                                  [&tabs] { tabs.refreshBlockMenuState(); });
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60));
        QCoreApplication::processEvents();

        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY(menu);

        QStringList shape;
        int separators = 0;
        for (QAction* a : menu->actions()) {
            if (a->isSeparator()) {
                ++separators;
                shape << QStringLiteral("---");
            } else {
                shape << a->objectName();
            }
        }
        // Undo/Redo --- Cut/Copy/Paste --- Collapse/Expand/Highlight Block
        QCOMPARE(shape.join(QLatin1Char('|')),
                 QStringLiteral("actionEditUndo|actionEditRedo|---|actionEditCut|"
                                "actionEditCopy|actionEditPaste|---|actionCtxCollapseBlock|"
                                "actionCtxExpandBlock|actionCtxHighlightBlock"));
        QVERIFY2(separators == 2, qPrintable(shape.join(QLatin1Char('|'))));
    }
};

QTEST_MAIN(TstCtxBlock)
#include "tst_ctxblock.moc"
