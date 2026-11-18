// SPDX-License-Identifier: GPL-3.0-or-later
// GUI pass 2026-11-17, Task 4: syntax highlighting for the supported token
// types, adapted to the Borland color scheme. The bundled CRTKTV theme used to
// paint EVERY token #ffff55 (flat yellow), so a C++ file read as a solid wall
// of yellow with no differentiation at all. This test pins the palette: the
// major token classes must be DISTINCT colors drawn from the TV/DOS 16-color
// set, all legible on the #0000AA editor background.
#include <QtTest>
#include <QApplication>
#include <ktexteditor/editor.h>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>
#include "src/editor/EditorTabManager.h"

namespace {
// Register the bundled theme by going through EditorTabManager::applyTvTheme
// - the ONLY path that force-registers the qrc object in a static lib. Calling
// Q_INIT_RESOURCE() directly here would put the call inside this anonymous
// namespace, mangling the symbol to (anonymous namespace)::qInitResources_tvtheme
// and failing the link (documented trap in EditorTabManager.cpp:39-42).
// A member call keeps it at file scope inside the linked library.
KSyntaxHighlighting::Repository& repo()
{
    return const_cast<KSyntaxHighlighting::Repository&>(
        KTextEditor::Editor::instance()->repository());
}

// Load + activate the bundled theme through the real code path (registers the
// resource, adds :/crttv, sets the view's theme), then the repository can serve
// it by name for the color assertions.
void activateTheme()
{
    EditorTabManager m;
    m.newDocument();
    KTextEditor::View* v = m.activeView();
    if (v)
        m.applyTvTheme(v);
}

// The Borland / TV 16-color set the editor may draw with. A token outside this
// set would break the retro scheme (e.g. a modern pastel from a default theme).
bool isTvColor(QRgb c)
{
    const int r = qRed(c), g = qGreen(c), b = qBlue(c);
    // Pure DOS primaries + their bright variants (each channel 0 or ~0xAA/0xFF).
    auto chan = [](int v) { return v == 0 || v >= 0x55; };
    // Reject intermediate grays that are not part of the DOS palette.
    if (r == g && g == b && r != 0 && r != 0xAA && r != 0xFF)
        return false;
    return chan(r) && chan(g) && chan(b);
}

QColor rgbToColor(QRgb c) { return QColor::fromRgb(c); }
} // namespace

