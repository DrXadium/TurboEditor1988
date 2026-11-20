// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-19: the Edit menu's line tools (affix to start/end of line, join
// lines) and the Word Wrap toggle. The transforms are pure static helpers so
// the edge cases - phantom last line, CRLF, break runs, empty documents - are
// pinned without a view; the document-level tests drive a real KTextEditor view.
//
// TDD: written before affixLines/removeLineBreaksText exist.
#include <QtTest>
#include <QApplication>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

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
