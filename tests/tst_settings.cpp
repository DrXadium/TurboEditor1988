// SPDX-License-Identifier: GPL-3.0-or-later
// Task 6: C++ CrtSettings contract test.
// Proves: (1) QSettings persistence survives a save/load round-trip into a
// fresh instance; (2) defaults match cool-retro-term's ApplicationSettings;
// (3) effect properties emit NOTIFY signals (required for live QML binding).
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QSettings>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include "src/settings/CrtSettings.h"
#include "src/AppIdentity.h"
#include <QLabel>
#include <QSpinBox>
#include "src/settings/SettingsDialog.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvSpinBox.h"

class TstSettings : public QObject
{
    Q_OBJECT
private slots:
    // Reported live: the up and down arrows are missing from every stepper in
    // the dialog, though a TvSpinBox drawn alone paints them. The dialog is the
    // environment that matters - build the real one, show it, grab the real
    // control and count its arrow ink.
    void the_settings_dialog_steppers_draw_their_arrows();

    // Rename guard: settings must keep living at ~/.config/CRT-Kate/crt.ini.
    // If the default store path follows the new display name, the user's
    // saved preferences are silently lost on next launch.
    void default_store_path_stays_under_legacy_storage() {
        CrtSettings s;
        QVERIFY2(s.storePath().endsWith(QStringLiteral("/CRT-Kate/crt.ini")),
                 qPrintable(QStringLiteral("settings store moved to '%1'").arg(s.storePath())));
    }

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
        // Curvature is FLAT by default (Task 2 / user: no curved-CRT gimmick) —
        // was cool-retro-term's 0.3; scanlines/bloom/texture stay on.
        QCOMPARE(s.screenCurvature(), 0.0);
        QCOMPARE(s.contrast(), 0.80);
        QCOMPARE(s.staticNoise(), 0.12);
        QCOMPARE(s.bloom(), 0.55);
        QCOMPARE(s.fontColor(), QColor("#ff8100"));
    }

    // Task 12: Turbo Vision default editor font is 16px and persists.
    void font_size_defaults_to_16_and_roundtrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            QCOMPARE(s.fontSize(), 16);            // TV default
            s.setStorePath(store);
            s.setFontSize(20);
            QCOMPARE(s.fontSize(), 20);
            QVERIFY2(s.save(), "save() returned false");
        }
        {
            CrtSettings s;                          // fresh instance
            s.setStorePath(store);
            QVERIFY2(s.load(), "load() returned false");
            QCOMPARE(s.fontSize(), 20);             // persisted
        }
        // NOTIFY signal fires on real change only.
        CrtSettings s;
        QSignalSpy spy(&s, &CrtSettings::fontSizeChanged);
        s.setFontSize(s.fontSize());                // no change -> no emit
        const int before = spy.count();
        s.setFontSize(24);
        QVERIFY2(spy.count() > before,
                 "fontSizeChanged did not emit on a real change");
    }

    // Deferred-minor fix: a corrupt/unreadable store must not silently wipe
    // saved settings — load() reports false and the defaults stay intact.
    // Task 2 regression: a CRT_KATE_BRIGHT dev session must not persist its
    // temporary brightness into the user's saved settings.
    void dev_brightness_does_not_persist() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt.ini");
        {
            CrtSettings s;
            s.setStorePath(store);
            s.setBrightness(0.5);          // the user's real value
            s.applyDevBrightness(3.0);     // CRT_KATE_BRIGHT override
            QCOMPARE(s.brightness(), 3.0); // active for the capture
            s.restoreDevBrightness();      // on quit
            QCOMPARE(s.brightness(), 0.5); // back to the user's value
            QVERIFY(s.save());
        }
        {
            CrtSettings s;
            s.setStorePath(store);
            QVERIFY(s.load());
            QCOMPARE(s.brightness(), 0.5); // persisted = user value, not 3.0
        }
    }

    // Task 2: the CRT is FLAT by default (user: "don't need the curved gimmick")
    // while keeping scanlines/sweep/bloom/texture.
    void curvature_defaults_to_flat() {
        CrtSettings s;
        QCOMPARE(s.screenCurvature(), 0.0);
        QVERIFY2(!s.curvatureEnabled(),
                 "curvature should be off by default (flat viewport)");
    }

    // The CRT still reads as a CRT: scanlines + bloom stay on by default.
    void crt_effects_keep_scanlines_and_bloom_by_default() {
        CrtSettings s;
        QVERIFY2(s.scanlinesEnabled(), "scanlines must stay on by default");
        QVERIFY2(s.rasterizationIntensity() > 0.0,
                 "scanline intensity must be > 0 by default");
        QVERIFY2(s.glowEnabled(), "bloom/glow must stay on by default");
    }

    void load_failure_is_reported_and_defaults_survive() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Unreadable store: a DIRECTORY where the .ini should be.
        const QString store = dir.filePath("corrupt-store");
        QVERIFY(QDir().mkpath(store));

        CrtSettings s;
        s.setStorePath(store);
        QVERIFY2(!s.load(),
                 "load() must return false when the store can't be read");
        QCOMPARE(s.fontSize(), 16);          // defaults intact
        QCOMPARE(s.scanlinesEnabled(), true); // defaults intact
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

    // Deferred-minor fix: the Appearance tab carries no stale copy claiming
    // controls are "wired in later tasks" — the font-size control is wired now.
    void appearance_tab_has_no_stale_future_copy() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        const auto labels = dlg.findChildren<QLabel*>();
        for (const QLabel* lab : labels) {
            QVERIFY2(!lab->text().contains(QStringLiteral("later tasks")),
                     qPrintable("stale label copy: " + lab->text()));
            QVERIFY2(!lab->text().contains(QStringLiteral("wired in later tasks")),
                     qPrintable("stale label copy: " + lab->text()));
        }
        // And the wired control is actually present.
        QVERIFY2(dlg.findChild<TvSpinBox*>(QStringLiteral("fontSizeSpin")),
                 "fontSizeSpin missing from the Appearance tab");
    }

    // Rename: the Settings dialog is user-facing and must name the app
    // the new display name, not the old CRT-Kate.
    void settings_dialog_title_names_turbo_kate() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        QVERIFY2(dlg.windowTitle().contains(QStringLiteral("TURBO Editor 1998 Edition")),
                 qPrintable(QStringLiteral("Settings title still says '%1'")
                                .arg(dlg.windowTitle())));
        QVERIFY2(!dlg.windowTitle().contains(QStringLiteral("CRT-Kate")),
                 qPrintable(QStringLiteral("Settings title still mentions the old name: '%1'")
                                .arg(dlg.windowTitle())));
    }

    // --- SettingsDialog (widget contract) --------------------------------
    void dialog_reflects_settings_on_open() {
        CrtSettings s;
        s.setBrightness(0.7);
        s.setScreenCurvature(0.0);
        SettingsDialog dlg(&s);
        auto* bright = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
        auto* curv = dlg.findChild<TvCheckBox*>(QStringLiteral("curvatureCheck"));
        QVERIFY2(bright, "brightnessSpin not found in dialog");
        QVERIFY2(curv, "curvatureCheck not found in dialog");
        QCOMPARE(bright->value(), 0.7);
        QVERIFY(!curv->isChecked());   // curvature 0.0 => disabled
    }

    void dialog_edits_write_to_settings_live() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        QSignalSpy brightSpy(&s, &CrtSettings::brightnessChanged);

        auto* bright = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
        auto* scan = dlg.findChild<TvCheckBox*>(QStringLiteral("scanlinesCheck"));
        QVERIFY(bright && scan);

        bright->setValue(0.42);
        QCOMPARE(brightSpy.count(), 1);
        QCOMPARE(s.brightness(), 0.42);

        // Toggle applies immediately (OK/Cancel: live preview, Cancel reverts).
        scan->setChecked(false);
        QVERIFY(!s.scanlinesEnabled());
        scan->setChecked(true);
        QVERIFY(s.scanlinesEnabled());
    }

    void dialog_curvature_checkbox_roundtrip() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        auto* curv = dlg.findChild<TvCheckBox*>(QStringLiteral("curvatureCheck"));
        auto* curvSpin = dlg.findChild<TvSpinBox*>(QStringLiteral("curvatureSpin"));
        QVERIFY(curv && curvSpin);

        curv->setChecked(true);
        QCOMPARE(s.screenCurvature(), 0.3);   // enabled => default curvature
        QVERIFY(curvSpin->isEnabled());
        curv->setChecked(false);
        QCOMPARE(s.screenCurvature(), 0.0);   // disabled => flat
        QVERIFY(!curvSpin->isEnabled());
    }

    // Autosave round (approach A): a user-configurable autosave interval in
    // SECONDS, defaulting to 120, persisted with the rest of the settings.
    void autosave_interval_seconds_defaults_to_120_and_roundtrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            QCOMPARE(s.autosaveIntervalSeconds(), 120);
            s.setStorePath(store);
            s.setAutosaveIntervalSeconds(45);
            QCOMPARE(s.autosaveIntervalSeconds(), 45);
            QVERIFY2(s.save(), "save() returned false");
        }

        CrtSettings loaded;
        loaded.setStorePath(store);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.autosaveIntervalSeconds(), 45);
    }

    // The interval must stay inside the spin box's range: 0 would mean "never"
    // or a hot loop, and an unbounded value could never be typed anyway.
    void autosave_interval_seconds_is_bounded() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CrtSettings s;
        s.setStorePath(dir.filePath("crt-kate.ini"));

        s.setAutosaveIntervalSeconds(1);
        QCOMPARE(s.autosaveIntervalSeconds(), 1);
        s.setAutosaveIntervalSeconds(0);        // below minimum -> clamped
        QVERIFY(s.autosaveIntervalSeconds() >= 1);
        s.setAutosaveIntervalSeconds(999999);   // above maximum -> clamped
        QVERIFY(s.autosaveIntervalSeconds() <= 3600);
    }

    // Minimap visibility persists (user report 2026-11-21): toggling
    // View > Show Minimap must survive a restart. Default stays OFF (the
    // TV-revision rule that the minimap is hidden unless asked for).
    void minimap_visible_defaults_off_and_roundtrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            QCOMPARE(s.minimapVisible(), false);   // default off
            s.setStorePath(store);
            s.setMinimapVisible(true);
            QCOMPARE(s.minimapVisible(), true);
            QVERIFY2(s.save(), "save() returned false");
        }

        CrtSettings loaded;
        loaded.setStorePath(store);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.minimapVisible(), true);

        // ... and OFF round-trips too (turning it back off must stick).
        loaded.setMinimapVisible(false);
        QVERIFY(loaded.save());
        CrtSettings reloaded;
        reloaded.setStorePath(store);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.minimapVisible(), false);
    }

    // The dialog exposes the setting as a spin box seeded from the settings
    // value and wired back into it (the live path main.cpp reads).
    void settings_dialog_offers_an_autosave_interval_spin() {
        CrtSettings s;
        s.setAutosaveIntervalSeconds(77);
        SettingsDialog dlg(&s);
        auto* spin = dlg.findChild<TvSpinBox*>(
            QStringLiteral("autosaveIntervalSpin"));
        QVERIFY2(spin, "no autosaveIntervalSpin in the Settings dialog");
        QCOMPARE(spin->value(), 77);
        QCOMPARE(spin->minimum(), 1);
        QCOMPARE(spin->maximum(), 3600);

        spin->setValue(30);
        QCOMPARE(s.autosaveIntervalSeconds(), 30);
    }
};

