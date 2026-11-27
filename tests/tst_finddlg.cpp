// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3 (2026-11-18 Find and Replace): the dialog's chrome and controls.
//
// The dialog must wear the house style - the same grey panel, black double
// frame, black offset shadow and green plates as Help > About and Settings -
// and it must lay out on the character grid without a control escaping the
// frame or a field growing the white border the user already reported once.
//
// TDD: written before FindReplaceDialog exists.
#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/FindReplaceDialog.h"
#include "src/editor/EditorTabManager.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvTabBar.h"
#include "src/shell/TvLineEdit.h"
#include "src/shell/ShellWindow.h"
#include "src/bridge/ContentBridge.h"
#include <QMouseEvent>

namespace {

int countColorIn(const QImage& img, const QColor& want, int tol, const QRect& r)
{
    int n = 0;
    const QRect clip = r.intersected(img.rect());
    for (int y = clip.top(); y <= clip.bottom(); ++y)
        for (int x = clip.left(); x <= clip.right(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol
                && qAbs(c.green() - want.green()) <= tol
                && qAbs(c.blue() - want.blue()) <= tol)
                ++n;
        }
    return n;
}

bool isPanelGrey(const QColor& c)
{
    return qAbs(c.red() - 0xAA) <= 10 && qAbs(c.green() - 0xAA) <= 10
           && qAbs(c.blue() - 0xAA) <= 10;
}

bool isBlack(const QColor& c)
{
    return c.red() < 16 && c.green() < 16 && c.blue() < 16;
}

QString rgb(const QColor& c)
{
    return QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

// A widget's rect in the dialog's coordinates (fields live inside a cell).
QRect inDialog(QWidget* w, QWidget* root)
{
    return QRect(w->mapTo(root, QPoint(0, 0)), w->size());
}

// The outermost 1 px ring of a rect - where a border would be painted.
int ringPixels(const QRect& r)
{
    return 2 * (r.width() + r.height()) - 4;
}

QImage grabWidget(QWidget* w)
{
    return w->grab().toImage().convertToFormat(QImage::Format_RGB32);
}

// How many yellow pixels the 2 px focus ring around a field's cell carries.
// Asserting on this (not on qApp->focusWidget()) keeps the test independent of
// window activation, which an offscreen platform does not always deliver.
//
// It is a COUNT, not a yes/no, because a field's yellow SELECTION can reach the
// inner edge of the ring and leak a few dozen pixels into it. A live ring is an
// order of magnitude larger than that leak, so the callers compare counts.
int ringYellow(const QImage& img, QLineEdit* field, QWidget* root)
{
    QWidget* cell = field->parentWidget();
    if (!cell)
        return 0;
    const QRect r = inDialog(cell, root).adjusted(0, 0, -1, -1);
    const QColor yellow(0xFF, 0xFF, 0x55);
    const int outer = countColorIn(img, yellow, 30, r);
    const int inner = countColorIn(img, yellow, 30, r.adjusted(2, 2, -2, -2));
    return outer - inner;
}

} // namespace

class TstFindDlg : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void the_dialog_is_house_style();
    void field_labels_are_black_and_options_are_yellow();
    void every_control_exists_with_a_usable_geometry();
    void the_fields_are_tv_blue_with_no_border();
    void clicking_a_field_marks_it_active();
    void the_status_row_starts_blank();

    // Task 4: the option and scope rules.
    void ticking_regex_disables_whole_words();
    void the_selection_chip_needs_a_selection();
    void an_empty_pattern_disables_the_action_buttons();
    void the_dialog_opens_on_the_find_field_with_the_last_pattern();
    // 2026-11-21 bug round: block caret + keyboard ownership in this modal.
    void the_fields_are_tv_line_edits_with_a_block_caret();
    void modal_shows_own_the_keyboard();

    // Task 5: Find Next across the three scopes.
    void find_next_selects_the_first_match();
    void find_next_wraps_to_the_top();
    void find_next_across_all_documents_switches_tab();
    void the_status_reports_the_match_count();
    void the_status_reports_not_found_and_bad_pattern();
    void return_in_the_find_field_runs_find_next();

    // Task 6: Replace and Replace All.
    void replace_swaps_the_current_match_and_advances();
    void replace_all_reports_the_count();
    void replace_all_across_documents_reports_documents();
    void one_replace_all_is_one_undo_step();
    void an_empty_replacement_deletes_the_matches();
    void the_selection_scope_uses_the_range_captured_at_open();
    void replace_leaves_a_non_matching_selection_alone();
    void return_on_the_dialog_itself_runs_find_next();

