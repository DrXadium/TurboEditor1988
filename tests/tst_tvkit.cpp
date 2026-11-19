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

QTEST_MAIN(TstTvKit)
#include "tst_tvkit.moc"
