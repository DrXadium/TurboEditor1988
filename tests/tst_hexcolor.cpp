// SPDX-License-Identifier: GPL-3.0-or-later
// Hex colour token scanner (2026-11-23).
//
// The editor draws a swatch after every HTML colour code. Deciding WHERE the
// codes are is pure text logic, so it lives in its own unit and is tested
// without a view: a wrong answer here puts a swatch in the middle of a word or
// leaves one off the end of a line, and no paint-level test would say why.
//
// Proves:
//  1. a six-digit token after a `#` mid-line is found with its exact range;
//  2. a three-digit token is found and expanded to its six-digit colour;
//  3. a token at column 0 belongs to its own line, not to a previous range;
//  4. seven hex digits is NOT a token (`#abcdef0` must not yield `#abcdef`);
//  5. junk (`#ggg`, `#12345`, a bare `#`) yields nothing;
//  6. two tokens on one line are both found;
//  7. parseHexColor accepts the short and long forms with or without `#`.
#include <QtTest>
#include <QColor>
#include "src/shell/HexColorScan.h"

class TstHexColor : public QObject
{
    Q_OBJECT
private slots:
    void finds_a_six_digit_token_mid_line()
    {
        const auto tokens = scanHexColors(QStringLiteral("a { color: #ff8800; }"));
        QCOMPARE(tokens.size(), 1);
        QCOMPARE(tokens.at(0).start, 11);   // 'a'=0, '{'=2, 'color:'=4..9, '#'=11
        QCOMPARE(tokens.at(0).length, 7);
        QCOMPARE(tokens.at(0).color, QColor(0xff, 0x88, 0x00));
    }

    void finds_a_three_digit_token()
    {
        const auto tokens = scanHexColors(QStringLiteral("color:#abc"));
        QCOMPARE(tokens.size(), 1);
        QCOMPARE(tokens.at(0).start, 6);
        QCOMPARE(tokens.at(0).length, 4);
        QCOMPARE(tokens.at(0).color, QColor(0xaa, 0xbb, 0xcc));
    }

    void token_at_line_start_is_found()
    {
        const auto tokens = scanHexColors(QStringLiteral("#00ff00 rest"));
        QCOMPARE(tokens.size(), 1);
        QCOMPARE(tokens.at(0).start, 0);
        QCOMPARE(tokens.at(0).length, 7);
        QCOMPARE(tokens.at(0).color, QColor(0x00, 0xff, 0x00));
    }

    void seven_hex_digits_is_not_a_token()
    {
        QVERIFY2(scanHexColors(QStringLiteral("#abcdef0")).isEmpty(),
                 "a seven-digit run must not be swatched as six digits");
    }

    void rejects_non_hex()
    {
        QVERIFY(scanHexColors(QStringLiteral("#ggg")).isEmpty());
        QVERIFY(scanHexColors(QStringLiteral("#12345")).isEmpty());
        QVERIFY(scanHexColors(QStringLiteral("#")).isEmpty());
        QVERIFY(scanHexColors(QStringLiteral("no colours here")).isEmpty());
    }

    void finds_two_tokens_on_one_line()
    {
        const auto tokens = scanHexColors(QStringLiteral("#111 #222"));
        QCOMPARE(tokens.size(), 2);
        QCOMPARE(tokens.at(0).start, 0);
        QCOMPARE(tokens.at(1).start, 5);
        QCOMPARE(tokens.at(1).color, QColor(0x22, 0x22, 0x22));
    }

    void parse_accepts_and_rejects()
    {
        QColor c;
        QVERIFY(parseHexColor(QStringLiteral("#ff0000"), &c));
        QCOMPARE(c, QColor(0xff, 0x00, 0x00));
        QVERIFY(parseHexColor(QStringLiteral("ff0000"), &c));
        QCOMPARE(c, QColor(0xff, 0x00, 0x00));
        QVERIFY(parseHexColor(QStringLiteral("#f00"), &c));
        QCOMPARE(c, QColor(0xff, 0x00, 0x00));

        QColor untouched(0x12, 0x34, 0x56);
        QVERIFY(!parseHexColor(QStringLiteral("#ff00"), &untouched));
        QCOMPARE(untouched, QColor(0x12, 0x34, 0x56));
        QVERIFY(!parseHexColor(QStringLiteral("nope"), &untouched));
        QVERIFY(!parseHexColor(QStringLiteral("#"), &untouched));
    }
};

QTEST_APPLESS_MAIN(TstHexColor)
#include "tst_hexcolor.moc"