    // Reported live (2026-11-19): "the find / replace dialog has a visual
    // glitch in the center where options are not fully visible".
    void the_option_row_is_not_squeezed();
};

void TstFindDlg::initTestCase()
{
    // The APP's environment, or the probes measure a friendlier layout than the
    // real dialog (the tst_settings stepper lesson): the bundled VGA8 face as
    // the app-wide default font, and borland.qss's 16 px sizing.
    QFile ttf(QStringLiteral(":/fonts/PxPlus_IBM_VGA8.ttf"));
    if (ttf.open(QIODevice::ReadOnly)) {
        const int id = QFontDatabase::addApplicationFontFromData(ttf.readAll());
        if (id >= 0) {
            const QStringList fams = QFontDatabase::applicationFontFamilies(id);
            if (!fams.isEmpty()) {
                QFont def = qApp->font();
                def.setFamily(fams.first());
                qApp->setFont(def);
            }
        }
    }
    QFile qss(QStringLiteral(":/borland.qss"));
    QVERIFY(qss.open(QIODevice::ReadOnly | QIODevice::Text));
    qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
}

void TstFindDlg::the_dialog_is_house_style()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    const QImage img = grabWidget(&dlg);
    const QRect f = dlg.frameRect();
    const int cw = dlg.cellWidth();
    const int ch = dlg.cellHeight();
    QVERIFY2(f.width() > 0 && cw > 0 && ch > 0, "the dialog has no frame or no cell metrics");

    // Gray panel between the frame's left column and the first control.
    const QColor panel = img.pixelColor(f.left() + cw + 1, f.center().y());
    QVERIFY2(isPanelGrey(panel),
             qPrintable(QStringLiteral("panel is %1, not the TV grey").arg(rgb(panel))));

    // Black double frame on all four sides (2026-11-26: white trim was
    // illegible with the CRT effects on).
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20, QRect(f.left(), f.top(), f.width(), ch)) > 100,
             "the top row carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20,
                          QRect(f.left(), f.bottom() - ch + 1, f.width(), ch)) > 100,
             "the bottom row carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20, QRect(f.left(), f.top(), cw, f.height())) > 100,
             "the left column carries no black frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::black), 20,
                          QRect(f.right() - cw + 1, f.top(), cw, f.height())) > 100,
             "the right column carries no black frame ink");

    // Hard black shadow down-right, and a clear gutter above it.
    const QColor right = img.pixelColor(f.right() + 3, f.center().y());
    QVERIFY2(isBlack(right), qPrintable(QStringLiteral("right shadow is %1").arg(rgb(right))));
    const QColor bottom = img.pixelColor(f.center().x(), f.bottom() + 3);
    QVERIFY2(isBlack(bottom), qPrintable(QStringLiteral("bottom shadow is %1").arg(rgb(bottom))));
    const QColor gutter = img.pixelColor(f.right() + 3, f.top() + 2);
    QVERIFY2(!isPanelGrey(gutter),
             qPrintable(QStringLiteral("the gutter above the shadow is panel grey (%1)").arg(rgb(gutter))));
}

// 2026-11-26 (user request): the row labels ("Find what:", "Replace with:",
// "Search in:") are BLACK on the grey panel; the option labels ("Case
// sensitive", ...) are editor yellow. Yellow labels on grey were hard to read
// with the CRT effects on, and black options read as ordinary body text.
void TstFindDlg::field_labels_are_black_and_options_are_yellow()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    const QImage img = grabWidget(&dlg);
    const QColor black(0x00, 0x00, 0x00);
    const QColor yellow(0xFF, 0xFF, 0x55);
    auto countIn = [&img](const QRect& r, const QColor& want) {
        int n = 0;
        for (int y = r.top(); y <= r.bottom(); ++y)
            for (int x = r.left(); x <= r.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.red() - want.red()) <= 20 &&
                    qAbs(c.green() - want.green()) <= 20 &&
                    qAbs(c.blue() - want.blue()) <= 20)
                    ++n;
            }
        return n;
    };

    // Every non-empty QLabel in the body is a row label.
    const QList<QLabel*> labels = dlg.findChildren<QLabel*>();
    int checkedLabels = 0;
    for (QLabel* label : labels) {
        if (label->text().isEmpty())
            continue;
        const QRect r(label->mapTo(&dlg, QPoint(0, 0)), label->size());
        const int blackPx = countIn(r, black);
        const int yellowPx = countIn(r, yellow);
        QVERIFY2(blackPx > 8,
                 qPrintable(QStringLiteral("label '%1' has only %2 black px")
                                .arg(label->text()).arg(blackPx)));
        QVERIFY2(yellowPx < 4,
                 qPrintable(QStringLiteral("label '%1' still paints %2 yellow px")
                                .arg(label->text()).arg(yellowPx)));
        ++checkedLabels;
    }
    QVERIFY2(checkedLabels >= 3, "expected at least the three row labels");

    for (TvCheckBox* box : {dlg.caseBox(), dlg.wholeWordsBox(), dlg.regexBox()}) {
        const QRect r(box->mapTo(&dlg, QPoint(0, 0)), box->size());
        const int yellowPx = countIn(r, yellow);
        QVERIFY2(yellowPx > 8,
                 qPrintable(QStringLiteral("option '%1' has only %2 yellow px")
                                .arg(box->text()).arg(yellowPx)));
    }
}

