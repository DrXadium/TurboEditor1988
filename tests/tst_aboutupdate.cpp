// SPDX-License-Identifier: GPL-3.0-or-later
// Task 6 (auto-update): the About box surfaces an available update as a
// passive version line always, and an "Update now" plate only when running as
// an AppImage (nothing to swap otherwise).
#include <QtTest>
#include <QPushButton>
#include <QLabel>
#include "src/shell/AboutModal.h"
#include "src/update/UpdateRunner.h"

namespace {
QPushButton* findUpdateButton(AboutModal& m)
{
    for (QPushButton* b : m.findChildren<QPushButton*>()) {
        if (b->objectName() == QLatin1String("tvModalAction")
            && b->text().remove(QLatin1Char('&'))
                   .compare(QLatin1String("Update now"), Qt::CaseInsensitive) == 0)
            return b;
    }
    return nullptr;
}
QLabel* findUpdateLine(AboutModal& m)
{
    for (QLabel* l : m.findChildren<QLabel*>()) {
        if (l->objectName() == QLatin1String("tvAboutUpdateLine"))
            return l;
    }
    return nullptr;
}
} // namespace

class TstAboutupdate : public QObject
{
    Q_OBJECT
private slots:
    void about_without_update_has_no_update_button()
    {
        AboutModal m;
        QVERIFY(!findUpdateButton(m));
        QVERIFY(!findUpdateLine(m));
    }

    void about_with_update_shows_passive_line_only_in_non_appimage()
    {
        // Not an AppImage (no APPIMAGE env): passive line, NO swap button.
        QVERIFY(!UpdateRunner::isAppImageRun());
        AboutModal m;
        m.setAvailableUpdate(QStringLiteral("9.9.9"), QUrl(QStringLiteral("http://x/a")),
                             QUrl(QStringLiteral("http://x/s")));
        QLabel* line = findUpdateLine(m);
        QVERIFY2(line, "no update line shown");
        QVERIFY2(line->text().contains(QLatin1String("9.9.9")),
                 qPrintable(QStringLiteral("line lacks version: %1").arg(line->text())));
        QVERIFY2(!findUpdateButton(m),
                 "Update now button shown outside an AppImage run");
    }

    void about_with_update_shows_button_in_appimage()
    {
        const QByteArray saved = qgetenv("APPIMAGE");
        qputenv("APPIMAGE", QByteArrayLiteral("/tmp/fake.AppImage"));
        auto restore = qScopeGuard([saved]() {
            if (saved.isEmpty())
                qunsetenv("APPIMAGE");
            else
                qputenv("APPIMAGE", saved);
        });

        AboutModal m;
        m.setAvailableUpdate(QStringLiteral("9.9.9"), QUrl(QStringLiteral("http://x/a")),
                             QUrl(QStringLiteral("http://x/s")));
        QVERIFY(findUpdateLine(m));
        QPushButton* b = findUpdateButton(m);
        QVERIFY2(b, "Update now button missing in an AppImage run");
        QVERIFY(b->isEnabled());
    }
};

QTEST_MAIN(TstAboutupdate)
#include "tst_aboutupdate.moc"
