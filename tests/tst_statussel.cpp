// SPDX-License-Identifier: GPL-3.0-or-later
// Status-line selectors (F-row indicators + dropdowns).
// Task 1: the Sonnet dictionary list must be reachable (links KF6::SonnetCore).
#include <QtTest>
#include <sonnet/speller.h>
#include <QImage>
#include "src/editor/StatusSelectors.h"

namespace {
const QColor kOvrRed(0xAA, 0x00, 0x00);

// Fraction of pixels near `want` inside the widget's grab.
double inkFraction(const QImage& img, const QColor& want, int tol = 24)
{
    int hits = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - want.red()) <= tol && qAbs(c.green() - want.green()) <= tol &&
                qAbs(c.blue() - want.blue()) <= tol)
                ++hits;
        }
    return img.isNull() ? 0.0 : double(hits) / double(img.width() * img.height());
}
} // namespace

class TstStatusSel : public QObject
{
    Q_OBJECT
private slots:
    void spell_list_is_nonempty_when_dicts_exist()
    {
        Sonnet::Speller speller;
        const QMap<QString, QString> dicts = speller.availableDictionaries();
        QVERIFY2(!dicts.isEmpty(),
                 "no spell dictionaries found (hunspell dicts should be in /usr/share/hunspell)");
    }

    // Task 2: the INS/OVR indicator. OVR must be drawn RED (#AA0000); INS must
    // NOT be red (it uses the bar's normal ink).
    void ovr_indicator_is_red_and_ins_is_not()
    {
        StatusSelectors s;
        s.resize(240, 20);
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));

        s.setOverwrite(true);
        QCoreApplication::processEvents();
        const QImage ovrImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double ovrRed = inkFraction(ovrImg, kOvrRed);
        QVERIFY2(ovrRed > 0.0,
                 qPrintable(QStringLiteral("OVR indicator has no red ink (%1% of pixels)")
                                .arg(ovrRed * 100.0, 0, 'f', 2)));

        s.setOverwrite(false);
        QCoreApplication::processEvents();
        const QImage insImg = s.grab().toImage().convertToFormat(QImage::Format_RGB32);
        const double insRed = inkFraction(insImg, kOvrRed);
        QVERIFY2(insRed == 0.0,
                 qPrintable(QStringLiteral("INS indicator is red (%1% of pixels) - only OVR is red")
                                .arg(insRed * 100.0, 0, 'f', 2)));
    }
};

QTEST_MAIN(TstStatusSel)
#include "tst_statussel.moc"