void TstFindDlg::every_control_exists_with_a_usable_geometry()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QVERIFY2(dlg.findField(), "no Find what field");
    QVERIFY2(dlg.replaceField(), "no Replace with field");
    QVERIFY2(dlg.caseBox(), "no Case sensitive box");
    QVERIFY2(dlg.wholeWordsBox(), "no Whole words box");
    QVERIFY2(dlg.regexBox(), "no Regular expression box");
    QVERIFY2(dlg.scopeBar(), "no scope bar");
    QCOMPARE(dlg.scopeBar()->count(), 3);
    QVERIFY2(dlg.findNextButton(), "no Find Next plate");
    QVERIFY2(dlg.replaceButton(), "no Replace plate");
    QVERIFY2(dlg.replaceAllButton(), "no Replace All plate");
    QCOMPARE(dlg.buttonCount(), 4);   // the three action plates + Cancel

    const QRect f = dlg.frameRect();
    QVERIFY2(f.width() > 0, "no frame rect");

    // Fields: big enough to type a pattern in, and inside the frame.
    for (QLineEdit* field : {dlg.findField(), dlg.replaceField()}) {
        const QRect r = inDialog(field, &dlg);
        QVERIFY2(r.width() >= 80 && r.height() >= 14,
                 qPrintable(QStringLiteral("field is only %1x%2").arg(r.width()).arg(r.height())));
        QVERIFY2(f.contains(r), qPrintable(QStringLiteral("field %1x%2 at %3,%4 escapes the frame")
                                               .arg(r.width()).arg(r.height())
                                               .arg(r.left()).arg(r.top())));
    }

    // Checkboxes: all three inside the frame, left to right, not overlapping.
    QWidget* boxes[3] = {dlg.caseBox(), dlg.wholeWordsBox(), dlg.regexBox()};
    QRect boxRects[3];
    for (int i = 0; i < 3; ++i) {
        boxRects[i] = inDialog(boxes[i], &dlg);
        QVERIFY2(f.contains(boxRects[i]),
                 qPrintable(QStringLiteral("checkbox %1 escapes the frame").arg(i)));
        QVERIFY2(boxes[i]->width() > 0 && boxes[i]->height() > 0,
                 qPrintable(QStringLiteral("checkbox %1 has no size").arg(i)));
    }
    QVERIFY2(boxRects[0].right() < boxRects[1].left()
                 && boxRects[1].right() < boxRects[2].left(),
             "the checkboxes are not laid out left to right");

    // Chips: three, inside the frame.
    for (int i = 0; i < 3; ++i) {
        const QRect chip = dlg.scopeBar()->tabRect(i);
        QVERIFY2(!chip.isNull(), qPrintable(QStringLiteral("chip %1 has no rect").arg(i)));
        const QRect mapped = QRect(dlg.scopeBar()->mapTo(&dlg, chip.topLeft()), chip.size());
        QVERIFY2(f.contains(mapped), qPrintable(QStringLiteral("chip %1 escapes the frame").arg(i)));
    }

    // Plates: four, right to left, inside the frame, no overlap.
    QRect prev;
    for (int i = 0; i < dlg.buttonCount(); ++i) {
        const QRect b = dlg.buttonRect(i);
        QVERIFY2(!b.isNull(), qPrintable(QStringLiteral("plate %1 has no rect").arg(i)));
        QVERIFY2(f.contains(b), qPrintable(QStringLiteral("plate %1 escapes the frame").arg(i)));
        if (i > 0)
            QVERIFY2(b.right() < prev.left(),
                     qPrintable(QStringLiteral("plates %1 and %2 overlap").arg(i - 1).arg(i)));
        prev = b;
    }
}

