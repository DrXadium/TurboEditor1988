// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-19: the Edit menu's line tools (affix to start/end of line, join
// lines) and the Word Wrap toggle. The transforms are pure static helpers so
// the edge cases - phantom last line, CRLF, break runs, empty documents - are
// pinned without a view; the document-level tests drive a real KTextEditor view.
//
// TDD: written before affixLines/removeLineBreaksText exist.
#include <QtTest>
#include <QApplication>
#include <QLineEdit>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"
#include "src/editor/LineAffixDialog.h"
#include "src/shell/TvCheckBox.h"

class TstLineTools : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // ---- pure helpers -----------------------------------------------------

    void affix_end_skips_the_phantom_last_line() {
        QCOMPARE(EditorTabManager::affixLines(QStringLiteral("a\nb\n"),
                                              QStringLiteral(";"), true),
                 QStringLiteral("a;\nb;\n"));
    }

    void affix_end_no_trailing_newline() {
        QCOMPARE(EditorTabManager::affixLines(QStringLiteral("a\nb"),
                                              QStringLiteral(";"), true),
                 QStringLiteral("a;\nb;"));
    }

    void affix_start() {
        QCOMPARE(EditorTabManager::affixLines(QStringLiteral("a\nb"),
                                              QStringLiteral("//"), false),
                 QStringLiteral("//a\n//b"));
    }

    void affix_empty_string_is_identity() {
        QCOMPARE(EditorTabManager::affixLines(QStringLiteral("a\nb"),
                                              QString(), true),
                 QStringLiteral("a\nb"));
    }

    void affix_crlf_keeps_the_carriage_return_after_the_affix() {
        QCOMPARE(EditorTabManager::affixLines(QStringLiteral("a\r\nb"),
                                              QStringLiteral(";"), true),
                 QStringLiteral("a;\r\nb;"));
    }

    void affix_empty_document() {
        QCOMPARE(EditorTabManager::affixLines(QString(), QStringLiteral(";"), true),
                 QStringLiteral(";"));
    }

    // ---- stripEdgeText (2026-11-19, remove mode) --------------------------

    void strip_edge_removes_the_start_of_each_line() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("// a\n// b"),
                                                 QStringLiteral("// "), false),
                 QStringLiteral("a\nb"));
    }

    void strip_edge_removes_the_end_of_each_line() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("a ;\nb ;"),
                                                 QStringLiteral(" ;"), true),
                 QStringLiteral("a\nb"));
    }

    void strip_edge_removes_only_the_leading_match() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("// a // b"),
                                                 QStringLiteral("// "), false),
                 QStringLiteral("a // b"));
    }

    void strip_edge_skips_the_phantom_last_line() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("a\n"),
                                                 QStringLiteral("a"), false),
                 QStringLiteral("\n"));
    }

    void strip_edge_keeps_the_carriage_return() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("// x\r\ny"),
                                                 QStringLiteral("// "), false),
                 QStringLiteral("x\r\ny"));
    }

    void strip_edge_on_empty_string_is_identity() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("// a"), QString(), false),
                 QStringLiteral("// a"));
    }

    void strip_edge_with_no_match_is_identity() {
        QCOMPARE(EditorTabManager::stripEdgeText(QStringLiteral("x"),
                                                 QStringLiteral("// "), false),
                 QStringLiteral("x"));
    }

    void remove_breaks_joins_with_one_space() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("one\ntwo")),
                 QStringLiteral("one two"));
    }

    void remove_breaks_run_becomes_one_space() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("one\n\n\ntwo")),
                 QStringLiteral("one two"));
    }

    void remove_breaks_keeps_existing_spaces() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("one\n two")),
                 QStringLiteral("one  two"));
    }

    void remove_breaks_drops_edge_spaces_from_breaks() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("\none\ntwo\n")),
                 QStringLiteral("one two"));
    }

    void remove_breaks_handles_crlf() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("one\r\ntwo")),
                 QStringLiteral("one two"));
    }

    void remove_breaks_without_breaks_is_identity() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QStringLiteral("one two")),
                 QStringLiteral("one two"));
    }

    void remove_breaks_on_empty_is_empty() {
        QCOMPARE(EditorTabManager::removeLineBreaksText(QString()), QString());
    }

    // ---- document-level application ---------------------------------------

    void remove_breaks_joins_the_selection_only() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("one\ntwo\nthree"));
        v->setSelection(KTextEditor::Range(0, 0, 1, 3));
        m.removeLineBreaks();
        QCOMPARE(v->document()->text(), QStringLiteral("one two\nthree"));
    }

    void remove_breaks_whole_document_without_selection() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("one\ntwo"));
        m.removeLineBreaks();
        QCOMPARE(v->document()->text(), QStringLiteral("one two"));
    }

    void remove_breaks_on_blank_document_is_a_no_op() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("   "));
        m.removeLineBreaks();
        QCOMPARE(v->document()->text(), QStringLiteral("   "));
    }

    void affix_end_applies_to_the_selection() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("a\nb\nc"));
        v->setSelection(KTextEditor::Range(0, 0, 1, 1));   // "a\nb"
        m.affixActiveLines(QStringLiteral(";"), true);
        QCOMPARE(v->document()->text(), QStringLiteral("a;\nb;\nc"));
    }

    void affix_start_applies_to_the_selection() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("a\nb\nc"));
        v->setSelection(KTextEditor::Range(1, 0, 1, 1));   // "b"
        m.affixActiveLines(QStringLiteral("//"), false);
        QCOMPARE(v->document()->text(), QStringLiteral("a\n//b\nc"));
    }

    void affix_mid_line_selection_does_not_expand() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("abc\ndef"));
        v->setSelection(KTextEditor::Range(0, 1, 0, 2));   // "b"
        m.affixActiveLines(QStringLiteral(";"), true);
        // The affix lands at the selection edge, not at the end of the line:
        // no whole-line expansion, and the 'c' outside the selection is intact.
        QCOMPARE(v->document()->text(), QStringLiteral("ab;c\ndef"));
    }

    void empty_affix_leaves_the_document_alone() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("a\nb"));
        m.affixActiveLines(QString(), true);
        QCOMPARE(v->document()->text(), QStringLiteral("a\nb"));
    }

    // ---- the affix dialog -------------------------------------------------

    void end_dialog_has_a_field_and_an_unchecked_break_box() {
        LineAffixDialog d(LineAffixDialog::Mode::End);
        d.resize(d.sizeHint());
        d.show();
        QVERIFY(d.field());
        QVERIFY(d.lineBreakBox());
        QVERIFY(!d.lineBreakBox()->isChecked());
        QVERIFY(d.field()->isEnabled());
    }

    void break_box_disables_the_field() {
        LineAffixDialog d(LineAffixDialog::Mode::End);
        d.resize(d.sizeHint());
        d.show();
        d.lineBreakBox()->setChecked(true);
        QVERIFY(d.wantsLineBreak());
        QVERIFY2(!d.field()->isEnabled(),
                 "a line-break affix must ignore the typed text, so the field goes disabled");
        d.lineBreakBox()->setChecked(false);
        QVERIFY(d.field()->isEnabled());
    }

    void start_dialog_has_no_break_box() {
        LineAffixDialog d(LineAffixDialog::Mode::Start);
        QVERIFY(!d.lineBreakBox());
        QVERIFY(!d.wantsLineBreak());
    }

    void dialog_reports_typed_text() {
        LineAffixDialog d(LineAffixDialog::Mode::End);
        d.field()->setText(QStringLiteral("//"));
        QCOMPARE(d.text(), QStringLiteral("//"));
    }

    void dialog_title_names_the_direction() {
        LineAffixDialog end(LineAffixDialog::Mode::End);
        LineAffixDialog start(LineAffixDialog::Mode::Start);
        QVERIFY(end.title().contains(QStringLiteral("End")));
        QVERIFY(start.title().contains(QStringLiteral("Start")));
    }
};

int main(int argc, char** argv)
{
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstLineTools tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_linetools.moc"
