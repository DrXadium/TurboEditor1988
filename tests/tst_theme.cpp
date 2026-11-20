// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: single-quoted text must use #40A769 in files that HAVE a syntax
// mode, not just in mode-None documents.
//
// The user's requirement: "the text in single quotes should be #40A769". In a
// mode-None document our MovingRange pass does this. In a code file the
// highlighter owns the colour, and a range attribute cannot recolour text the
// highlighter has styled (measured: a magenta range over a string span changed
// nothing). The fix is therefore in the theme: Kate assigns Char to single-quoted
// literals, so crt_tv.theme's Char must be #40A769 while String stays #55FF55.
//
// This suite pins the theme file itself, so a silent revert fails the gate.
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

class TstTheme : public QObject
{
    Q_OBJECT

    static QJsonObject loadTheme()
    {
        QFile f(QStringLiteral(CRT_KATE_SOURCE_DIR
                               "/src/editor/tvtheme/themes/crt_tv.theme"));
        if (!f.open(QIODevice::ReadOnly))
            return QJsonObject();
        return QJsonDocument::fromJson(f.readAll()).object();
    }

private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // The theme must be a readable KSyntaxHighlighting theme JSON document.
    void the_theme_file_is_valid_json()
    {
        const QJsonObject o = loadTheme();
        QVERIFY2(!o.isEmpty(), "crt_tv.theme could not be parsed");
        QCOMPARE(o.value(QStringLiteral("metadata")).toObject()
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("CRTKTV"));
    }

    // THE assertion: single-quoted literals (Kate's Char style) are the user's
    // single-quote colour, and double-quoted text (String) is NOT.
    void single_quoted_text_uses_the_single_quote_colour()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        const QString single = styles.value(QStringLiteral("Char")).toObject()
                                   .value(QStringLiteral("text-color")).toString();
        const QString dbl = styles.value(QStringLiteral("String")).toObject()
                                .value(QStringLiteral("text-color")).toString();

        QCOMPARE(single.toLower(), QStringLiteral("#40a769"));
        // Double-quoted text keeps the green, so the two shades differ.
        QCOMPARE(dbl.toLower(), QStringLiteral("#55ff55"));
        QVERIFY2(single.compare(dbl, Qt::CaseInsensitive) != 0,
                 "both quote kinds share a colour - the nesting cue is lost");
    }

    // Related single-quoted styles follow Char so a literal reads as one shade.
    void related_single_quoted_styles_follow_char()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        const QString single = styles.value(QStringLiteral("Char")).toObject()
                                   .value(QStringLiteral("text-color")).toString().toLower();
        // SpecialChar is the escape-sequence flavour of a char literal.
        QCOMPARE(styles.value(QStringLiteral("SpecialChar")).toObject()
                     .value(QStringLiteral("text-color")).toString().toLower(),
                 single);
    }

    // Sanity: the palette the user sees must not be empty for strings either.
    void double_quoted_text_still_has_a_colour()
    {
        const QJsonObject styles = loadTheme().value(QStringLiteral("text-styles"))
                                       .toObject();
        QVERIFY(!styles.value(QStringLiteral("String")).toObject()
                   .value(QStringLiteral("text-color")).toString().isEmpty());
    }
};

QTEST_MAIN(TstTheme)
#include "tst_theme.moc"
