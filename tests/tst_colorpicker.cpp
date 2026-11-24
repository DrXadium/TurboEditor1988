// SPDX-License-Identifier: GPL-3.0-or-later
// Full-range colour picker panel (2026-11-23).
//
// The body of the "Color" modal: a saturation/value square, a hue strip, a hex
// field and a preview chip. Xadium asked for the FULL range ("it may look retro
// but we'll be doing modern work"), so the model is HSV, not a palette grid.
//
// Proves:
//  1. a colour set from outside round-trips EXACTLY (no HSV rounding drift);
//  2. the primaries survive the same trip;
//  3. clicking the SV square picks saturation/value there;
//  4. clicking the hue strip picks that hue;
//  5. typing a valid hex updates the colour and the markers;
//  6. typing junk changes nothing (the picker must never commit a lie);
//  7. the preview chip actually paints the candidate colour.
#include <QtTest>
#include <QImage>
#include <QLineEdit>
#include "src/shell/ColorPickerPanel.h"
#include "src/shell/TvLineEdit.h"
#include "src/shell/ColorPickerModal.h"
#include "src/shell/ShellWindow.h"
#include "src/bridge/ContentBridge.h"
#include <QTimer>

class TstColorPicker : public QObject
{
    Q_OBJECT
private slots:
    void set_colour_round_trips_exactly()
    {
        ColorPickerPanel p;
        p.setColor(QColor(0x12, 0x34, 0x56));
        QCOMPARE(p.color(), QColor(0x12, 0x34, 0x56));
    }

    void primaries_round_trip()
    {
        ColorPickerPanel p;
        const QColor cases[] = { QColor(0xff,0x00,0x00), QColor(0x00,0xff,0x00),
                                 QColor(0x00,0x00,0xff), QColor(0xff,0xff,0xff),
                                 QColor(0x00,0x00,0x00) };
        for (const QColor& c : cases) {
            p.setColor(c);
            QCOMPARE(p.color(), c);
        }
    }

    void hex_field_shows_the_colour()
    {
        ColorPickerPanel p;
        p.setColor(QColor(0xff, 0x88, 0x00));
        QVERIFY(p.hexField());
        QCOMPARE(p.hexField()->text().toLower(), QStringLiteral("#ff8800"));
    }

    void clicking_the_sv_square_sets_saturation_and_value()
    {
        ColorPickerPanel p;
        p.resize(360, 260);
        p.setColor(QColor(0xff, 0x00, 0x00));   // hue 0, s 255, v 255

        const QRect sv = p.svRect();
        QVERIFY(sv.isValid());
        // Bottom-left CORNER pixel: saturation 0, value 0 -> black. (One pixel
        // in would already be value 2, which is the mapping working, not a bug.)
        QTest::mousePress(&p, Qt::LeftButton, Qt::NoModifier, sv.bottomLeft());
        QTest::mouseRelease(&p, Qt::LeftButton, Qt::NoModifier, sv.bottomLeft());
        QCOMPARE(p.color(), QColor(0x00, 0x00, 0x00));

        // Top-right CORNER pixel: saturation 255, value 255 -> the pure hue.
        QTest::mousePress(&p, Qt::LeftButton, Qt::NoModifier, sv.topRight());
        QTest::mouseRelease(&p, Qt::LeftButton, Qt::NoModifier, sv.topRight());
        QCOMPARE(p.color(), QColor(0xff, 0x00, 0x00));
    }

    void clicking_the_hue_strip_changes_the_hue()
    {
        ColorPickerPanel p;
        p.resize(360, 260);
        p.setColor(QColor(0xff, 0x00, 0x00));   // hue 0

        const QRect hue = p.hueRect();
        QVERIFY(hue.isValid());
        // A third of the way down the strip is the green band (120 degrees).
        const QPoint at(hue.center().x(), hue.top() + hue.height() / 3);
        QTest::mousePress(&p, Qt::LeftButton, Qt::NoModifier, at);
        QTest::mouseRelease(&p, Qt::LeftButton, Qt::NoModifier, at);
        const int h = p.color().hsvHue();
        QVERIFY2(h >= 100 && h <= 140,
                 qPrintable(QStringLiteral("hue after a third of the strip: %1").arg(h)));
    }

