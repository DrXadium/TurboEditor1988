// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (auto-update): the app must know its own version so it can compare
// itself to a GitHub release tag. version() is baked at configure time from
// the git tag (or a fallback) and must always be a bare X.Y.Z.
#include <QtTest>
#include <QRegularExpression>
#include "src/AppIdentity.h"

class TstAppidentity : public QObject
{
    Q_OBJECT
private slots:
    void version_returns_a_bare_semver()
    {
        const QString v = AppIdentity::version();
        QVERIFY2(QRegularExpression(QStringLiteral("^\\d+\\.\\d+\\.\\d+$")).match(v).hasMatch(),
                 qPrintable(QStringLiteral("version() is not a bare semver: '%1'").arg(v)));
        QVERIFY2(!v.startsWith(QLatin1Char('v')),
                 qPrintable(QStringLiteral("version() kept a leading v: '%1'").arg(v)));
    }
};

QTEST_APPLESS_MAIN(TstAppidentity)
#include "tst_appidentity.moc"
