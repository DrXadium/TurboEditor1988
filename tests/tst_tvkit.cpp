// SPDX-License-Identifier: GPL-3.0-or-later
// The TV widget kit (2026-11-18): the controls a house-style modal needs, drawn
// the way Turbo Vision drew them rather than by the style engine.
//
// TDD: written before the widgets exist.
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QWidget>
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvSpinBox.h"
#include "src/shell/TvTabBar.h"
#include <QLineEdit>

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

QImage grab(QWidget& w)
{
    w.show();
    if (!QTest::qWaitForWindowExposed(&w))
        return QImage();
    return w.grab().toImage().convertToFormat(QImage::Format_RGB32);
}

bool isBlack(const QColor& c)
{
    return c.red() < 40 && c.green() < 40 && c.blue() < 40;
}

// The checkbox lives on the modal's gray panel, so the probes must too: a bare
// widget grab has a BLACK background (nothing fills it), which would count as
// bracket ink and make every probe here lie.
QImage grabOnGreyPanel(TvCheckBox& cb, QWidget& panel)
{
    panel.setAutoFillBackground(true);
    QPalette pal = panel.palette();
    pal.setColor(QPalette::Window, QColor(0xAA, 0xAA, 0xAA));
    panel.setPalette(pal);
    cb.setParent(&panel);
    cb.move(0, 0);
    cb.resize(cb.sizeHint());
    panel.resize(cb.sizeHint());
    panel.show();
    if (!QTest::qWaitForWindowExposed(&panel))
        return QImage();
    return panel.grab().toImage().convertToFormat(QImage::Format_RGB32);
}

} // namespace

class TstTvKit : public QObject
{
    Q_OBJECT

private slots:
    // TvCheckBox
    void the_box_is_empty_when_unchecked();
    void the_box_shows_a_green_square_when_checked();
    void the_brackets_are_black();
    void clicking_anywhere_in_the_row_toggles();
    void space_toggles();
    void toggled_fires_only_on_a_real_change();

    // TvSpinBox
    void the_value_is_shown_in_the_field();
    void the_suffix_is_appended();
    void step_up_and_down_move_by_the_step();
    void stepping_stops_at_the_range_ends();
    void typed_out_of_range_clamps_on_commit();
    void unparsable_text_snaps_back();
    void clicking_the_up_cell_steps_up();
    void valueChanged_fires_only_on_a_real_change();
    void the_stepper_cells_have_no_black_outline();

    // TvTabBar
    void chips_are_laid_out_left_to_right();
    void the_active_chip_is_grey_and_the_others_blue();
    void clicking_a_chip_switches_to_it();
    void currentChanged_fires_only_on_a_real_change();
};

// The bracket is white too, so counting the whole interior measures the frame.
// The CROSS is probed at the box centre, where the two diagonals meet and the
// bracket never reaches.
namespace {
QRect boxCentre(const QRect& box)
{
    return QRect(box.center() - QPoint(1, 1), QSize(3, 3));
}
} // namespace

// The bracket is the ink of the control, so "empty" means the interior carries
// no fill at all - the centre is where the filled square sits when checked.
void TstTvKit::the_box_is_empty_when_unchecked()
{
    QWidget panel;
    TvCheckBox cb(QStringLiteral("Scanlines"));
    const QImage img = grabOnGreyPanel(cb, panel);
    QVERIFY2(!img.isNull(), "the checkbox never showed");

    const QColor green(0x00, 0xAA, 0x00);
    const int fill = countColorIn(img, green, 40, cb.boxRect());
    QVERIFY2(fill == 0,
             qPrintable(QStringLiteral("an unchecked box carries %1 green px").arg(fill)));
    const int centre = countColorIn(img, QColor(Qt::black), 40, boxCentre(cb.boxRect()));
    QVERIFY2(centre == 0, "an unchecked box still draws ink at its centre");
    QVERIFY2(cb.boxRect().width() > 6 && cb.boxRect().height() > 6,
             "the box has no room for a square");
}

// Reported: the control reads [ ] empty and [green square] checked - the
// brackets are black, the square inside them is green.
void TstTvKit::the_box_shows_a_green_square_when_checked()
{
    QWidget panel;
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.setChecked(true);
    const QImage img = grabOnGreyPanel(cb, panel);
    QVERIFY2(!img.isNull(), "the checkbox never showed");

    const QColor green(0x00, 0xAA, 0x00);
    const int fill = countColorIn(img, green, 40, cb.boxRect());
    QVERIFY2(fill > 8,
             qPrintable(QStringLiteral("a checked box carries only %1 green px").arg(fill)));
    const int centre = countColorIn(img, green, 40, boxCentre(cb.boxRect()));
    QVERIFY2(centre > 0, "the green square does not cover the box centre");
}