    void typing_a_valid_hex_updates_the_colour()
    {
        ColorPickerPanel p;
        p.setColor(QColor(0xff, 0x00, 0x00));
        QVERIFY(p.hexField());
        p.hexField()->setText(QStringLiteral("#00ff00"));
        QCOMPARE(p.color(), QColor(0x00, 0xff, 0x00));
    }

    void typing_an_invalid_hex_leaves_the_colour_alone()
    {
        ColorPickerPanel p;
        const QColor before(0xff, 0x88, 0x00);
        p.setColor(before);
        p.hexField()->setText(QStringLiteral("#zzzzzz"));
        QCOMPARE(p.color(), before);
    }

    // The picker is shown through the house modal, not a window of its own:
    // the dialog owns the panel, seeds it with the clicked token's colour, and
    // reports what the user is pointing at.
    void modal_hosts_the_panel_and_seeds_it()
    {
        ColorPickerModal modal;
        QVERIFY2(modal.panel(), "modal has no picker panel");
        QCOMPARE(modal.buttonCount(), 2);   // Cancel + OK

        modal.setInitialColor(QColor(0x12, 0x34, 0x56));
        QCOMPARE(modal.initialColor(), QColor(0x12, 0x34, 0x56));
        QCOMPARE(modal.pickedColor(), QColor(0x12, 0x34, 0x56));
        QCOMPARE(modal.panel()->originalColor(), QColor(0x12, 0x34, 0x56));

        // Typing a new hex in the panel is what OK would commit.
        modal.panel()->hexField()->setText(QStringLiteral("#abcdef"));
        QCOMPARE(modal.pickedColor(), QColor(0xab, 0xcd, 0xef));
        // ... and it never changed the "before" colour.
        QCOMPARE(modal.panel()->originalColor(), QColor(0x12, 0x34, 0x56));
    }

    // Xadium 2026-11-23: "while the dialog pops up you can't click in it".
    // The live app delivers every mouse event through ContentBridge's
    // synthesized routing, so this drives the real path: a press on the OK
    // plate and a press in the SV square must reach the dialog.
    void presses_reach_the_dialog_through_the_bridge()
    {
        ShellWindow w;
        w.resize(1000, 640);
        ContentBridge b;
        b.setShellWindow(&w);

        ColorPickerModal modal;
        modal.setInitialColor(QColor(0xff, 0x00, 0x00));

        // Inside the modal's own loop, click the SV square's bottom-left corner
        // (black) and then the OK plate.
        QTimer::singleShot(60, &w, [&]() {
            const QPoint sv = modal.panel()->mapTo(&w, modal.panel()->svRect().bottomLeft());
            b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier, sv.x(), sv.y());
            b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier, sv.x(), sv.y());
        });
        QTimer::singleShot(140, &w, [&]() {
            const QPoint ok = modal.mapTo(&w, modal.buttonRect(1).center());
            b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier, ok.x(), ok.y());
            b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier, ok.x(), ok.y());
        });

        const int role = TvModal::exec(&w, &modal);
        QCOMPARE(role, int(TvModal::Accept));
        QVERIFY2(modal.pickedColor() == QColor(0x00, 0x00, 0x00),
                 qPrintable(QStringLiteral("the SV square press did not reach the panel: %1")
                                .arg(modal.pickedColor().name())));
    }

    void preview_chip_paints_the_candidate_colour()
    {
        ColorPickerPanel p;
        p.resize(360, 260);
        p.setColor(QColor(0x00, 0xff, 0x00));

        const QRect chip = p.previewRect();
        QVERIFY2(chip.isValid(), "no preview chip geometry");
        const QImage img = p.grab().toImage().convertToFormat(QImage::Format_RGB32);
        int hits = 0;
        for (int y = chip.top() + 2; y <= chip.bottom() - 2; ++y)
            for (int x = chip.left() + 2; x <= chip.right() - 2; ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                    continue;
                const QColor c = img.pixelColor(x, y);
                if (qAbs(c.green() - 255) <= 20 && c.red() <= 20 && c.blue() <= 20)
                    ++hits;
            }
        QVERIFY2(hits >= 20,
                 qPrintable(QStringLiteral("preview chip not painted (%1 px)").arg(hits)));
    }
};

QTEST_MAIN(TstColorPicker)
#include "tst_colorpicker.moc"
