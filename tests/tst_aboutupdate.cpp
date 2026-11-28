// SPDX-License-Identifier: GPL-3.0-or-later
// Task 6 (auto-update): the About box surfaces an available update as a
// passive version line always, and an "Update now" plate only when running as
// an AppImage (nothing to swap otherwise).
#include <QtTest>
#include <QPushButton>
#include <QLabel>
#include "src/shell/AboutModal.h"
#include "src/AppIdentity.h"
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
QLabel* findErrorLabel(AboutModal& m)
{
    for (QLabel* l : m.findChildren<QLabel*>()) {
        if (l->objectName() == QLatin1String("tvAboutUpdateError"))
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

    // The About box must state which build this is; "is an update available?"
    // is unanswerable without it (user request 2026-11-28).
    void about_shows_the_app_version()
    {
        AboutModal m;
        QLabel* v = nullptr;
        for (QLabel* l : m.findChildren<QLabel*>()) {
            if (l->objectName() == QLatin1String("tvAboutVersion"))
                v = l;
        }
        QVERIFY2(v, "no version line in the About box");
        QVERIFY2(v->text().contains(AppIdentity::version()),
                 qPrintable(QStringLiteral("version line lacks the version: %1").arg(v->text())));
        // A bare semver, never the git-describe suffix.
        QVERIFY2(!v->text().contains(QLatin1Char('-')),
                 qPrintable(QStringLiteral("version line is not a bare semver: %1").arg(v->text())));
    }

    // Spec §136-138: disable while running; re-enable + show an error line on
    // failure. A double click must not start two downloads either.
    void clicking_update_disables_the_button()
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
        int fired = 0;
        m.setUpdateTrigger([&fired](const QString&, const QString&) { ++fired; });

        QPushButton* b = findUpdateButton(m);
        QVERIFY(b);
        QVERIFY(b->isEnabled());
        b->click();
        QCOMPARE(fired, 1);
        QVERIFY2(!b->isEnabled(), "button stayed enabled during the update");
        b->click();                       // second click must be a no-op
        QCOMPARE(fired, 1);
    }

    void update_error_reenables_and_shows_message()
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
        QPushButton* b = findUpdateButton(m);
        QVERIFY(b);
        b->click();
        QVERIFY(!b->isEnabled());

        m.setUpdateError(QStringLiteral("checksum mismatch"));
        QVERIFY2(b->isEnabled(), "button not re-enabled after a failure");
        QLabel* err = findErrorLabel(m);
        QVERIFY2(err, "no error line shown");
        QVERIFY2(err->text().contains(QLatin1String("checksum mismatch")),
                 qPrintable(err->text()));
    }
};

QTEST_MAIN(TstAboutupdate)
#include "tst_aboutupdate.moc"
