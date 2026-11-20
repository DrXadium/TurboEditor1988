// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: ReloadDialog - the house modal behind the external-change prompt.
// Two modes share one frame: Modified (Reload / Ignore) and Deleted (Keep /
// Close Tab, because a deleted file may still be worth saving back to disk).
//
// TDD: written before ReloadDialog exists.
#include <QtTest>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include "src/editor/ReloadDialog.h"

namespace {
QStringList buttonTexts(ReloadDialog* d)
{
    QStringList out;
    const auto buttons = d->findChildren<QPushButton*>();
    for (QPushButton* b : buttons)
        out << b->text().remove(QLatin1Char('&'));
    return out;
}
}

class TstReloadDlg : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void modified_mode_names_the_file_and_offers_reload_or_ignore() {
        ReloadDialog d(ReloadDialog::Mode::Modified, QStringLiteral("/tmp/report.txt"));
        QCOMPARE(d.title(), QStringLiteral("File Changed on Disk"));
        QCOMPARE(d.mode(), ReloadDialog::Mode::Modified);
        QCOMPARE(buttonTexts(&d), QStringList({QStringLiteral("Ignore"),
                                               QStringLiteral("Reload")}));
        QVERIFY2(d.fileNameLabel(), "file name label missing");
        QVERIFY2(d.fileNameLabel()->text().contains(QStringLiteral("report.txt")),
                 qPrintable(d.fileNameLabel()->text()));
        QVERIFY2(d.hintLabel()->text().contains(QStringLiteral("discard"), Qt::CaseInsensitive),
                 qPrintable(d.hintLabel()->text()));
    }

    void deleted_mode_offers_keep_or_close_and_says_saving_recreates() {
        ReloadDialog d(ReloadDialog::Mode::Deleted, QStringLiteral("/tmp/report.txt"));
        QCOMPARE(d.title(), QStringLiteral("File Deleted on Disk"));
        QCOMPARE(d.mode(), ReloadDialog::Mode::Deleted);
        QCOMPARE(buttonTexts(&d), QStringList({QStringLiteral("Close Tab"),
                                               QStringLiteral("Keep")}));
        QVERIFY2(d.hintLabel()->text().contains(QStringLiteral("Save"), Qt::CaseSensitive),
                 qPrintable(d.hintLabel()->text()));
    }

    void a_long_path_is_elided_but_keeps_the_file_name() {
        const QString longPath = QStringLiteral("/tmp/")
                                 + QString(300, QLatin1Char('a'))
                                 + QStringLiteral("/important-file-name.txt");
        ReloadDialog d(ReloadDialog::Mode::Modified, longPath);
        const QString shown = d.fileNameLabel()->text();
        QVERIFY2(shown.size() < longPath.size(), qPrintable(shown));
        QVERIFY2(shown.contains(QStringLiteral("important-file-name.txt")), qPrintable(shown));
    }

    // The TvModal exec trap: heap dialog, reject unconditionally, assert AFTER
    // exec returns. A stack dialog reparented by exec double-frees.
    void exec_returns_reject_when_cancelled() {
        QWidget anchor;
        anchor.resize(800, 600);
        anchor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&anchor));

        auto* d = new ReloadDialog(ReloadDialog::Mode::Modified, QStringLiteral("/tmp/x.txt"));
        QTimer::singleShot(0, &anchor, [d] { d->reject(); });
        const int role = TvModal::exec(&anchor, d);
        QCOMPARE(role, int(TvModal::Reject));
    }
};

QTEST_MAIN(TstReloadDlg)
#include "tst_reloaddlg.moc"