void TstFindDlg::the_fields_are_tv_blue_with_no_border()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    const QImage img = grabWidget(&dlg);
    const QColor blue(0x00, 0x00, 0xAA);

    // The dialog opens with Find what active, so its ring is the focus yellow.
    // The Replace field is at rest and must read as a flat blue box; neither may
    // carry a white border.
    QLineEdit* fields[2] = {dlg.findField(), dlg.replaceField()};
    for (int i = 0; i < 2; ++i) {
        QLineEdit* field = fields[i];
        // The field's cell: the QLineEdit sits inset inside it, and the cell's
        // own 2 px ring is what a border would show up on.
        QWidget* cell = field->parentWidget();
        QVERIFY2(cell, "the field has no cell");
        const QRect r = inDialog(cell, &dlg).adjusted(0, 0, -1, -1);
        QVERIFY2(r.width() > 4 && r.height() > 4, "the field cell has no size");

        const int ring = ringPixels(r.adjusted(0, 0, 1, 1));
        int white = 0;
        int bluePx = 0;
        for (int x = r.left(); x <= r.right(); ++x) {
            for (int y : {r.top(), r.bottom()}) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() > 200 && c.blue() > 200) ++white;
                if (qAbs(c.blue() - blue.blue()) <= 40 && c.red() < 40 && c.green() < 40) ++bluePx;
            }
        }
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x : {r.left(), r.right()}) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() > 200 && c.blue() > 200) ++white;
                if (qAbs(c.blue() - blue.blue()) <= 40 && c.red() < 40 && c.green() < 40) ++bluePx;
            }
        }
        QVERIFY2(white * 10 < ring,
                 qPrintable(QStringLiteral("the field border is white (%1 of %2 ring px)")
                                .arg(white).arg(ring)));
        if (i == 1) {
            QVERIFY2(bluePx * 2 > ring,
                     qPrintable(QStringLiteral("the resting field ring is not TV blue (%1 of %2 px)")
                                    .arg(bluePx).arg(ring)));
        }
    }
}

void TstFindDlg::clicking_a_field_marks_it_active()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));
    dlg.findField()->setText(QStringLiteral("needle"));

    QTest::mouseClick(dlg.findField(), Qt::LeftButton);

    QVERIFY2(ringYellow(grabWidget(&dlg), dlg.findField(), &dlg) > 100,
             "clicking the field painted no yellow ring");
    // ...and the content is selected, the visible proof the click landed.
    QCOMPARE(dlg.findField()->selectedText(), QStringLiteral("needle"));
}

void TstFindDlg::the_status_row_starts_blank()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QVERIFY2(dlg.statusText().isEmpty(),
             qPrintable(QStringLiteral("the status row opens saying '%1'").arg(dlg.statusText())));
    QVERIFY2(dlg.findChild<QLabel*>(QStringLiteral("findStatus")),
             "the dialog has no status label");
}

// KTextEditor's whole-word matching is plaintext-only, so a regex and the
// Whole words box cannot both be in force. Ticking Regex greys the box out and
// leaves its state alone for when Regex is switched back off.
void TstFindDlg::ticking_regex_disables_whole_words()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QVERIFY2(dlg.wholeWordsBox()->isEnabled(), "Whole words starts disabled");
    dlg.wholeWordsBox()->setChecked(true);

    dlg.regexBox()->setChecked(true);
    QVERIFY2(!dlg.wholeWordsBox()->isEnabled(),
             "ticking Regular expression left Whole words usable");
    QVERIFY2(!dlg.options().wholeWords,
             "a regex search still reports wholeWords in its options");
    QVERIFY2(dlg.options().regex, "options() does not report the regex box");

    dlg.regexBox()->setChecked(false);
    QVERIFY2(dlg.wholeWordsBox()->isEnabled(), "unticking Regex left Whole words disabled");
    QVERIFY2(dlg.wholeWordsBox()->isChecked(), "the box lost its state on the round trip");
    QVERIFY(dlg.options().wholeWords);

    dlg.caseBox()->setChecked(true);
    QVERIFY(dlg.options().caseSensitive);
}

