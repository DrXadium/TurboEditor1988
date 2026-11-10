// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3: Borland chrome shell contract test.
// Asserts ShellWindow exists, has MenuBar / TabStrip / FKeyStatusBar chrome,
// uses BorlandStyle, and the window title is "CRT-Kate".
#include <QtTest>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include "src/shell/ShellWindow.h"
#include "src/shell/BorlandStyle.h"
#include "src/shell/TabStrip.h"
#include "src/shell/FKeyStatusBar.h"

class TstChrome : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (!QApplication::instance())
            qWarning("expected QApplication (main provides it)");
    }

    void windowTitle_is_crt_kate() {
        ShellWindow w;
        QCOMPARE(w.windowTitle(), QStringLiteral("CRT-Kate"));
    }

    void has_chrome_children() {
        ShellWindow w;
        // MenuBar (top), TabStrip, FKeyStatusBar (bottom) present + central placeholder.
        QVERIFY2(w.menuBar(), "no menuBar");
        QVERIFY2(w.findChild<TabStrip*>(), "no TabStrip");
        QVERIFY2(w.findChild<FKeyStatusBar*>(), "no FKeyStatusBar");
        QVERIFY2(w.centralWidget(), "no central editor placeholder");
    }

    void uses_borland_style() {
        ShellWindow w;
        // QSS wraps the installed style in QStyleSheetStyle (which hides the
        // base), so assert on the BorlandStyle ShellWindow installed.
        QVERIFY2(w.borlandStyle(), "ShellWindow did not install a BorlandStyle");
        QCOMPARE(QString(w.borlandStyle()->metaObject()->className()),
                 QStringLiteral("BorlandStyle"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstChrome tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_chrome.moc"