void TstTvKit::the_brackets_are_black()
{
    for (bool checked : {false, true}) {
        QWidget panel;
        TvCheckBox cb(QStringLiteral("Scanlines"));
        cb.setChecked(checked);
        const QImage img = grabOnGreyPanel(cb, panel);
        QVERIFY2(!img.isNull(), "the checkbox never showed");
        const int black = countColorIn(img, QColor(Qt::black), 40, cb.boxRect());
        QVERIFY2(black > 10,
                 qPrintable(QStringLiteral("a %1 box carries only %2 black px - the brackets "
                                           "are not black")
                                .arg(checked ? QStringLiteral("checked") : QStringLiteral("empty"))
                                .arg(black)));
    }
}

void TstTvKit::clicking_anywhere_in_the_row_toggles()
{
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.resize(cb.sizeHint());
    cb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&cb));

    QSignalSpy spy(&cb, &TvCheckBox::toggled);
    // Far right of the row: the label, not the box.
    QTest::mouseClick(&cb, Qt::LeftButton, Qt::NoModifier,
                      QPoint(cb.width() - 5, cb.height() / 2));
    QVERIFY2(cb.isChecked(), "clicking the label did not toggle");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void TstTvKit::space_toggles()
{
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.resize(cb.sizeHint());
    cb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&cb));
    cb.setFocus();

    QSignalSpy spy(&cb, &TvCheckBox::toggled);
    QTest::keyClick(&cb, Qt::Key_Space);
    QVERIFY2(cb.isChecked(), "Space did not check the box");
    QTest::keyClick(&cb, Qt::Key_Space);
    QVERIFY2(!cb.isChecked(), "Space did not uncheck the box");
    QCOMPARE(spy.count(), 2);
}

void TstTvKit::toggled_fires_only_on_a_real_change()
{
    TvCheckBox cb(QStringLiteral("Scanlines"));
    QSignalSpy spy(&cb, &TvCheckBox::toggled);
    cb.setChecked(true);
    cb.setChecked(true);
    cb.setChecked(false);
    cb.setChecked(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

// --- TvSpinBox ---------------------------------------------------------------

void TstTvKit::the_value_is_shown_in_the_field()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setValue(0.5);
    QVERIFY2(sb.editor(), "the spin box has no field");
    QCOMPARE(sb.editor()->text(), QStringLiteral("0.50"));
}

void TstTvKit::the_suffix_is_appended()
{
    TvSpinBox sb;
    sb.setRange(1, 3600);
    sb.setDecimals(0);
    sb.setSuffix(QStringLiteral(" s"));
    sb.setValue(120);
    QCOMPARE(sb.editor()->text(), QStringLiteral("120 s"));
    QCOMPARE(sb.suffix(), QStringLiteral(" s"));
}

void TstTvKit::step_up_and_down_move_by_the_step()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setSingleStep(0.05);
    sb.setValue(0.5);
    sb.stepUp();
    QCOMPARE(sb.value(), 0.55);
    sb.stepDown();
    sb.stepDown();
    QCOMPARE(sb.value(), 0.45);
}

void TstTvKit::stepping_stops_at_the_range_ends()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setSingleStep(0.5);
    sb.setValue(0.9);
    sb.stepUp();
    QCOMPARE(sb.value(), 1.0);
    sb.stepUp();
    QCOMPARE(sb.value(), 1.0);      // no overshoot, no wrap
    sb.setValue(0.1);
    sb.stepDown();
    QCOMPARE(sb.value(), 0.0);
    sb.stepDown();
    QCOMPARE(sb.value(), 0.0);
}

// Review Focus 1: a typed value outside the range must clamp on commit and be
// re-rendered, not accepted silently.
void TstTvKit::typed_out_of_range_clamps_on_commit()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setValue(0.5);
    QSignalSpy spy(&sb, &TvSpinBox::valueChanged);

    sb.editor()->setText(QStringLiteral("99"));
    sb.commitTyped();

    QCOMPARE(sb.value(), 1.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 1.0);
    QCOMPARE(sb.editor()->text(), QStringLiteral("1.00"));
}

void TstTvKit::unparsable_text_snaps_back()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setValue(0.5);
    QSignalSpy spy(&sb, &TvSpinBox::valueChanged);

    sb.editor()->setText(QStringLiteral("abc"));
    sb.commitTyped();

    QCOMPARE(sb.value(), 0.5);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(sb.editor()->text(), QStringLiteral("0.50"));
}

void TstTvKit::clicking_the_up_cell_steps_up()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setSingleStep(0.1);
    sb.setValue(0.5);
    sb.resize(sb.sizeHint());
    sb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sb));

    QVERIFY2(!sb.upRect().isNull() && !sb.downRect().isNull(), "the stepper cells were never laid out");
    QTest::mouseClick(&sb, Qt::LeftButton, Qt::NoModifier, sb.upRect().center());
    QCOMPARE(sb.value(), 0.6);
    QTest::mouseClick(&sb, Qt::LeftButton, Qt::NoModifier, sb.downRect().center());
    QCOMPARE(sb.value(), 0.5);
}

