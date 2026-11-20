// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: Coloured Brackets, the PURE pair-matching + colour-assignment
// logic (Kate's rainbow-parens algorithm, ported). No widgets, no KTextEditor:
// these are the assertions that fail loudly. The MovingRange wiring is verified
// through the renderer in tst_brackets (lineAttributes() is NOT the render path).
#include <QtTest>
#include "src/editor/BracketColorizer.h"

using BracketColorizer::Coloured;
using BracketColorizer::colorize;
using BracketColorizer::defaultColors;

namespace {
QVector<QString> lines(std::initializer_list<const char*> ls)
{
    QVector<QString> out;
    for (const char* s : ls)
        out << QString::fromLatin1(s);
    return out;
}
const BracketColorizer::StyleProbe plain = [](int, int) { return false; };
} // namespace

class TstBrackets : public QObject
{
    Q_OBJECT
private slots:
    void palette_has_five_kate_colours()
    {
        QCOMPARE(defaultColors().size(), 5);
        QCOMPARE(defaultColors().at(0), QColor(0x12, 0x75, 0xEF));
        QCOMPARE(defaultColors().at(4), QColor(0x37, 0xD2, 0x1C));
    }

    void a_pair_gets_one_colour_for_both_brackets()
    {
        // A single pair: opener and closer are both reported, opener first.
        const auto got = colorize(lines({ "if (x)" }), 0, 0, plain, 5);
        QVERIFY2(got.size() == 1,
                 qPrintable(QStringLiteral("expected 1 pair, got %1").arg(got.size())));
        QCOMPARE(got[0].color, 0);          // first pair, first colour
        QVERIFY(got[0].closeCol > got[0].col);
    }

    void nested_pairs_get_different_colours()
    {
        // "{ a ( b ) c }" -> outer/inner must differ (the user's core request).
        const auto got = colorize(lines({ "f({ a })" }), 0, 0, plain, 5);
        QCOMPARE(got.size(), 2);
        QVERIFY2(got[0].color != got[1].color,
                 "nested pair reused the outer pair's colour");
    }

    void colours_repeat_but_never_adjacent()
    {
        // Six sibling pairs, 5 colours: colour 5 (index 0) must repeat, but never
        // right after index 0 — i.e. prevColor == color ? color + 1 : color.
        QVector<QString> doc;
        QString row;
        for (int i = 0; i < 6; ++i)
            row += QStringLiteral("a(b%1)").arg(i);
        doc << row;
        const auto got = colorize(doc, 0, 0, plain, 5);
        QCOMPARE(got.size(), 6);
        for (int i = 1; i < got.size(); ++i)
            QVERIFY2(got[i].color != got[i - 1].color,
                     qPrintable(QStringLiteral("pairs %1 and %2 share colour %3")
                                    .arg(i - 1).arg(i).arg(got[i].color)));
        QVERIFY2(got[5].color == 0,
                 qPrintable(QStringLiteral("expected the palette to wrap to 0, got %1")
                                .arg(got[5].color)));
    }

    void brackets_in_strings_are_ignored()
    {
        auto stringish = [](int line, int col) {
            return line == 0 && col >= 3 && col <= 4;   // the () inside the quotes
        };
        // s = "()" (inside the string) plus a real pair AFTER it.
        const auto got = colorize(lines({ "s=\"()\"  (r)" }), 0, 0, stringish, 5);
        QVERIFY2(got.size() == 1,
                 qPrintable(QStringLiteral("expected 1 real pair, got %1").arg(got.size())));
        QCOMPARE(got[0].col, 8);            // only the real '(' is coloured
    }

    void viewport_scopes_which_brackets_are_coloured()
    {
        // Two lines, colour only the second: the pair on line 0 must not appear.
        const QVector<QString> doc = lines({ "a(b)", "c(d)" });
        const auto got = colorize(doc, 1, 1, plain, 5);
        QCOMPARE(got.size(), 1);
        QCOMPARE(got[0].line, 1);
    }

    void unmatched_brackets_produce_no_pair()
    {
        const auto got = colorize(lines({ "a(b" }), 0, 0, plain, 5);
        QVERIFY2(got.isEmpty(), "an unclosed bracket was reported as a pair");
    }

    void one_line_empty_pair_is_skipped()
    {
        // "()" with nothing else on the line: upstream skips it (noise).
        const auto got = colorize(lines({ "x = ()" }), 0, 0, plain, 5);
        QVERIFY2(got.isEmpty(), "a lone () pair was coloured");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstBrackets tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_brackets.moc"
