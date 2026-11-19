// SPDX-License-Identifier: GPL-3.0-or-later
// Task 1 (2026-11-18 Find and Replace): the matching engine.
//
// Every semantic the dialog depends on is proved HERE, on plain strings, before
// a single widget exists: what a hit is, how case/whole-word/regex change it,
// how a replacement expands, and - the two that bite - that a pattern which can
// match nothing terminates, and that replace-all never rescans its own output.
#include <QtTest>
#include <QRegularExpression>
#include "src/editor/FindReplace.h"

using namespace FindReplace;

class TstFind : public QObject
{
    Q_OBJECT

private slots:
    void plain_finds_every_occurrence()
    {
        const auto hits = findAll(QStringLiteral("abcabc"), QStringLiteral("abc"), Options{});
        QCOMPARE(hits.size(), 2);
        QCOMPARE(hits[0].offset, 0);
        QCOMPARE(hits[0].length, 3);
        QCOMPARE(hits[1].offset, 3);
        QCOMPARE(hits[1].length, 3);
    }

    void case_sensitivity_follows_the_option()
    {
        QCOMPARE(findAll(QStringLiteral("Abc"), QStringLiteral("abc"), Options{}).size(), 1);

        Options sensitive;
        sensitive.caseSensitive = true;
        QCOMPARE(findAll(QStringLiteral("Abc"), QStringLiteral("abc"), sensitive).size(), 0);
        QCOMPARE(findAll(QStringLiteral("abc"), QStringLiteral("abc"), sensitive).size(), 1);
    }

    void whole_words_skips_substrings()
    {
        Options o;
        o.wholeWords = true;
        const auto hits = findAll(QStringLiteral("cat cats"), QStringLiteral("cat"), o);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits[0].offset, 0);