// The Selection scope is only offered when the active view HAS a selection at
// the moment the dialog opens.
void TstFindDlg::the_selection_chip_needs_a_selection()
{
    EditorTabManager tabs;   // no document, so no selection
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QVERIFY2(!dlg.scopeBar()->isTabEnabled(2),
             "the Selection chip is offered with nothing selected");
    QCOMPARE(dlg.scope(), FindReplaceDialog::Scope::CurrentDocument);

    dlg.setScope(FindReplaceDialog::Scope::Selection);
    QVERIFY2(dlg.scope() == FindReplaceDialog::Scope::CurrentDocument,
             "setScope(Selection) with no selection did not fall back");
}

// An empty pattern has nothing to look for: the three action plates go dead
// rather than reporting "Not found" on every click.
void TstFindDlg::an_empty_pattern_disables_the_action_buttons()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QString());
    QVERIFY2(!dlg.findNextButton()->isEnabled(), "Find Next is live with an empty pattern");
    QVERIFY2(!dlg.replaceButton()->isEnabled(), "Replace is live with an empty pattern");
    QVERIFY2(!dlg.replaceAllButton()->isEnabled(), "Replace All is live with an empty pattern");

    dlg.findField()->setText(QStringLiteral("a"));
    QVERIFY2(dlg.findNextButton()->isEnabled(), "Find Next stayed dead after typing a pattern");
    QVERIFY2(dlg.replaceButton()->isEnabled(), "Replace stayed dead after typing a pattern");
    QVERIFY2(dlg.replaceAllButton()->isEnabled(), "Replace All stayed dead after typing a pattern");
}

// Ctrl+F lands in the Find what field, pre-filled with the last pattern and
// selected, so typing replaces it. Ctrl+H lands in Replace with instead.
void TstFindDlg::the_dialog_opens_on_the_find_field_with_the_last_pattern()
{

    EditorTabManager tabs;
    {
        FindReplaceDialog first(&tabs);
        first.findField()->setText(QStringLiteral("remembered"));
    }

    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QCOMPARE(dlg.findField()->text(), QStringLiteral("remembered"));
    QCOMPARE(dlg.findField()->selectedText(), QStringLiteral("remembered"));
    const QImage opened = grabWidget(&dlg);
    const int findRing = ringYellow(opened, dlg.findField(), &dlg);
    const int replaceRing = ringYellow(opened, dlg.replaceField(), &dlg);
    QVERIFY2(findRing > 100, "the dialog did not open on the Find what field");
    QVERIFY2(findRing > 3 * replaceRing,
             qPrintable(QStringLiteral("the Replace with field opened marked as active too "
                                       "(%1 vs %2 ring px)")
                            .arg(replaceRing).arg(findRing)));

    dlg.setInitialField(1);
    const QImage after = grabWidget(&dlg);
    const int findAfter = ringYellow(after, dlg.findField(), &dlg);
    const int replaceAfter = ringYellow(after, dlg.replaceField(), &dlg);
    QVERIFY2(replaceAfter > 100, "setInitialField(1) did not move to the Replace with field");
    QVERIFY2(replaceAfter > 3 * findAfter,
             qPrintable(QStringLiteral("setInitialField(1) left the Find what field marked "
                                       "active (%1 vs %2 ring px)")
                            .arg(findAfter).arg(replaceAfter)));
}

// Reported 2026-11-21: "none of the input fields on these dialogs shows the
// mouse cursor position". The Find/Replace fields are plain QLineEdits, and
// Qt paints no caret in a never-active window - the exact reason TvLineEdit
// exists (see its header). Both fields must be TvLineEdits that paint the
// DOS block caret when they hold key focus.
void TstFindDlg::the_fields_are_tv_line_edits_with_a_block_caret()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    auto* find = qobject_cast<TvLineEdit*>(dlg.findField());
    QVERIFY2(find, "the find field is not a TvLineEdit");
    auto* repl = qobject_cast<TvLineEdit*>(dlg.replaceField());
    QVERIFY2(repl, "the replace field is not a TvLineEdit");

    // Unfocused: no caret cell (nothing flickers behind an inactive field).
    QVERIFY(find->caretRect().isEmpty());

    // Clicked: focus follows, and the caret cell exists inside the field.
    const QPoint p = find->mapTo(find->window(), find->rect().center());
    QMouseEvent press(QEvent::MouseButtonPress, p, p, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(find, &press);
    QCoreApplication::processEvents();
    QVERIFY2(find->holdsKeyFocus(), "clicking did not give the find field key focus");
    const QRect caret = find->caretRect();
    QVERIFY2(!caret.isEmpty(), "focused find field reports no caret cell");
    QVERIFY2(find->rect().contains(caret.topLeft()),
             "caret cell lies outside the find field");

    // ...and it is actually INK (blink off for a deterministic probe).
    find->setCaretBlinkEnabled(false);
    find->setCaretColors(QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0xAA));
    const QImage img = find->grab().toImage();
    int white = 0;
    for (int y = caret.top(); y <= caret.bottom() && y < img.height(); ++y)
        for (int x = caret.left(); x <= caret.right() && x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 240 && c.green() > 240 && c.blue() > 240)
                ++white;
        }
    QVERIFY2(white >= 4,
             qPrintable(QStringLiteral("no block caret painted (%1 white px in the cell)")
                            .arg(white)));
}

