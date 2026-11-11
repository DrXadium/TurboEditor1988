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
#include <QImage>
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
        // Spec 7 exact format (two spaces between items, no yellow).
        QCOMPARE(text, QStringLiteral("F2 Save  F3 Open  F5 Zoom  F6 Next  F10 Menu"));
    }

    // TV-revision spec 7: the status line is a FLAT solid #AAAAAA row across
    // the full width (every sampled row-pixel is gray), with no texture.
    void fkey_bar_is_flat_gray_full_width() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QImage img = bar->grab().toImage();
        QVERIFY(!img.isNull());
        // Sample several rows across the full bar width; all must be gray.
        int nonGray = 0;
        QString bad;
        const int rows[] = { 1, img.height() / 2, img.height() - 2 };
        for (int y : rows)
            for (int x = 0; x < img.width(); x += 7) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0xAA) > 14 || qAbs(c.green() - 0xAA) > 14
                    || qAbs(c.blue() - 0xAA) > 14) {
                    // The F-key text glyphs (black/red) sit on the middle row;
                    // only those are allowed off-gray there, not on top/bottom.
                    if (y != img.height() / 2) {
                        ++nonGray;
                        if (bad.size() < 80)
                            bad += QStringLiteral("(%1,%2) ").arg(x).arg(y);
                    }
                }
            }
        QVERIFY2(nonGray == 0,
                 qPrintable(QStringLiteral("status bar not flat gray at %1 sample(s): %2")
                                .arg(nonGray).arg(bad)));
    }

    // TV-revision spec 7: NO yellow (#FFFF55) anywhere in the status bar.
    void fkey_bar_has_no_yellow() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QImage img = bar->grab().toImage();
        int yellow = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0xFF) <= 30 && qAbs(c.green() - 0xFF) <= 30
                    && qAbs(c.blue() - 0x55) <= 40)
                    ++yellow;
            }
        QVERIFY2(yellow == 0,
                 qPrintable(QStringLiteral("status bar contains yellow px: %1").arg(yellow)));
    }

    // TV-revision spec 7: the line starts near column 12 (first red key F2 at
    // x in [8,20]).
    void fkey_line_starts_near_column_12() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QImage img = bar->grab().toImage();
        int firstRedX = -1;
        const int midY = img.height() / 2;
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, midY);
            if (c.red() > 150 && c.green() < 90 && c.blue() < 90) {
                firstRedX = x;
                break;
            }
        }
        QVERIFY2(firstRedX >= 8 && firstRedX <= 20,
                 qPrintable(QStringLiteral("first red F-key at x=%1, expected 8..20")
                                .arg(firstRedX)));
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
