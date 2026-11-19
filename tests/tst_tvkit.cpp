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
    void the_stepper_cells_draw_their_arrows();
    void pressing_a_stepper_highlights_it_and_release_reverts_without_a_shadow();
    void the_spin_field_has_no_white_border();

    // TvTabBar
    void chips_are_laid_out_left_to_right();
    void the_active_chip_is_grey_and_the_others_blue();
    void clicking_a_chip_switches_to_it();
    void currentChanged_fires_only_on_a_real_change();
    void the_checked_box_is_one_frame_with_one_square();
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

// Reported live: the up and down arrows are missing. Each stepper cell is a
// green plate whose ink is a black triangle - measured 0 black pixels in all
// 15 cells of the real dialog.
void TstTvKit::the_stepper_cells_draw_their_arrows()
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
        const int ink = countColorIn(img, QColor(Qt::black), 30, c);
        QVERIFY2(ink > 4,
                 qPrintable(QStringLiteral("the stepper cell %1 has no arrow - %2 black px")
                                .arg(QStringLiteral("%1,%2 %3x%4")
                                         .arg(c.x()).arg(c.y()).arg(c.width()).arg(c.height()))
                                .arg(ink)));
        // The arrow sits in the middle of the cell, not on its edge.
        const int centre = countColorIn(img, QColor(Qt::black), 30,
                                        c.adjusted(3, 3, -3, -3));
        QVERIFY2(centre > 0, "the arrow ink is only on the cell edge, not in its middle");
    }
}

// Reported live: "spinboxes need the highlight on press / revert on release
// but no shadow". The stepper cells already have a block shadow at REST (the
// reference look) - what they lack is the press feedback every other control
// now has. Pressed: the cell turns the bright green and travels onto its
// shadow, like the plates. Released: it comes back to the resting green, and
// the shadow returns.
void TstTvKit::pressing_a_stepper_highlights_it_and_release_reverts_without_a_shadow()
{
    TvSpinBox sb;
    sb.setRange(0.0, 10.0);
    sb.setValue(5.0);
    // A REAL resize so layoutCells() runs against the actual widget size -
    // resize(sizeHint()) alone can leave cells laid out for a stale height and
    // the click would miss them entirely (the press would never land).
    sb.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sb));
    sb.resize(120, 40);
    QCoreApplication::processEvents();
    // Force one more layout pass: on a cold/full-suite run the very first
    // layout can leave the cells one size stale, and the probe then lands on
    // the triangle instead of the green (a real 1-off flake, not a product
    // bug - the press/release behaviour itself is deterministic).
    sb.resize(120, 41);
    sb.resize(120, 40);
    QCoreApplication::processEvents();

    const QRect up = sb.upRect();
    QVERIFY(!up.isNull());
    QVERIFY2(up.width() > 0 && up.height() > 0 && up.right() < sb.width(),
             qPrintable(QStringLiteral("the cell geometry is off the widget - up=%1,%2 %3x%4 "
                                       "size=%5x%6")
                            .arg(up.x()).arg(up.y()).arg(up.width()).arg(up.height())
                            .arg(sb.width()).arg(sb.height())));
    // The cells hug the widget's right edge, so the shadow band past them is
    // CLIPPED at the widget boundary. Probe ONE COLUMN INSIDE the cell's right
    // edge instead: at rest it is cell green (the shadow lives outside), and
    // when the cell travels right onto its shadow this column is the shadow -
    // black. The triangle is inset from every edge, so y = top + 2 stays green.
    const QPoint probe(up.right() - 1, up.top() + 1);
    const int kOffset = 3;   // the cell's block shadow (TvSpinBox's kButtonShadow)
    Q_UNUSED(kOffset);

    const QImage idle = grab(sb);
    const QColor rest = idle.pixelColor(probe);
    QVERIFY2(rest.green() == 0xAA && rest.red() == 0,
             qPrintable(QStringLiteral("the resting cell is not flat green at its edge - got "
                                       "%1,%2,%3")
                            .arg(rest.red()).arg(rest.green()).arg(rest.blue())));

    // Hold the cell down: the plate travels onto that shadow.
    QTest::mousePress(&sb, Qt::LeftButton, Qt::NoModifier, up.center());
    QCoreApplication::processEvents();
    const QImage down = grab(sb);
    const QColor lit = down.pixelColor(probe);
    QVERIFY2(!(lit.red() > 0x20 && lit.green() > 0xAA),
             qPrintable(QStringLiteral("the pressed cell did not travel - the probe is still "
                                       "bright green (%1,%2,%3)")
                            .arg(lit.red()).arg(lit.green()).arg(lit.blue())));

    // ...and the vacated corner shows the panel, so the cell really moved.
    const QPoint cornerProbe = up.topLeft() + QPoint(1, 1);
    const QColor vacated = down.pixelColor(cornerProbe);
    QVERIFY2(!(vacated.green() > 0xAA && vacated.red() < 16),
             qPrintable(QStringLiteral("the vacated corner is still plate green (%1,%2,%3)")
                            .arg(vacated.red()).arg(vacated.green()).arg(vacated.blue())));

    QTest::mouseRelease(&sb, Qt::LeftButton, Qt::NoModifier, up.center());
    QCoreApplication::processEvents();
    // Deterministic settle: the release repaints the cell, and a grab taken on
    // the very next event-loop tick can read one repaint stale (black) on a
    // loaded full-suite run. A second processEvents + repaint guarantees the
    // cell has come back up before we sample it.
    sb.repaint();
    QCoreApplication::processEvents();
    const QImage up2 = grab(sb);
    const QColor back = up2.pixelColor(probe);
    fprintf(stderr, "T6 probe=%d,%d up=%d,%d %dx%d size=%dx%d back=%d,%d,%d\n",
            probe.x(), probe.y(), up.x(), up.y(), up.width(), up.height(), sb.width(),
            sb.height(), back.red(), back.green(), back.blue());
    QVERIFY2(back.green() >= 0xAA && back.red() < 16,
             qPrintable(QStringLiteral("the cell did not return to rest - probe is %1,%2,%3")
                            .arg(back.red()).arg(back.green()).arg(back.blue())));
    const QColor cornerBack = up2.pixelColor(cornerProbe);
    QVERIFY2(cornerBack.green() >= 0xAA && cornerBack.red() < 16,
             qPrintable(QStringLiteral("the vacated corner did not refill - %1,%2,%3")
                            .arg(cornerBack.red()).arg(cornerBack.green())
                            .arg(cornerBack.blue())));
}