// Reported 2026-11-21: "backspacing ... also backspaces in the editor" on
// Find/Replace. The modal must claim the keyboard while shown (the file dock
// already does - same contract), so ContentBridge stops offering the keystroke
// to the editor behind it, and must release it on close.
void TstFindDlg::modal_shows_own_the_keyboard()
{
    ShellWindow shell;
    shell.resize(900, 700);
    EditorTabManager tabs;
    shell.setEditorWidget(tabs.containerWidget());
    shell.show();
    ContentBridge bridge;
    bridge.setShellWindow(&shell);
    QCoreApplication::processEvents();
    tabs.containerWidget()->grab();

    KTextEditor::Document* doc = tabs.activeDocument();
    QVERIFY(doc);
    doc->setText(QStringLiteral("alpha beta"));
    QVERIFY(tabs.activeView());
    tabs.activeView()->setCursorPosition(KTextEditor::Cursor(0, 10));

    int editorShortcutCalls = 0;
    shell.setEditorShortcutHandler([&](const QKeySequence& seq) {
        ++editorShortcutCalls;
        return tabs.triggerActiveViewShortcut(seq);
    });

    auto* dlg = new FindReplaceDialog(&tabs, &shell);
    dlg->setGeometry(60, 60, dlg->sizeHint().width(), dlg->sizeHint().height());
    dlg->show();
    QCoreApplication::processEvents();
    QVERIFY2(shell.keyboardIsOwned(),
             "showing Find/Replace did not claim the keyboard for the modal");

    QLineEdit* field = dlg->findField();
    QVERIFY(field);
    const QPoint c = field->mapTo(&shell, field->rect().center());
    bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                             Qt::NoModifier, c.x(), c.y());
    bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton,
                             Qt::NoModifier, c.x(), c.y());
    QCoreApplication::processEvents();
    field->setText(QStringLiteral("needle"));

    const QString before = field->text();
    const int docBefore = int(doc->text().size());
    bridge.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier,
                           QStringLiteral("\b"));
    QCoreApplication::processEvents();

    QVERIFY2(field->text() == before.chopped(1),
             qPrintable(QStringLiteral("backspace did not edit the field (text=%1)")
                            .arg(field->text())));
    QVERIFY2(int(doc->text().size()) == docBefore,
             qPrintable(QStringLiteral("backspace also reached the editor (doc %1 -> %2)")
                            .arg(docBefore).arg(doc->text().size())));
    QCOMPARE(editorShortcutCalls, 0);

    dlg->reject();
    QCoreApplication::processEvents();
    QVERIFY2(!shell.keyboardIsOwned(), "closing Find/Replace left the keyboard claimed");
}

// ---- Task 5: Find Next -------------------------------------------------

namespace {

// A manager with exactly one document holding `text`.
void seedDocument(EditorTabManager& tabs, const QString& text)
{
    if (tabs.tabCount() == 0)
        tabs.newDocument();   // the manager starts with one untitled doc anyway
    tabs.activeDocument()->setText(text);
}

QString selectedText(EditorTabManager& tabs)
{
    KTextEditor::View* view = tabs.activeView();
    if (!view || !view->selection())
        return QString();
    return tabs.activeDocument()->text(view->selectionRange());
}

} // namespace

void TstFindDlg::find_next_selects_the_first_match()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    dlg.findNext();

    QCOMPARE(selectedText(tabs), QStringLiteral("one"));
    QCOMPARE(tabs.activeView()->cursorPosition().line(), 0);
    QCOMPARE(tabs.activeView()->cursorPosition().column(), 0);
}