// Reported live: the arrows are missing from every stepper in the dialog.
// A TvSpinBox grabbed alone paints them (tst_tvkit passes) - so the parity
// lives in the DIALOG's environment. Build the real SettingsDialog, show it,
// and grab the real control.
void TstSettings::the_settings_dialog_steppers_draw_their_arrows()
{
    // Replicate the APP's environment exactly, or the probe sizes a taller,
    // friendlier widget and never sees the bug:
    //  (1) load the bundled VGA8 TTF as the app-wide default font, and
    //  (2) load borland.qss (font-size 16px), as ShellWindow does.
    // The app's real spin box then measures 82x18 -> cells 9px -> the
    // triangle's 3px inset lands BELOW its own guard and draws nothing.
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

    CrtSettings s;
    SettingsDialog dlg(&s);
    // The app resizes the modal to its sizeHint - same path.
    dlg.resize(dlg.sizeHint());
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));

    auto* spin = dlg.findChild<TvSpinBox*>(QStringLiteral("brightnessSpin"));
    QVERIFY2(spin, "the settings dialog has no brightness stepper");
    // The real app's spin box measures 82x18; assert the layout really gave it
    // that tight size, so this test cannot quietly pass by probing a taller,
    // friendlier widget than the one the user sees.
    QVERIFY2(spin->height() <= 20,
             qPrintable(QStringLiteral("the stepper was laid out %1px tall - too tall to be the "
                                       "app's 18; the probe would not see the bug")
                            .arg(spin->height())));
    const QImage img = spin->grab().toImage();
    QVERIFY2(!img.isNull(), "the stepper never showed");

    const QRect cells[] = {spin->upRect(), spin->downRect()};
    for (const QRect& c : cells) {
        QVERIFY(!c.isNull());
        int ink = 0;
        for (int y = c.top(); y <= c.bottom(); ++y)
            for (int x = c.left(); x <= c.right(); ++x) {
                const QColor p = img.pixelColor(x, y);
                if (p.red() < 60 && p.green() < 60 && p.blue() < 60)
                    ++ink;
            }
        QVERIFY2(ink > 4,
                 qPrintable(QStringLiteral("the dialog stepper cell %1 (%2x%3) carries no arrow - %4 "
                                           "black px")
                                .arg(QStringLiteral("%1,%2").arg(c.x()).arg(c.y()))
                                .arg(c.width()).arg(c.height()).arg(ink)));
    }
    dlg.hide();
    qApp->setStyleSheet(QString());
}

int main(int argc, char** argv)
{
    // QApplication: the dialog slots instantiate real widgets.
    QApplication app(argc, argv);
    TstSettings tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_settings.moc"