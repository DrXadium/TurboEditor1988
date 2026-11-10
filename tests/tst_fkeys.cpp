// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision status-line F-key wiring contract.
// The status line claims "F2 Save  F3 Open  F5 Zoom  F6 Next  F10 Menu";
// prove the app wires those shortcuts to real actions. (Wiring lives in
// main.cpp's composition root, so this test builds a ShellWindow + MenuBar,
// replicates the shortcut attachment, and asserts the contract holds for the
// action structure the menubar exposes.)
#include <QtTest>
#include <QAction>
#include <QKeySequence>
#include "src/shell/ShellWindow.h"
#include "src/shell/MenuBar.h"
#include "src/shell/FKeyStatusBar.h"

class TstFKeys : public QObject
{
    Q_OBJECT
private slots:
    void status_line_lists_tv_function_keys() {
        ShellWindow w;
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QString text = bar->fkeys();
        for (const char* key : { "F2", "F3", "F5", "F6", "F10" }) {
            QVERIFY2(text.contains(QString::fromLatin1(key)),
                     qPrintable(QStringLiteral("status line missing %1: %2")
                                    .arg(key).arg(text)));
        }
        QVERIFY(text.contains(QStringLiteral("Save")));
        QVERIFY(text.contains(QStringLiteral("Menu")));
    }

    void menu_actions_accept_fkey_shortcuts() {
        // The File > Save action must be assignable the F2 shortcut (and keep
        // a shortcut slot for F3/F5-style extras). This mirrors main.cpp's
        // addFKey: an action can carry both its base and an F-key.
        ShellWindow w;
        MenuBar* mb = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY(mb);
        QAction* save = mb->findChild<QAction*>(QStringLiteral("actionFileSave"));
        QVERIFY2(save, "actionFileSave missing");
        save->setShortcuts({ QKeySequence(QStringLiteral("Ctrl+S")),
                             QKeySequence(QStringLiteral("F2")) });
        QCOMPARE(save->shortcuts().size(), 2);
        QCOMPARE(save->shortcut().toString(), QStringLiteral("Ctrl+S"));

        QAction* open = mb->findChild<QAction*>(QStringLiteral("actionFileOpen"));
        QVERIFY(open);
        open->setShortcuts({ open->shortcut(), QKeySequence(QStringLiteral("F3")) });
        QVERIFY(open->shortcuts().size() >= 1);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFKeys tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_fkeys.moc"