void TstFindDlg::find_next_wraps_to_the_top()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    dlg.findNext();
    dlg.findNext();
    QCOMPARE(tabs.activeView()->cursorPosition().column(), 8);   // the second one

    dlg.findNext();                                              // wraps
    QCOMPARE(tabs.activeView()->cursorPosition().column(), 0);
    QCOMPARE(selectedText(tabs), QStringLiteral("one"));
}

void TstFindDlg::find_next_across_all_documents_switches_tab()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("alpha"));
    tabs.newDocument();
    tabs.activeDocument()->setText(QStringLiteral("beta needle"));
    QCOMPARE(tabs.tabCount(), 2);
    tabs.setActiveIndex(0);

    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.setScope(FindReplaceDialog::Scope::AllDocuments);
    dlg.findField()->setText(QStringLiteral("needle"));
    dlg.findNext();

    QCOMPARE(tabs.activeIndex(), 1);
    QCOMPARE(selectedText(tabs), QStringLiteral("needle"));
}

void TstFindDlg::the_status_reports_the_match_count()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    dlg.findNext();
    QCOMPARE(dlg.statusText(), QStringLiteral("2 matches"));

    dlg.findField()->setText(QStringLiteral("two"));
    dlg.findNext();
    QCOMPARE(dlg.statusText(), QStringLiteral("1 match"));
}

void TstFindDlg::the_status_reports_not_found_and_bad_pattern()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("zzz"));
    dlg.findNext();
    QCOMPARE(dlg.statusText(), QStringLiteral("Not found"));

    dlg.regexBox()->setChecked(true);
    dlg.findField()->setText(QStringLiteral("["));
    dlg.findNext();
    QCOMPARE(dlg.statusText(), QStringLiteral("Bad pattern"));
}

void TstFindDlg::return_in_the_find_field_runs_find_next()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    QTest::keyClick(dlg.findField(), Qt::Key_Return);
    QCOMPARE(selectedText(tabs), QStringLiteral("one"));
    QCOMPARE(tabs.activeView()->cursorPosition().column(), 0);

    QTest::keyClick(dlg.findField(), Qt::Key_Return);
    QCOMPARE(tabs.activeView()->cursorPosition().column(), 8);
    QVERIFY2(dlg.isVisible(), "Return closed the dialog - TV behaviour is to stay open");
}

// ---- Task 6: Replace and Replace All ------------------------------------

void TstFindDlg::replace_swaps_the_current_match_and_advances()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    dlg.replaceField()->setText(QStringLiteral("1"));
    dlg.findNext();
    dlg.replaceOne();

    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("1 two one"));
    QCOMPARE(selectedText(tabs), QStringLiteral("one"));   // the next hit is selected
}

void TstFindDlg::replace_all_reports_the_count()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("a a a"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("a"));
    dlg.replaceField()->setText(QStringLiteral("b"));
    dlg.replaceAll();

    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("b b b"));
    QCOMPARE(dlg.statusText(), QStringLiteral("Replaced 3"));
}

void TstFindDlg::replace_all_across_documents_reports_documents()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("a a"));
    tabs.newDocument();
    tabs.activeDocument()->setText(QStringLiteral("a"));
    tabs.setActiveIndex(0);

    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.setScope(FindReplaceDialog::Scope::AllDocuments);
    dlg.findField()->setText(QStringLiteral("a"));
    dlg.replaceField()->setText(QStringLiteral("b"));
    dlg.replaceAll();

    QCOMPARE(tabs.documents().at(0)->text(), QStringLiteral("b b"));
    QCOMPARE(tabs.documents().at(1)->text(), QStringLiteral("b"));
    QCOMPARE(dlg.statusText(), QStringLiteral("Replaced 3 in 2 documents"));
}

// This KTextEditor build has no undo API, so the manager keeps its own snapshot
// stack: one replace-all must be ONE snapshot, or the user cannot undo it.
void TstFindDlg::one_replace_all_is_one_undo_step()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("a a a"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("a"));
    dlg.replaceField()->setText(QStringLiteral("b"));
    dlg.replaceAll();
    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("b b b"));

    tabs.undo();
    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("a a a"));
}

void TstFindDlg::an_empty_replacement_deletes_the_matches()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("a-b-a"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("a"));
    dlg.replaceField()->setText(QString());
    dlg.replaceAll();

    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("-b-"));
    QCOMPARE(dlg.statusText(), QStringLiteral("Replaced 2"));
}

