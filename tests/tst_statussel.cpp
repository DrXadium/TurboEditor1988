// SPDX-License-Identifier: GPL-3.0-or-later
// Status-line selectors (F-row indicators + dropdowns).
// Task 1: the Sonnet dictionary list must be reachable (links KF6::SonnetCore).
#include <QtTest>
#include <sonnet/speller.h>

class TstStatusSel : public QObject
{
    Q_OBJECT
private slots:
    void spell_list_is_nonempty_when_dicts_exist()
    {
        Sonnet::Speller speller;
        const QMap<QString, QString> dicts = speller.availableDictionaries();
        QVERIFY2(!dicts.isEmpty(),
                 "no spell dictionaries found (hunspell dicts should be in /usr/share/hunspell)");
    }
};

QTEST_MAIN(TstStatusSel)
#include "tst_statussel.moc"