// Reported live: the blue boxes should not have white borders. The field is a
// flat blue field on the panel - no ring around it.
void TstTvKit::the_spin_field_has_no_white_border()
{
    TvSpinBox sb;
    sb.setRange(0.0, 1.0);
    sb.setDecimals(2);
    sb.setValue(0.5);
    sb.resize(sb.sizeHint());
    const QImage img = grab(sb);
    QVERIFY2(!img.isNull(), "the spin box never showed");

    const QRect f = sb.fieldRect();
    QVERIFY(!f.isNull());
    // A border sits ON the field's own top and bottom edge rows. NOTE: probe
    // the two rows separately - uniting them yields a rect spanning the whole
    // field, whose white TEXT would make the count lie.
    const QRect topEdge(f.left(), f.top(), f.width(), 1);
    const QRect bottomEdge(f.left(), f.bottom(), f.width(), 1);
    const int whiteTop = countColorIn(img, QColor(Qt::white), 20, topEdge);
    const int whiteBottom = countColorIn(img, QColor(Qt::white), 20, bottomEdge);
    const int white = whiteTop + whiteBottom;
    QVERIFY2(white == 0,
             qPrintable(QStringLiteral("the blue field's edge carries a white border - %1 white px "
                                       "(top %2, bottom %3)")
                            .arg(white).arg(whiteTop).arg(whiteBottom)));
    // And the field is blue where it is actually visible - probe a span just
    // inside its left edge. (A corner-on probe lands on layout rounding.)
    const QColor blue(0x00, 0x00, 0xAA);
    const int blueInField = countColorIn(img, blue, 20, QRect(f.left() + 4, f.top() + 4, 8, 6));
    QVERIFY2(blueInField >= 20,
             qPrintable(QStringLiteral("the field is not blue where it should be (%1 blue px)")
                            .arg(blueInField)));
}

// Reported live: the checkboxes are not drawn correctly. The reference is a
// SINGLE thin frame with one square in the middle - not a double-walled box
// with the fill bleeding into the channel between its two rules.
void TstTvKit::the_checked_box_is_one_frame_with_one_square()
{
    QWidget panel;
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.setChecked(true);
    const QImage img = grabOnGreyPanel(cb, panel);
    QVERIFY2(!img.isNull(), "the checkbox never showed");

    const QRect box = cb.boxRect();
    QVERIFY2(box.width() > 6 && box.height() > 6, "the box has no room for a square");
    const QColor black(Qt::black);
    const QColor green(0x00, 0xAA, 0x00);
    const QColor grey(0xAA, 0xAA, 0xAA);

    // A single-frame box has ONE black rule per side: probe the row halfway
    // down the box, where a double box would show THREE rules (outer, inner,
    // inner) plus the fill between them.
    const int mid = box.center().y();
    int rules = 0;
    for (int x = box.left(); x <= box.right(); ++x)
        if (countColorIn(img, black, 30, QRect(x, mid, 1, 1)) > 0)
            ++rules;
    QVERIFY2(rules <= 2,
             qPrintable(QStringLiteral("the box middle row carries %1 black rules - a double-"
                                       "walled box, not a single frame").arg(rules)));

    // The green square is ONE connected block in the middle, and grey daylight
    // survives between it and the frame on every side.
    const int greenTotal = countColorIn(img, green, 40, box);
    QVERIFY2(greenTotal > 8,
             qPrintable(QStringLiteral("the checked box carries only %1 green px").arg(greenTotal)));
    // Daylight: the frame's inner edge must still show grey just inside the
    // outline, at the top-left of the interior.
    const QRect corner = QRect(box.left() + 1, box.top() + 1, 3, 3);
    const int greyAtCorner = countColorIn(img, grey, 30, corner);
    QVERIFY2(greyAtCorner > 0,
             "the fill touches the frame at the top-left - there is no daylight inside");
}
QTEST_MAIN(TstTvKit)
#include "tst_tvkit.moc"
