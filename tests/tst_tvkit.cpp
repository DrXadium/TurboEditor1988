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

} // namespace

class TstTvKit : public QObject
{
    Q_OBJECT

private slots:
    // TvCheckBox
    void the_box_is_empty_when_unchecked();
    void the_box_shows_a_cross_when_checked();
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

void TstTvKit::the_box_is_empty_when_unchecked()
{
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.resize(cb.sizeHint());
    const QImage img = grab(cb);
    QVERIFY2(!img.isNull(), "the checkbox never showed");

    const int ink = countColorIn(img, QColor(Qt::white), 20, boxCentre(cb.boxRect()));
    QVERIFY2(ink == 0,
             qPrintable(QStringLiteral("an unchecked box carries %1 white px at its centre").arg(ink)));
    QVERIFY2(cb.boxRect().width() > 6 && cb.boxRect().height() > 6,
             "the box has no room for a cross");
}

void TstTvKit::the_box_shows_a_cross_when_checked()
{
    TvCheckBox cb(QStringLiteral("Scanlines"));
    cb.setChecked(true);
    cb.resize(cb.sizeHint());
    const QImage img = grab(cb);
    QVERIFY2(!img.isNull(), "the checkbox never showed");

    const int ink = countColorIn(img, QColor(Qt::white), 20, boxCentre(cb.boxRect()));
    QVERIFY2(ink > 0,
             "a checked box shows no cross at its centre - the diagonals do not meet");
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

QTEST_MAIN(TstTvKit)
#include "tst_tvkit.moc"
