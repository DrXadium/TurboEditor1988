// SPDX-License-Identifier: GPL-3.0-or-later
// Task 6: C++ CrtSettings contract test.
// Proves: (1) QSettings persistence survives a save/load round-trip into a
// fresh instance; (2) defaults match cool-retro-term's ApplicationSettings;
// (3) effect properties emit NOTIFY signals (required for live QML binding).
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include "src/settings/CrtSettings.h"
#include "src/settings/SettingsDialog.h"

class TstSettings : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        // A default-constructed instance must expose cool-retro-term defaults
        // (guards against a broken default-initializer before other slots run).
        CrtSettings s;
        QVERIFY(s.brightness() > 0.0);
    }

    void brightness_default_matches_cool_retro_term() {
        CrtSettings s;
        // cool-retro-term ApplicationSettings.qml: brightness = 0.5
        QCOMPARE(qRound(s.brightness() * 100), 50);
        // Other core defaults (from Task 2's QML model / cool-retro-term).
        QCOMPARE(s.screenCurvature(), 0.3);
        QCOMPARE(s.contrast(), 0.80);
        QCOMPARE(s.staticNoise(), 0.12);
        QCOMPARE(s.bloom(), 0.55);
        QCOMPARE(s.fontColor(), QColor("#ff8100"));
    }

    void save_load_roundtrip_persists_scanlines_enabled_false() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            s.setStorePath(store);
            s.setScanlinesEnabled(false);
            QVERIFY(!s.scanlinesEnabled());
            QVERIFY2(s.save(), "save() returned false");
        }
        {
            CrtSettings s;                 // fresh instance
            s.setStorePath(store);
            QVERIFY2(s.load(), "load() returned false");
            QVERIFY2(!s.scanlinesEnabled(),
                     "scanlinesEnabled did not persist (expected false)");
        }
    }

    void scanlines_enabled_false_disables_scanline_rasterization() {
        CrtSettings s;
        s.setScanlinesEnabled(false);
        // Off means either rasterization=none or intensity=0.
        QVERIFY(!s.scanlinesEnabled());
        QVERIFY(s.rasterization() == CrtSettings::NoRasterization
                || s.rasterizationIntensity() == 0.0);
    }

    void scanlines_enabled_true_restores_scanlines() {
        CrtSettings s;
        s.setScanlinesEnabled(false);
        s.setScanlinesEnabled(true);
        QVERIFY(s.scanlinesEnabled());
        QVERIFY(s.rasterization() != CrtSettings::NoRasterization);
        QVERIFY(s.rasterizationIntensity() > 0.0);
    }

    void effect_properties_emit_notify() {
        CrtSettings s;
        QSignalSpy spy(&s, &CrtSettings::brightnessChanged);
        s.setBrightness(0.75);
        QCOMPARE(spy.count(), 1);

        QSignalSpy spy2(&s, &CrtSettings::scanlinesEnabledChanged);
        s.setScanlinesEnabled(!s.scanlinesEnabled());
        QCOMPARE(spy2.count(), 1);

        QSignalSpy spy3(&s, &CrtSettings::screenCurvatureChanged);
        s.setScreenCurvature(0.6);
        QCOMPARE(spy3.count(), 1);
    }

    // --- SettingsDialog (widget contract) --------------------------------
    void dialog_reflects_settings_on_open() {
        CrtSettings s;
        s.setBrightness(0.7);
        s.setScreenCurvature(0.0);
        SettingsDialog dlg(&s);
        auto* bright = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("brightnessSpin"));
        auto* curv = dlg.findChild<QCheckBox*>(QStringLiteral("curvatureCheck"));
        QVERIFY2(bright, "brightnessSpin not found in dialog");
        QVERIFY2(curv, "curvatureCheck not found in dialog");
        QCOMPARE(bright->value(), 0.7);
        QVERIFY(!curv->isChecked());   // curvature 0.0 => disabled
    }

    void dialog_edits_write_to_settings_live() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        QSignalSpy brightSpy(&s, &CrtSettings::brightnessChanged);

        auto* bright = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("brightnessSpin"));
        auto* scan = dlg.findChild<QCheckBox*>(QStringLiteral("scanlinesCheck"));
        QVERIFY(bright && scan);

        bright->setValue(0.42);
        QCOMPARE(brightSpy.count(), 1);
        QCOMPARE(s.brightness(), 0.42);

        // Toggle applies immediately (no OK button; Close only).
        scan->setChecked(false);
        QVERIFY(!s.scanlinesEnabled());
        scan->setChecked(true);
        QVERIFY(s.scanlinesEnabled());
    }

    void dialog_curvature_checkbox_roundtrip() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        auto* curv = dlg.findChild<QCheckBox*>(QStringLiteral("curvatureCheck"));
        auto* curvSpin = dlg.findChild<QDoubleSpinBox*>(QStringLiteral("curvatureSpin"));
        QVERIFY(curv && curvSpin);

        curv->setChecked(true);
        QCOMPARE(s.screenCurvature(), 0.3);   // enabled => default curvature
        QVERIFY(curvSpin->isEnabled());
        curv->setChecked(false);
        QCOMPARE(s.screenCurvature(), 0.0);   // disabled => flat
        QVERIFY(!curvSpin->isEnabled());
    }
};

int main(int argc, char** argv)
{
    // QApplication: the dialog slots instantiate real widgets.
    QApplication app(argc, argv);
    TstSettings tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_settings.moc"