// Moved from Task 4: it needs replaceAll() to exist. The Selection scope must
// search the range captured when the dialog opened, NOT the selection Find Next
// just made - otherwise picking Selection after a Find Next silently searches
// the hit.
void TstFindDlg::the_selection_scope_uses_the_range_captured_at_open()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("abc abc"));
    KTextEditor::View* view = tabs.activeView();
    view->setSelection(KTextEditor::Range(0, 0, 0, 3));   // first "abc"

    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));
    QVERIFY2(dlg.scopeBar()->isTabEnabled(2),
             "a selection at open did not enable the Selection chip");

    dlg.setScope(FindReplaceDialog::Scope::Selection);
    dlg.findField()->setText(QStringLiteral("abc"));
    dlg.replaceField()->setText(QStringLiteral("X"));
    dlg.findNext();   // selects the hit inside the captured range...
    dlg.replaceAll(); // ...which must still be the FIRST abc

    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("X abc"));
}

// Replace must not destroy text the user merely had selected: if the selection
// is not a match of the pattern, Replace behaves like Find Next instead.
void TstFindDlg::replace_leaves_a_non_matching_selection_alone()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("hello world"));
    tabs.activeView()->setSelection(KTextEditor::Range(0, 0, 0, 5));   // "hello"

    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("world"));
    dlg.replaceField()->setText(QStringLiteral("X"));
    dlg.replaceOne();

    QCOMPARE(tabs.activeDocument()->text(), QStringLiteral("hello world"));
    QCOMPARE(selectedText(tabs), QStringLiteral("world"));   // it found the real hit
}

// Return is the repeat gesture wherever the dialog has focus - it must never
// fall through to TvModal's Return-means-accept and close the dialog.
void TstFindDlg::return_on_the_dialog_itself_runs_find_next()
{
    EditorTabManager tabs;
    seedDocument(tabs, QStringLiteral("one two one"));
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    dlg.findField()->setText(QStringLiteral("one"));
    dlg.setFocus();
    QTest::keyClick(&dlg, Qt::Key_Return);

    QVERIFY2(dlg.isVisible(), "Return on the dialog closed it");
    QCOMPARE(selectedText(tabs), QStringLiteral("one"));
}

// Reported live (2026-11-19): the three option boxes came out 4 px tall - their
// text clipped to a sliver - because the grid had 16 px less height than it
// needed and dumped the whole deficit on the one row that can collapse to zero.
// The dialog must be laid out at the size exec() would give it (the FIRST
// sizeHint(), before anything else asks for one) and every option must then
// stand at its full height, inside the body.
void TstFindDlg::the_option_row_is_not_squeezed()
{
    EditorTabManager tabs;
    FindReplaceDialog dlg(&tabs);
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    QWidget* body = dlg.findChild<QWidget*>(QStringLiteral("findBody"));
    QVERIFY2(body, "no body widget");

    int i = 0;
    for (TvCheckBox* box : {dlg.caseBox(), dlg.wholeWordsBox(), dlg.regexBox()}) {
        const QSize need = box->sizeHint();
        const QRect r(box->mapTo(body, QPoint(0, 0)), box->size());
        QVERIFY2(r.height() >= need.height(),
                 qPrintable(QStringLiteral("option %1 is %2 px tall, needs %3 - the row was squeezed")
                                .arg(i).arg(r.height()).arg(need.height())));
        QVERIFY2(r.width() >= need.width(),
                 qPrintable(QStringLiteral("option %1 is %2 px wide, needs %3 - the row was squeezed")
                                .arg(i).arg(r.width()).arg(need.width())));
        QVERIFY2(body->rect().contains(r),
                 qPrintable(QStringLiteral("option %1 at %2,%3 %4x%5 escapes the body")
                                .arg(i).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())));
        ++i;
    }

    // The rows below it must keep their own height too: the deficit was taken
    // out of the option row, but a short dialog would next eat the status line.
    QVERIFY2(dlg.scopeBar()->height() >= dlg.scopeBar()->sizeHint().height(),
             "the scope chips were squeezed");
    QLabel* status = dlg.findChild<QLabel*>(QStringLiteral("findStatus"));
    QVERIFY2(status, "no status label");
    QVERIFY2(status->height() >= status->sizeHint().height(), "the status line was squeezed");
}

QTEST_MAIN(TstFindDlg)
#include "tst_finddlg.moc"
