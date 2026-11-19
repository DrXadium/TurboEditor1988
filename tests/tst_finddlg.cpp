// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3 (2026-11-18 Find and Replace): the dialog's chrome and controls.
//
// The dialog must wear the house style - the same grey panel, white double
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

#include "src/editor/FindReplaceDialog.h"
#include "src/editor/EditorTabManager.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvTabBar.h"

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

} // namespace

class TstFindDlg : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void the_dialog_is_house_style();
    void every_control_exists_with_a_usable_geometry();
    void the_fields_are_tv_blue_with_no_border();
    void clicking_a_field_marks_it_active();
    void the_status_row_starts_blank();
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

    // White double frame on all four sides.
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20, QRect(f.left(), f.top(), f.width(), ch)) > 100,
             "the top row carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                          QRect(f.left(), f.bottom() - ch + 1, f.width(), ch)) > 100,
             "the bottom row carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20, QRect(f.left(), f.top(), cw, f.height())) > 100,
             "the left column carries no white frame ink");
    QVERIFY2(countColorIn(img, QColor(Qt::white), 20,
                          QRect(f.right() - cw + 1, f.top(), cw, f.height())) > 100,
             "the right column carries no white frame ink");

    // Hard black shadow down-right, and a clear gutter above it.
    const QColor right = img.pixelColor(f.right() + 3, f.center().y());
    QVERIFY2(isBlack(right), qPrintable(QStringLiteral("right shadow is %1").arg(rgb(right))));
    const QColor bottom = img.pixelColor(f.center().x(), f.bottom() + 3);
    QVERIFY2(isBlack(bottom), qPrintable(QStringLiteral("bottom shadow is %1").arg(rgb(bottom))));
    const QColor gutter = img.pixelColor(f.right() + 3, f.top() + 2);
    QVERIFY2(!isPanelGrey(gutter),
             qPrintable(QStringLiteral("the gutter above the shadow is panel grey (%1)").arg(rgb(gutter))));
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

    const QImage img = grabWidget(&dlg);
    QWidget* cell = dlg.findField()->parentWidget();
    const QRect r = inDialog(cell, &dlg).adjusted(0, 0, -1, -1);
    // The 2 px ring around the field is the "you are typing here" mark.
    const int yellow = countColorIn(img, QColor(0xFF, 0xFF, 0x55), 30, r)
                       - countColorIn(img, QColor(0xFF, 0xFF, 0x55), 30, r.adjusted(2, 2, -2, -2));
    QVERIFY2(yellow > 20,
             qPrintable(QStringLiteral("clicking the field painted no yellow ring (%1 px)").arg(yellow)));
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

QTEST_MAIN(TstFindDlg)
#include "tst_finddlg.moc"
