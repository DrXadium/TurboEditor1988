// SPDX-License-Identifier: GPL-3.0-or-later
// Task 2 (auto-update): numeric comparison of release versions, tolerant of a
// leading "v" and a git-describe suffix. Equal or unparseable must NOT be
// "newer".
#include <QtTest>
#include "src/update/VersionCompare.h"

class TstVersioncompare : public QObject
{
    Q_OBJECT
private slots:
    void newer_release_is_newer()
    {
        QVERIFY(VersionCompare::isNewer(QStringLiteral("0.1.4"), QStringLiteral("0.1.3")));
    }
    void older_release_is_not_newer()
    {
        QVERIFY(!VersionCompare::isNewer(QStringLiteral("0.1.3"), QStringLiteral("0.1.4")));
    }
    void equal_release_is_not_newer()
    {
        QVERIFY(!VersionCompare::isNewer(QStringLiteral("0.1.4"), QStringLiteral("0.1.4")));
    }
    void segments_compare_numerically_not_lexically()
    {
        // "0.1.10" > "0.1.9" would be false if compared as strings.
        QVERIFY(VersionCompare::isNewer(QStringLiteral("v0.1.10"), QStringLiteral("0.1.9")));
    }
    void git_describe_suffix_is_ignored()
    {
        QVERIFY(!VersionCompare::isNewer(QStringLiteral("v0.1.4-2-gabc123"), QStringLiteral("0.1.4")));
    }
    void unparseable_is_not_newer()
    {
        QVERIFY(!VersionCompare::isNewer(QStringLiteral("garbage"), QStringLiteral("0.1.0")));
    }
};

QTEST_APPLESS_MAIN(TstVersioncompare)
#include "tst_versioncompare.moc"