class TstSyntax : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QVERIFY2(KTextEditor::Editor::instance(), "no KTextEditor");
        activateTheme();   // registers :/crttv through the linked library
    }

    // The theme registers and loads as "CRTKTV".
    void crt_theme_is_registered_and_valid()
    {
        const KSyntaxHighlighting::Theme t = repo().theme(QStringLiteral("CRTKTV"));
        QVERIFY2(t.isValid(), "bundled CRTKTV theme failed to load from :/crttv");
        QCOMPARE(t.name(), QStringLiteral("CRTKTV"));
        // Default (Normal) text is TV yellow. NOTE: backgroundColor(Normal)
        // reads the per-style background (unset -> black), NOT the
        // editor-colors.BackgroundColor field; the editor field itself is
        // pinned blue by applyTvStyle and asserted in tst_tveditor's
        // editor_field_is_flat_tv_blue, so it is not re-asserted here.
        QCOMPARE(rgbToColor(t.textColor(KSyntaxHighlighting::Theme::Normal)),
                 QColor(0xFF, 0xFF, 0x55));
    }

    // The core bug: EVERY token used to be #ffff55. Keyword, string, comment
    // and number must be four DIFFERENT colors, or there is no highlighting.
    void token_classes_are_distinct_colors()
    {
        const KSyntaxHighlighting::Theme t = repo().theme(QStringLiteral("CRTKTV"));
        QVERIFY(t.isValid());
        const QRgb kw = t.textColor(KSyntaxHighlighting::Theme::Keyword);
        const QRgb str = t.textColor(KSyntaxHighlighting::Theme::String);
        const QRgb com = t.textColor(KSyntaxHighlighting::Theme::Comment);
        const QRgb num = t.textColor(KSyntaxHighlighting::Theme::DecVal);
        QVERIFY2(kw != str && kw != com && kw != num && str != com && str != num && com != num,
                 qPrintable(QStringLiteral(
                                "token classes not distinct: keyword=%1 string=%2 comment=%3 "
                                "number=%4")
                                .arg(QString::number(kw, 16))
                                .arg(QString::number(str, 16))
                                .arg(QString::number(com, 16))
                                .arg(QString::number(num, 16))));

        // And none of them may be the old flat yellow #ffff55 (that was the
        // bug: the whole file was one color). Normal text may be yellow, but
        // these four must not ALL collapse to it.
        const QRgb yellow = 0xffffff55;   // fromRgb(#ffff55)
        QVERIFY2(!(kw == yellow && str == yellow && com == yellow && num == yellow),
                 "keyword/string/comment/number are all still flat yellow");
    }

    // Everything must stay in the Borland 16-color set so the editor reads as a
    // DOS IDE, not a modern pastel theme.
    void all_token_colors_stay_in_the_tv_palette()
    {
        const KSyntaxHighlighting::Theme t = repo().theme(QStringLiteral("CRTKTV"));
        QVERIFY(t.isValid());
        QStringList outside;
        for (int s = KSyntaxHighlighting::Theme::Normal;
             s <= KSyntaxHighlighting::Theme::Others; ++s) {
            const auto style = static_cast<KSyntaxHighlighting::Theme::TextStyle>(s);
            const QRgb c = t.textColor(style);
            if (!isTvColor(c))
                outside << QStringLiteral("%1=%2").arg(s).arg(QString::number(c, 16));
        }
        QVERIFY2(outside.isEmpty(),
                 qPrintable(QStringLiteral(
                                "%1 token colors fall outside the Borland 16-color palette: %2")
                                .arg(outside.size())
                                .arg(outside.join(QStringLiteral(", ")))));
    }

    // Comments must be readable but visually quieter than code: a distinct,
    // muted green (not the bright yellow of active code).
    void comments_are_a_muted_green()
    {
        const KSyntaxHighlighting::Theme t = repo().theme(QStringLiteral("CRTKTV"));
        QVERIFY(t.isValid());
        const QColor com = rgbToColor(t.textColor(KSyntaxHighlighting::Theme::Comment));
        QVERIFY2(com.green() > com.red() && com.green() > com.blue(),
                 qPrintable(QStringLiteral("comment color is not green-ish: %1")
                                .arg(com.name())));
        // Must be darker than the bright yellow code color so it recedes.
        const QColor code = rgbToColor(t.textColor(KSyntaxHighlighting::Theme::Normal));
        QVERIFY2(com.lightness() < code.lightness(),
                 qPrintable(QStringLiteral("comment %1 is not quieter than code %2")
                                .arg(com.name(), code.name())));
    }

    // Keywords (control flow, types) must be the classic TV cyan so a reader's
    // eye finds structure instantly.
    void keywords_and_control_flow_are_cyan()
    {
        const KSyntaxHighlighting::Theme t = repo().theme(QStringLiteral("CRTKTV"));
        QVERIFY(t.isValid());
        const QColor kw = rgbToColor(t.textColor(KSyntaxHighlighting::Theme::Keyword));
        const QColor cf = rgbToColor(t.textColor(KSyntaxHighlighting::Theme::ControlFlow));
        QVERIFY2(qAbs(kw.red() - 0x00) < 64 && kw.blue() > 0x80,
                 qPrintable(QStringLiteral("keyword is not cyan: %1").arg(kw.name())));
        QCOMPARE(cf, kw);   // control flow reads the same as keywords
    }
};

QTEST_MAIN(TstSyntax)
#include "tst_syntax.moc"