        // Punctuation is a word boundary too.
        const auto more = findAll(QStringLiteral("a cat, cat."), QStringLiteral("cat"), o);
        QCOMPARE(more.size(), 2);
    }

    void regex_uses_capture_groups()
    {
        Options o;
        o.regex = true;
        const auto hits = findAll(QStringLiteral("a1b22"), QStringLiteral("(\\d+)"), o);
        QCOMPARE(hits.size(), 2);
        QCOMPARE(hits[0].offset, 1);
        QCOMPARE(hits[0].length, 1);
        QCOMPARE(hits[1].offset, 3);
        QCOMPARE(hits[1].length, 2);
    }

    void an_invalid_pattern_yields_no_hits()
    {
        Options o;
        o.regex = true;
        QVERIFY2(findAll(QStringLiteral("abc"), QStringLiteral("["), o).isEmpty(),
                 "an unparsable pattern must report nothing, not crash");
        // The dialog has to tell "no hits" from "you typed nonsense".
        QVERIFY2(!isValid(QStringLiteral("["), o), "an unparsable pattern reported valid");
        QVERIFY2(isValid(QStringLiteral("a.c"), o), "a good regex reported invalid");
        QVERIFY2(!isValid(QString(), o), "an empty pattern reported valid");
        QVERIFY2(isValid(QStringLiteral("["), Options{}),
                 "a plain-text pattern was parsed as a regex");
        // ...and replace-all must hand the text back untouched.
        int count = -1;
        QCOMPARE(applyAll(QStringLiteral("abc"), QStringLiteral("["), QStringLiteral("x"), o, &count),
                 QStringLiteral("abc"));
        QCOMPARE(count, 0);
    }

    void a_zero_length_match_does_not_loop()
    {
        Options o;
        o.regex = true;

        // "^" matches nothing but the empty string at offset 0. The engine
        // reports it (it is a real match) and must terminate.
        const auto hits = findAll(QStringLiteral("abc"), QStringLiteral("^"), o);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits[0].length, 0);

        // ...and inserting there is meaningful.
        int count = 0;
        QCOMPARE(applyAll(QStringLiteral("abc"), QStringLiteral("^"), QStringLiteral(">"), o, &count),
                 QStringLiteral(">abc"));
        QCOMPARE(count, 1);

        // "a*" can match empty at every position: still terminates, and never
        // runs away.
        int many = 0;
        const QString out = applyAll(QStringLiteral("baaac"), QStringLiteral("a*"),
                                     QStringLiteral("X"), o, &many);
        QVERIFY2(out.size() < 20, qPrintable(QStringLiteral("runaway output: %1").arg(out)));
        QVERIFY(many >= 1);
    }

    void replacement_expands_capture_groups()
    {
        Options o;
        o.regex = true;
        int count = 0;
        const QString out = applyAll(QStringLiteral("a1 b22"), QStringLiteral("([a-z])(\\d+)"),
                                     QStringLiteral("<\\2\\1>"), o, &count);
        QCOMPARE(out, QStringLiteral("<1a> <22b>"));
        QCOMPARE(count, 2);
    }

    void a_replacement_is_literal_when_regex_is_off()
    {
        Options o;   // regex off
        int count = 0;
        // "\1" is just three characters here: a plain replace must not expand it.
        const QString out = applyAll(QStringLiteral("a"), QStringLiteral("a"),
                                     QStringLiteral("\\1"), o, &count);
        QCOMPARE(out, QStringLiteral("\\1"));
        QCOMPARE(count, 1);

        // Plain mode inserts exactly what was typed - a backslash-n stays two
        // characters. Escape sequences are a regex-mode convenience.
        int plainN = 0;
        QCOMPARE(applyAll(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("\\n"), o, &plainN),
                 QStringLiteral("\\n"));
        QCOMPARE(plainN, 1);

        // ...and with regex on, \n IS the newline the user meant.
        Options r;
        r.regex = true;
        int regexN = 0;
        QCOMPARE(applyAll(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("\\n"), r, &regexN),
                 QStringLiteral("\n"));
        QCOMPARE(regexN, 1);
    }

    void apply_all_reports_the_count()
    {
        int count = 0;
        QCOMPARE(applyAll(QStringLiteral("aaa"), QStringLiteral("a"), QStringLiteral("b"), Options{},
                          &count),
                 QStringLiteral("bbb"));
        QCOMPARE(count, 3);
    }

    void apply_all_never_rescans_its_own_output()
    {
        int count = 0;
        QCOMPARE(applyAll(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("aa"), Options{},
                          &count),
                 QStringLiteral("aa"));
        QCOMPARE(count, 1);
    }

    void apply_all_leaves_the_text_identical_when_nothing_matches()
    {
        int count = 7;
        QCOMPARE(applyAll(QStringLiteral("xyz"), QStringLiteral("q"), QStringLiteral("Q"), Options{},
                          &count),
                 QStringLiteral("xyz"));
        QCOMPARE(count, 0);
    }

    void cursor_at_maps_an_offset_to_line_and_column()
    {
        const QString text = QStringLiteral("ab\ncd");
        QCOMPARE(cursorAt(text, 0).line(), 0);
        QCOMPARE(cursorAt(text, 0).column(), 0);
        QCOMPARE(cursorAt(text, 1).line(), 0);
        QCOMPARE(cursorAt(text, 1).column(), 1);
        QCOMPARE(cursorAt(text, 3).line(), 1);
        QCOMPARE(cursorAt(text, 3).column(), 0);
        QCOMPARE(cursorAt(text, 4).line(), 1);
        QCOMPARE(cursorAt(text, 4).column(), 1);
    }

    void next_hit_wraps_around()
    {
        const QList<Hit> hits{{2, 1}, {7, 1}};
        QCOMPARE(nextHitIndex(hits, 0), 0);   // the first at or after the cursor
        QCOMPARE(nextHitIndex(hits, 2), 0);   // exactly on a hit
        QCOMPARE(nextHitIndex(hits, 5), 1);
        QCOMPARE(nextHitIndex(hits, 9), 0);   // past the last -> wrap to the top
        QCOMPARE(nextHitIndex({}, 0), -1);
    }
};

QTEST_APPLESS_MAIN(TstFind)
#include "tst_find.moc"
