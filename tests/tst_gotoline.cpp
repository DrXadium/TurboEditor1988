// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-19 (second round): View > Go. The dialog collects a line number and
// EditorTabManager moves the caret there. The off-by-one (human 1-based in, 0-based
// cursor out) and the validator-vs-clamp split are the two places this can go wrong,
// so both are pinned directly.
//
// TDD: written before GoToLineDialog/goToLine exist.
#include <QtTest>
#include <QApplication>
#include <QLineEdit>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"
#include "src/editor/GoToLineDialog.h"

class TstGoToLine : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // ---- the dialog --------------------------------------------------------

    void field_starts_empty() {
        GoToLineDialog d(10);
        QVERIFY(d.field());
        QCOMPARE(d.field()->text(), QString());
    }

    void accepts_a_valid_line() {
        GoToLineDialog d(10);
        d.field()->setText(QStringLiteral("7"));
        QCOMPARE(d.line(), 7);
    }

    void clamps_an_out_of_range_line() {
        GoToLineDialog d(10);
        d.field()->setText(QStringLiteral("99"));
        QCOMPARE(d.line(), 10);
        d.field()->setText(QStringLiteral("0"));
        QCOMPARE(d.line(), 1);
        d.field()->setText(QStringLiteral("-4"));
        QCOMPARE(d.line(), 1);
    }

    void non_numeric_input_reads_as_line_one() {
        GoToLineDialog d(10);
        d.field()->setText(QStringLiteral("abc"));
        QCOMPARE(d.line(), 1);
    }

    // Qt validators allow INTERMEDIATE states so multi-digit numbers can be
    // typed: for range [1,10], "9" is Valid, "99" is Intermediate (a waypoint
    // toward a valid answer), "999" is Invalid. A leading "0" is Intermediate
    // too, because it is a legal prefix. What matters is that no value ever
    // escapes the range when interpreted - line() clamps whatever lands.
    void validator_refuses_typing_past_the_last_line() {
        GoToLineDialog d(10);
        QLineEdit* f = d.field();

        QTest::keyClicks(f, QStringLiteral("999"));
        QVERIFY2(f->text() != QStringLiteral("999"),
                 qPrintable(QStringLiteral("invalid final value stuck: '%1'").arg(f->text())));

        f->setText(QString());
        QTest::keyClicks(f, QStringLiteral("-1"));
        QVERIFY2(!f->text().startsWith(QLatin1Char('-')), "a negative sign must not land");

        // Whatever can land is still interpreted in-range, always.
        f->setText(QString());
        QTest::keyClicks(f, QStringLiteral("0"));
        QVERIFY2(d.line() >= 1 && d.line() <= 10,
                 qPrintable(QStringLiteral("line() out of range: %1").arg(d.line())));

        // A valid in-range number types through and reads back exactly.
        f->setText(QString());
        QTest::keyClicks(f, QStringLiteral("5"));
        QCOMPARE(f->text(), QStringLiteral("5"));
        QCOMPARE(d.line(), 5);
    }

    void dialog_title_names_the_line() {
        GoToLineDialog d(10);
        QCOMPARE(d.title(), QStringLiteral("Go to Line"));
    }

    // ---- the jump ----------------------------------------------------------

    void go_to_line_one_lands_on_cursor_line_zero() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("a\nb\nc"));
        m.goToLine(1);
        QCOMPARE(v->cursorPosition().line(), 0);
        m.goToLine(3);
        QCOMPARE(v->cursorPosition().line(), 2);
    }

    void go_to_line_clamps_a_pasted_value() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("a\nb\nc"));
        m.goToLine(99);                       // a paste can bypass the validator
        QCOMPARE(v->cursorPosition().line(), 2);   // clamped to lines() - 1
        m.goToLine(0);
        QCOMPARE(v->cursorPosition().line(), 0);   // and floored at the first line
    }

    void go_is_safe_without_a_view() {
        EditorTabManager m;                   // no document at all
        m.goToLine(5);                        // must not crash or touch anything
        QVERIFY(true);
    }
};

int main(int argc, char** argv)
{
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstGoToLine tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_gotoline.moc"