void TstTvKit::valueChanged_fires_only_on_a_real_change()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    QSignalSpy spy(&sb, &TvSpinBox::valueChanged);
    sb.setValue(0.5);
    sb.setValue(0.5);
    QCOMPARE(spy.count(), 1);
    sb.setValue(0.6);
    QCOMPARE(spy.count(), 2);
}

// --- TvTabBar ----------------------------------------------------------------

void TstTvKit::chips_are_laid_out_left_to_right()
{
    TvTabBar tabs;
    tabs.addTab(QStringLiteral("CRT"));
    tabs.addTab(QStringLiteral("Editor"));
    QCOMPARE(tabs.count(), 2);
    tabs.resize(tabs.sizeHint());

    const QRect a = tabs.tabRect(0);
    const QRect b = tabs.tabRect(1);
    QVERIFY2(!a.isNull() && !b.isNull(), "a chip was never laid out");
    QVERIFY2(a.left() < b.left(), "chips are not left to right");
    QVERIFY2(a.right() < b.left(), "the chips overlap");
}

void TstTvKit::the_active_chip_is_grey_and_the_others_blue()
{
    TvTabBar tabs;
    tabs.addTab(QStringLiteral("CRT"));
    tabs.addTab(QStringLiteral("Editor"));
    tabs.setCurrentIndex(0);
    tabs.resize(tabs.sizeHint());
    const QImage img = grab(tabs);
    QVERIFY2(!img.isNull(), "the tab bar never showed");

    // Near each chip's top-left, clear of the centred label.
    const QColor active = img.pixelColor(tabs.tabRect(0).left() + 4, tabs.tabRect(0).top() + 3);
    const QColor inactive = img.pixelColor(tabs.tabRect(1).left() + 4, tabs.tabRect(1).top() + 3);
    QVERIFY2(qAbs(active.red() - 0xAA) < 16 && qAbs(active.green() - 0xAA) < 16
                 && qAbs(active.blue() - 0xAA) < 16,
             qPrintable(QStringLiteral("the active chip is %1,%2,%3, not panel grey")
                            .arg(active.red()).arg(active.green()).arg(active.blue())));
    QVERIFY2(inactive.blue() > 0x88 && inactive.red() < 0x30,
             qPrintable(QStringLiteral("the inactive chip is %1,%2,%3, not blue")
                            .arg(inactive.red()).arg(inactive.green()).arg(inactive.blue())));
}

void TstTvKit::clicking_a_chip_switches_to_it()
{
    TvTabBar tabs;
    tabs.addTab(QStringLiteral("CRT"));
    tabs.addTab(QStringLiteral("Editor"));
    tabs.resize(tabs.sizeHint());
    tabs.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabs));
    QCOMPARE(tabs.currentIndex(), 0);

    QSignalSpy spy(&tabs, &TvTabBar::currentChanged);
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.tabRect(1).center());
    QCOMPARE(tabs.currentIndex(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);
}

void TstTvKit::currentChanged_fires_only_on_a_real_change()
{
    TvTabBar tabs;
    tabs.addTab(QStringLiteral("CRT"));
    tabs.addTab(QStringLiteral("Editor"));
    QSignalSpy spy(&tabs, &TvTabBar::currentChanged);
    tabs.setCurrentIndex(0);        // already 0 after the first add
    QCOMPARE(spy.count(), 0);
    tabs.setCurrentIndex(1);
    QCOMPARE(spy.count(), 1);
    tabs.setCurrentIndex(1);
    QCOMPARE(spy.count(), 1);
    tabs.setCurrentIndex(99);       // out of range: ignored, no signal
    QCOMPARE(spy.count(), 1);
    QCOMPARE(tabs.currentIndex(), 1);
}

// Reported live: the green plates have black outlines they should not have.
// Same rule for the stepper cells - the triangles are the ink, the cell is a
// flat green field.
void TstTvKit::the_stepper_cells_have_no_black_outline()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setValue(0.5);
    sb.resize(sb.sizeHint());
    const QImage img = grab(sb);
    QVERIFY2(!img.isNull(), "the spin box never showed");

    const QRect cells[] = {sb.upRect(), sb.downRect()};
    for (const QRect& c : cells) {
        QVERIFY(!c.isNull());
        // The four edge midpoints, clear of the inset triangle.
        const QPoint probes[] = {
            QPoint(c.center().x(), c.top()),
            QPoint(c.center().x(), c.bottom()),
            QPoint(c.left(), c.center().y()),
            QPoint(c.right(), c.center().y()),
        };
        for (const QPoint& pt : probes) {
            const QColor px = img.pixelColor(pt);
            QVERIFY2(!isBlack(px),
                     qPrintable(QStringLiteral("cell edge at %1,%2 is black (%3,%4,%5)")
                                    .arg(pt.x()).arg(pt.y())
                                    .arg(px.red()).arg(px.green()).arg(px.blue())));
        }
    }
}

QTEST_MAIN(TstTvKit)
#include "tst_tvkit.moc"
