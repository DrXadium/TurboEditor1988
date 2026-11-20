// SPDX-License-Identifier: GPL-3.0-or-later
// Status-line contract (2026-11-19): the left line is a whole-document
// word/character count, NOT the old F-key reminder. The bar must render the
// counts with black labels and red values, and must never show an F-key.
#include <QtTest>
#include <QAction>
#include <QKeySequence>
#include <QImage>
#include <QRegularExpression>
#include <QLabel>
#include <QSet>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextBlock>
#include <QApplication>
#include "src/shell/ShellWindow.h"
#include "src/shell/MenuBar.h"
#include "src/shell/FKeyStatusBar.h"

class TstFKeys : public QObject
{
    Q_OBJECT
private slots:
    // The whole reason the line exists: word count then character count,
    // labels black (#000000), values red (#AA0000), no F-key reminder.
    // Strip the span tags so assertions read the line a human sees.
    static QString plain(const QString& html)
    {
        QString s = html;
        s.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
        return s.simplified();
    }

    void count_line_shows_word_and_character_counts_with_ink()
    {
        ShellWindow w;
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        bar->setCounts(3, 14);
        const QString text = plain(bar->displayedText());
        QVERIFY2(text.contains(QStringLiteral("Word Count: 3")),
                 qPrintable(QStringLiteral("no word count: %1").arg(text)));
        QVERIFY2(text.contains(QStringLiteral("Character Count: 14")),
                 qPrintable(QStringLiteral("no character count: %1").arg(text)));
        QVERIFY2(!text.contains(QStringLiteral("F2")) && !text.contains(QStringLiteral("F10")),
                 qPrintable(QStringLiteral("F-key reminder survived: %1").arg(text)));

        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        const QImage img = bar->grab().toImage().convertToFormat(QImage::Format_RGB32);
        int red = 0, black = 0, yellow = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0xAA) < 40 && c.green() < 50 && c.blue() < 50) ++red;
                if (c.red() < 40 && c.green() < 40 && c.blue() < 40) ++black;
                if (qAbs(c.red() - 0xFF) <= 30 && qAbs(c.green() - 0xFF) <= 30
                    && qAbs(c.blue() - 0x55) <= 40) ++yellow;
            }
        QVERIFY2(red >= 3, qPrintable(QStringLiteral("no red count values (%1 px)").arg(red)));
        QVERIFY2(black >= 10, qPrintable(QStringLiteral("no black labels (%1 px)").arg(black)));
        QVERIFY2(yellow == 0, qPrintable(QStringLiteral("yellow px: %1").arg(yellow)));
    }

    // An empty (or whitespace-only) document is 0 words and 0 characters: the
    // line must still render, never collapse or crash.
    void empty_document_counts_zero_words_and_characters()
    {
        ShellWindow w;
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        bar->setCounts(0, 0);
        const QString text = plain(bar->displayedText());
        QVERIFY2(text.contains(QStringLiteral("Word Count: 0")),
                 qPrintable(QStringLiteral("zero word count missing: %1").arg(text)));
        QVERIFY2(text.contains(QStringLiteral("Character Count: 0")),
                 qPrintable(QStringLiteral("zero character count missing: %1").arg(text)));
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        QVERIFY(!bar->grab().isNull());
    }

    // Tab switch contract: the composition root re-points the counts, so the
    // line must show the LAST numbers it was given, not the first document's.
    void counts_follow_a_repointed_document()
    {
        ShellWindow w;
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        bar->setCounts(2, 7);          // doc A: "aaa bbb"
        QVERIFY2(plain(bar->displayedText()).contains(QStringLiteral("Word Count: 2")),
                 "doc A counts not shown");
        bar->setCounts(1, 1);          // doc B: "x"
        QVERIFY2(plain(bar->displayedText()).contains(QStringLiteral("Word Count: 1")),
                 qPrintable(QStringLiteral("re-point failed: %1").arg(bar->displayedText())));
        QVERIFY2(plain(bar->displayedText()).contains(QStringLiteral("Character Count: 1")),
                 qPrintable(QStringLiteral("char re-point failed: %1").arg(bar->displayedText())));
    }

    // TV-revision spec 7: the status line is a FLAT solid #AAAAAA row across
    // the full width (every sampled row-pixel is gray), with no texture.
    void fkey_bar_is_flat_gray_full_width() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QImage img = bar->grab().toImage();
        QVERIFY(!img.isNull());
        // Sample several rows across the full bar width; all must be gray.
        int nonGray = 0;
        QString bad;
        const int rows[] = { 1, img.height() / 2, img.height() - 2 };
        for (int y : rows)
            for (int x = 0; x < img.width(); x += 7) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0xAA) > 14 || qAbs(c.green() - 0xAA) > 14
                    || qAbs(c.blue() - 0xAA) > 14) {
                    // The F-key text glyphs (black/red) sit on the middle row;
                    // only those are allowed off-gray there, not on top/bottom.
                    if (y != img.height() / 2) {
                        ++nonGray;
                        if (bad.size() < 80)
                            bad += QStringLiteral("(%1,%2) ").arg(x).arg(y);
                    }
                }
            }
        QVERIFY2(nonGray == 0,
                 qPrintable(QStringLiteral("status bar not flat gray at %1 sample(s): %2")
                                .arg(nonGray).arg(bad)));
    }

    // TV-revision spec 7: NO yellow (#FFFF55) anywhere in the status bar.
    void fkey_bar_has_no_yellow() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        const QImage img = bar->grab().toImage();
        int yellow = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - 0xFF) <= 30 && qAbs(c.green() - 0xFF) <= 30
                    && qAbs(c.blue() - 0x55) <= 40)
                    ++yellow;
            }
        QVERIFY2(yellow == 0,
                 qPrintable(QStringLiteral("status bar contains yellow px: %1").arg(yellow)));
    }

    // TV-revision spec 7: the line starts near column 12 (first red key F2 at
    // x in [8,20]).
    // The count line still STARTS near column 12: the BLACK "Word Count:"
    // label's first TRUE-BLACK pixel is the line's left edge, and the first
    // RED pixel (the word-count value) must follow AFTER that label, not at
    // the line start (the old F-key's red key name sat at x=8..20 - retired).
    // Scans the WHOLE label band: a single mid-row scan misses the glyph body
    // depending on the 16px VGA font grid, reporting only faint antialias.
    void fkey_line_starts_near_column_12() {
        ShellWindow w;
        w.resize(700, 400);
        w.show();
        QCoreApplication::processEvents();
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        bar->setCounts(3, 14);
        QCoreApplication::processEvents();
        const QImage img = bar->grab().toImage().convertToFormat(QImage::Format_RGB32);
        int firstBlackX = -1, firstRedX = -1;
        for (int x = 0; x < img.width(); ++x) {
            for (int y = 3; y < img.height() - 3; ++y) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 30 && c.green() < 30 && c.blue() < 30 && firstBlackX < 0)
                    firstBlackX = x;
                if (c.red() > 160 && c.green() < 70 && c.blue() < 70 && firstRedX < 0)
                    firstRedX = x;
            }
        }
        QVERIFY2(firstBlackX >= 8 && firstBlackX <= 24,
                 qPrintable(QStringLiteral("count line first black at x=%1, expected 8..24")
                                .arg(firstBlackX)));
        QVERIFY2(firstRedX > firstBlackX,
                 qPrintable(QStringLiteral("red value (%1) not after the black label (%2)")
                                .arg(firstRedX).arg(firstBlackX)));
    }

    void menu_actions_accept_fkey_shortcuts() {
        // The File > Save action must be assignable the F2 shortcut (and keep
        // a shortcut slot for F3/F5-style extras). This mirrors main.cpp's
        // addFKey: an action can carry both its base and an F-key.
        ShellWindow w;
        MenuBar* mb = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY(mb);
        QAction* save = mb->findChild<QAction*>(QStringLiteral("actionFileSave"));
        QVERIFY2(save, "actionFileSave missing");
        save->setShortcuts({ QKeySequence(QStringLiteral("Ctrl+S")),
                             QKeySequence(QStringLiteral("F2")) });
        QCOMPARE(save->shortcuts().size(), 2);
        QCOMPARE(save->shortcut().toString(), QStringLiteral("Ctrl+S"));

        QAction* open = mb->findChild<QAction*>(QStringLiteral("actionFileOpen"));
        QVERIFY(open);
        open->setShortcuts({ open->shortcut(), QKeySequence(QStringLiteral("F3")) });
        QVERIFY(open->shortcuts().size() >= 1);
    }

    // TV-revision2 spec 7: while a dropdown is open the status line is
    // REPLACED by "F1 Help | <hint>": F1 red, "Help" black, a black vertical
    // bar, then the hint text in black. Covers all five View-item hints.
    void status_line_renders_hint_format_when_dropdown_open() {
        const QList<QPair<QString, QString>> hints = {
            { QStringLiteral("Zoom In"), QStringLiteral("Increase the text size") },
            { QStringLiteral("Zoom Out"), QStringLiteral("Decrease the text size") },
            { QStringLiteral("Reset Zoom"), QStringLiteral("Restore the default text size") },
            { QStringLiteral("Show Minimap"), QStringLiteral("Toggle the document minimap") },
            { QStringLiteral("Show Folding Bar"),
              QStringLiteral("Toggle the code folding bar") },
        };
        FKeyStatusBar bar;
        bar.resize(900, 26);
        bar.show();
        QCoreApplication::processEvents();

        for (const auto& h : hints) {
            bar.setContextHint(h.second);
            QVERIFY2(bar.contextHintActive(),
                     qPrintable(QStringLiteral("hint not active for %1").arg(h.first)));
            const QString text = bar.displayedText();
            QVERIFY2(text.startsWith(QStringLiteral("F1 Help")),
                     qPrintable(QStringLiteral("hint format wrong: %1").arg(text)));
            QVERIFY2(text.contains(QStringLiteral("\u2502")),
                     qPrintable(QStringLiteral("hint missing the | separator: %1").arg(text)));
            QVERIFY2(text.contains(h.second),
                     qPrintable(QStringLiteral("hint text missing for %1: %2")
                                    .arg(h.first, text)));

            const QImage img = bar.grab().toImage();
            QVERIFY(!img.isNull());
            int red = 0, black = 0, gray = 0, total = 0;
            int firstRedX = 9999, firstBlackX = 9999;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    ++total;
                    if (qAbs(c.red() - 0xAA) < 40 && c.green() < 50 && c.blue() < 50) {
                        ++red;
                        firstRedX = qMin(firstRedX, x);
                    }
                    if (c.red() < 40 && c.green() < 40 && c.blue() < 40) {
                        ++black;
                        firstBlackX = qMin(firstBlackX, x);
                    }
                    if (qAbs(c.red() - 0xAA) < 30 && qAbs(c.green() - 0xAA) < 30
                        && qAbs(c.blue() - 0xAA) < 30)
                        ++gray;
                }
            QVERIFY2(red >= 3,
                     qPrintable(QStringLiteral("F1 not red for %1 (%2 px)")
                                    .arg(h.first).arg(red)));
            QVERIFY2(black >= 10,
                     qPrintable(QStringLiteral("hint/Help text not black for %1 (%2 px)")
                                    .arg(h.first).arg(black)));
            QVERIFY2(gray * 2 >= total,
                     qPrintable(QStringLiteral("status bar not flat gray for %1")
                                    .arg(h.first)));
            // Structure: the red F1 sits LEFT of the black Help/hint text.
            QVERIFY2(firstRedX < firstBlackX,
                     qPrintable(QStringLiteral("F1 not left of Help for %1 (red@%2 black@%3)")
                                    .arg(h.first).arg(firstRedX).arg(firstBlackX)));
        }

        bar.clearContextHint();
        QVERIFY(!bar.contextHintActive());
        // Restores the COUNT line, not the retired F-key reminder.
        QVERIFY2(plain(bar.displayedText()).contains(QStringLiteral("Word Count")),
                 qPrintable(QStringLiteral("count line did not restore: %1")
                                .arg(bar.displayedText())));
    }






























    // The counting rule the status line runs on: whitespace-separated words
    // over the WHOLE document. These pin the cases a naive split gets wrong.
    void word_count_ignores_whitespace_only_text()
    {
        QCOMPARE(FKeyStatusBar::wordCount(QString()), qint64(0));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("")), qint64(0));
        // Whitespace-only documents are 0 words, not 1 (a naive split() emits
        // empty tokens and reports a word for a blank file).
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("   ")), qint64(0));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("\n\n\n")), qint64(0));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral(" \t\r\n  \t ")), qint64(0));
    }

    void word_count_splits_on_any_whitespace()
    {
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("one")), qint64(1));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("one two\nthree")), qint64(3));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("one\ttwo\tthree")), qint64(3));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("one\r\ntwo")), qint64(2));
        // Leading / trailing / repeated whitespace never adds or drops a word.
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("  one  two  ")), qint64(2));
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("a\n\n\nb")), qint64(2));
        // Punctuation stays inside its word (Kate-class behaviour: no stemming).
        QCOMPARE(FKeyStatusBar::wordCount(QStringLiteral("hello, world!")), qint64(2));
        // Non-ASCII words count as words.
        QCOMPARE(FKeyStatusBar::wordCount(QString::fromUtf8("café déjà vu")), qint64(3));
    }

    // The line the composition root feeds: counts are independent of the label
    // (a zero-character doc still shows both labels).
    void count_line_carries_both_numbers_for_a_real_document()
    {
        const QString text = QStringLiteral("alpha beta\ngamma");
        ShellWindow w;
        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        bar->setCounts(FKeyStatusBar::wordCount(text), text.size());
        const QString line = plain(bar->displayedText());
        QVERIFY2(line.contains(QStringLiteral("Word Count: 3")), qPrintable(line));
        QVERIFY2(line.contains(QStringLiteral("Character Count: 16")), qPrintable(line));
    }


};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFKeys tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_fkeys.moc"
