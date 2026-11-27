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
#include "src/shell/ColorPickerModal.h"
#include "src/shell/ColorPickerPanel.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvColorSwatch.h"
#include "src/shell/TvPlate.h"
#include "src/shell/TvSelect.h"
#include "src/shell/TvSpinBox.h"
#include "src/shell/TvLineEdit.h"
#include <QPushButton>

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

    // 2026-11-26: the last chosen effect set must still be there on the next
    // launch even when the exit never reaches aboutToQuit (SIGTERM from the
    // relaunch script, a killed dev instance). An effect change therefore
    // schedules its own coalesced write; no explicit save() call is needed.
    void effect_changes_autosave_without_an_explicit_save() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");
        {
            CrtSettings s;
            s.setStorePath(store);
            s.setContrast(1.23);
            s.setSweep(0.77);
            QTest::qWait(400);   // let the 250 ms coalescing timer fire
        }
        {
            CrtSettings s;
            s.setStorePath(store);
            QVERIFY2(s.load(), "load() returned false after an autosave");
            QCOMPARE(s.contrast(), 1.23);
            QCOMPARE(s.sweep(), 0.77);
        }
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

    // Rename: the Settings dialog is user-facing and must name the app the
    // canonical TurboEditor1988, not the legacy CRT-Kate storage identity.
    void settings_dialog_title_names_turboeditor1988() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        QVERIFY2(dlg.windowTitle().contains(QStringLiteral("TurboEditor1988")),
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
        QVERIFY2(bright, "brightnessSpin not found in dialog");
        QCOMPARE(bright->value(), 0.7);
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
            // Recent files on the File menu: ON by default - it is a
            // convenience the user asked for, and the menu hides itself when
            // there is nothing to show.
            QCOMPARE(s.showRecentFiles(), true);
            s.setShowRecentFiles(false);
            QCOMPARE(s.showRecentFiles(), false);

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

    // --- 2026-11-26 four-stage pipeline settings --------------------------

    void pipeline_defaults_match_the_spec() {
        CrtSettings s;
        QCOMPARE(s.chromaColor(), 0.0);       // UI colours preserved by default
        QCOMPARE(s.saturationColor(), 0.0);
        QCOMPARE(s.screenCurvature(), 0.0);
        QCOMPARE(s.phosphorEnabled(), false);
        QCOMPARE(s.scanlinePitch(), 2);
        QCOMPARE(s.glassSheen(), 0.0);
        QCOMPARE(s.screenRadius(), 0.0);
        QCOMPARE(s.screenInset(), 0.0);
        QCOMPARE(s.bloomQuality(), 0.5);
        QCOMPARE(s.burnInQuality(), 0.5);
        QCOMPARE(s.contrast(), 0.80);
        QCOMPARE(s.brightness(), 0.50);
        QCOMPARE(s.ambientLight(), 0.20);
        QCOMPARE(s.staticNoise(), 0.12);
        QCOMPARE(s.glowingLine(), 0.20);
        QCOMPARE(s.burnIn(), 0.25);
        QCOMPARE(s.bloom(), 0.55);
        QCOMPARE(s.jitter(), 0.20);
        QCOMPARE(s.horizontalSync(), 0.08);
        QCOMPARE(s.flickering(), 0.10);
        QCOMPARE(s.rgbShift(), 0.0);
        QCOMPARE(s.rasterization(), CrtSettings::ScanlineRasterization);
        QCOMPARE(s.rasterizationIntensity(), 0.22);
        QCOMPARE(s.effectsEnabled(), true);
    }

    void new_effect_setters_emit_notify_only_on_change() {
        CrtSettings s;

        QSignalSpy phosphor(&s, &CrtSettings::phosphorEnabledChanged);
        s.setPhosphorEnabled(false);           // unchanged
        QCOMPARE(phosphor.count(), 0);
        s.setPhosphorEnabled(true);
        QCOMPARE(phosphor.count(), 1);

        QSignalSpy pitch(&s, &CrtSettings::scanlinePitchChanged);
        s.setScanlinePitch(2);                 // unchanged
        QCOMPARE(pitch.count(), 0);
        s.setScanlinePitch(3);
        QCOMPARE(pitch.count(), 1);

        QSignalSpy sheen(&s, &CrtSettings::glassSheenChanged);
        s.setGlassSheen(0.0);
        QCOMPARE(sheen.count(), 0);
        s.setGlassSheen(0.4);
        QCOMPARE(sheen.count(), 1);

        QSignalSpy radius(&s, &CrtSettings::screenRadiusChanged);
        s.setScreenRadius(0.0);
        QCOMPARE(radius.count(), 0);
        s.setScreenRadius(0.5);
        QCOMPARE(radius.count(), 1);

        QSignalSpy inset(&s, &CrtSettings::screenInsetChanged);
        s.setScreenInset(0.0);
        QCOMPARE(inset.count(), 0);
        s.setScreenInset(0.5);
        QCOMPARE(inset.count(), 1);

        QSignalSpy bloomQuality(&s, &CrtSettings::bloomQualityChanged);
        s.setBloomQuality(0.5);
        QCOMPARE(bloomQuality.count(), 0);
        s.setBloomQuality(0.8);
        QCOMPARE(bloomQuality.count(), 1);

        QSignalSpy burnQuality(&s, &CrtSettings::burnInQualityChanged);
        s.setBurnInQuality(0.5);
        QCOMPARE(burnQuality.count(), 0);
        s.setBurnInQuality(0.8);
        QCOMPARE(burnQuality.count(), 1);
    }

    void new_effect_values_are_bounded() {
        CrtSettings s;
        s.setScanlinePitch(0);
        QCOMPARE(s.scanlinePitch(), 1);
        s.setScanlinePitch(99);
        QCOMPARE(s.scanlinePitch(), 4);
        s.setBloomQuality(0.0);
        QCOMPARE(s.bloomQuality(), 0.1);
        s.setBurnInQuality(9.0);
        QCOMPARE(s.burnInQuality(), 1.0);
        s.setScreenRadius(-1.0);
        QCOMPARE(s.screenRadius(), 0.0);
        s.setScreenInset(2.0);
        QCOMPARE(s.screenInset(), 1.0);
    }

    void new_effect_values_roundtrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            s.setStorePath(store);
            s.setContrast(1.20);
            s.setBrightness(0.70);
            s.setAmbientLight(0.45);
            s.setStaticNoise(0.33);
            s.setScreenCurvature(0.55);
            s.setGlowingLine(0.66);
            s.setBurnIn(0.77);
            s.setBloom(0.88);
            s.setChromaColor(0.44);
            s.setSaturationColor(0.22);
            s.setJitter(0.11);
            s.setHorizontalSync(0.19);
            s.setFlickering(0.09);
            s.setRgbShift(0.31);
            s.setSweep(0.42);
            s.setRasterization(CrtSettings::SubpixelRasterization);
            s.setRasterizationIntensity(0.61);
            s.setPhosphorEnabled(true);
            s.setScanlinePitch(3);
            s.setGlassSheen(0.37);
            s.setScreenRadius(0.48);
            s.setScreenInset(0.59);
            s.setBloomQuality(0.62);
            s.setBurnInQuality(0.73);
            s.setEffectsEnabled(false);
            QVERIFY2(s.save(), "save() returned false");
        }

        CrtSettings loaded;
        loaded.setStorePath(store);
        QVERIFY2(loaded.load(), "load() returned false");
        QCOMPARE(loaded.contrast(), 1.20);
        QCOMPARE(loaded.brightness(), 0.70);
        QCOMPARE(loaded.ambientLight(), 0.45);
        QCOMPARE(loaded.staticNoise(), 0.33);
        QCOMPARE(loaded.screenCurvature(), 0.55);
        QCOMPARE(loaded.glowingLine(), 0.66);
        QCOMPARE(loaded.burnIn(), 0.77);
        QCOMPARE(loaded.bloom(), 0.88);
        QCOMPARE(loaded.chromaColor(), 0.44);
        QCOMPARE(loaded.saturationColor(), 0.22);
        QCOMPARE(loaded.jitter(), 0.11);
        QCOMPARE(loaded.horizontalSync(), 0.19);
        QCOMPARE(loaded.flickering(), 0.09);
        QCOMPARE(loaded.rgbShift(), 0.31);
        QCOMPARE(loaded.sweep(), 0.42);
        QCOMPARE(loaded.rasterization(), CrtSettings::SubpixelRasterization);
        QCOMPARE(loaded.rasterizationIntensity(), 0.61);
        QCOMPARE(loaded.phosphorEnabled(), true);
        QCOMPARE(loaded.scanlinePitch(), 3);
        QCOMPARE(loaded.glassSheen(), 0.37);
        QCOMPARE(loaded.screenRadius(), 0.48);
        QCOMPARE(loaded.screenInset(), 0.59);
        QCOMPARE(loaded.bloomQuality(), 0.62);
        QCOMPARE(loaded.burnInQuality(), 0.73);
        QCOMPARE(loaded.effectsEnabled(), false);
    }

    void master_bypass_keeps_every_effect_value() {
        CrtSettings s;
        s.setBloom(0.66);
        s.setScanlinePitch(4);
        s.setPhosphorEnabled(true);
        s.setEffectsEnabled(false);
        QCOMPARE(s.effectsEnabled(), false);
        // The bypass is not a reset: the selected look is still there.
        QCOMPARE(s.bloom(), 0.66);
        QCOMPARE(s.scanlinePitch(), 4);
        QCOMPARE(s.phosphorEnabled(), true);
        s.setEffectsEnabled(true);
        QCOMPARE(s.effectsEnabled(), true);
        QCOMPARE(s.bloom(), 0.66);
    }

    // --- Named effect profiles --------------------------------------------

    void profiles_save_multiple_select_and_persist() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        {
            CrtSettings s;
            s.setStorePath(store);

            s.setBloom(0.10);
            s.setScanlinePitch(1);
            s.setEffectsEnabled(false);
            QVERIFY(s.saveProfile(QStringLiteral("Amber")));

            s.setBloom(0.90);
            s.setScanlinePitch(4);
            QVERIFY(s.saveProfile(QStringLiteral("Green")));

            s.setBloom(0.40);
            s.setScanlinePitch(2);
            QVERIFY(s.saveProfile(QStringLiteral("VGA")));

            QCOMPARE(s.profileNames(),
                     QStringList({QStringLiteral("Amber"), QStringLiteral("Green"),
                                  QStringLiteral("VGA")}));
            QCOMPARE(s.currentProfile(), QStringLiteral("VGA"));

            // Selecting a profile applies its values.
            s.setEffectsEnabled(true);
            QVERIFY(s.applyProfile(QStringLiteral("Amber")));
            QCOMPARE(s.bloom(), 0.10);
            QCOMPARE(s.scanlinePitch(), 1);
            QCOMPARE(s.currentProfile(), QStringLiteral("Amber"));
            // The master bypass is global, not part of a profile.
            QCOMPARE(s.effectsEnabled(), true);

            QVERIFY(s.applyProfile(QStringLiteral("Green")));
            QCOMPARE(s.bloom(), 0.90);
            QCOMPARE(s.scanlinePitch(), 4);
        }

        // Profiles survive a restart, including the selected one.
        CrtSettings loaded;
        loaded.setStorePath(store);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.profileNames(),
                 QStringList({QStringLiteral("Amber"), QStringLiteral("Green"),
                              QStringLiteral("VGA")}));
        QCOMPARE(loaded.currentProfile(), QStringLiteral("Green"));
        QVERIFY(loaded.applyProfile(QStringLiteral("VGA")));
        QCOMPARE(loaded.bloom(), 0.40);
        QCOMPARE(loaded.scanlinePitch(), 2);

        QVERIFY(loaded.deleteProfile(QStringLiteral("Amber")));
        QCOMPARE(loaded.profileNames(),
                 QStringList({QStringLiteral("Green"), QStringLiteral("VGA")}));
        QVERIFY(!loaded.deleteProfile(QStringLiteral("Amber")));

        CrtSettings reloaded;
        reloaded.setStorePath(store);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.profileNames(),
                 QStringList({QStringLiteral("Green"), QStringLiteral("VGA")}));
    }

    void profiles_reject_empty_names_and_missing_lookups() {
        CrtSettings s;
        QVERIFY(!s.saveProfile(QString()));
        QVERIFY(!s.saveProfile(QStringLiteral("   ")));
        QVERIFY(!s.profileExists(QStringLiteral("nope")));
        QVERIFY(!s.applyProfile(QStringLiteral("nope")));
        QVERIFY(!s.deleteProfile(QStringLiteral("nope")));
    }

    void a_stale_settings_instance_does_not_erase_a_new_profile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString store = dir.filePath("crt-kate.ini");

        CrtSettings writer;
        writer.setStorePath(store);
        writer.load();
        writer.setBloom(0.1);
        QVERIFY(writer.saveProfile(QStringLiteral("Amber")));

        // A second instance (second window, dev probe) loads Amber here. The
        // writer then saves a new profile; the stale save must not drop it.
        CrtSettings stale;
        stale.setStorePath(store);
        QVERIFY(stale.load());
        QVERIFY(stale.profileExists(QStringLiteral("Amber")));

        writer.setBloom(0.2);
        QVERIFY(writer.saveProfile(QStringLiteral("Xadium")));
        stale.setBloom(0.9);
        QVERIFY(stale.save());

        CrtSettings check;
        check.setStorePath(store);
        QVERIFY(check.load());
        QVERIFY(check.profileExists(QStringLiteral("Amber")));
        QVERIFY(check.profileExists(QStringLiteral("Xadium")));

        // A deliberate delete is still honoured.
        QVERIFY(stale.deleteProfile(QStringLiteral("Amber")));
        CrtSettings afterDelete;
        afterDelete.setStorePath(store);
        QVERIFY(afterDelete.load());
        QVERIFY(!afterDelete.profileExists(QStringLiteral("Amber")));
        QVERIFY(afterDelete.profileExists(QStringLiteral("Xadium")));
    }

    void deleting_the_selected_profile_clears_the_selection() {
        CrtSettings s;
        QVERIFY(s.saveProfile(QStringLiteral("Amber")));
        QCOMPARE(s.currentProfile(), QStringLiteral("Amber"));
        QVERIFY(s.deleteProfile(QStringLiteral("Amber")));
        QCOMPARE(s.currentProfile(), QString());
    }

    // --- Dialog controls ---------------------------------------------------

    void dialog_exposes_every_pipeline_control() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        const char* spins[] = {
            "scanlinePitchSpin", "rasterIntensitySpin", "glowSpin", "burnInSpin",
            "glowLineSpin", "noiseSpin", "brightnessSpin", "contrastSpin",
            "ambientSpin", "flickerSpin", "hSyncSpin", "motionSpin", "rgbShiftSpin",
            "glassSheenSpin", "chromaSpin", "saturationSpin", "bloomQualitySpin",
            "burnInQualitySpin",
        };
        for (const char* name : spins) {
            QVERIFY2(dlg.findChild<TvSpinBox*>(QString::fromLatin1(name)),
                     qPrintable(QStringLiteral("missing %1").arg(name)));
        }
        const char* checks[] = {
            "effectsEnabledCheck", "scanlinesCheck", "glowCheck", "burnInCheck",
            "flickerCheck", "motionCheck", "phosphorCheck",
        };
        for (const char* name : checks) {
            QVERIFY2(dlg.findChild<TvCheckBox*>(QString::fromLatin1(name)),
                     qPrintable(QStringLiteral("missing %1").arg(name)));
        }
        QVERIFY(dlg.findChild<TvSelect*>(QStringLiteral("rasterizationSelect")));
        QVERIFY(dlg.findChild<TvSelect*>(QStringLiteral("profileSelect")));
        QVERIFY(dlg.findChild<TvLineEdit*>(QStringLiteral("profileNameEdit")));
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("profileSaveButton")));
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("profileDeleteButton")));
        QVERIFY(dlg.findChild<TvColorSwatch*>(QStringLiteral("fontSwatch")));
        QVERIFY(dlg.findChild<TvColorSwatch*>(QStringLiteral("bgSwatch")));
        // The three screen-shape controls were removed on request 2026-11-26.
        QVERIFY(!dlg.findChild<TvCheckBox*>(QStringLiteral("curvatureCheck")));
        QVERIFY(!dlg.findChild<TvSpinBox*>(QStringLiteral("curvatureSpin")));
        QVERIFY(!dlg.findChild<TvSpinBox*>(QStringLiteral("screenRadiusSpin")));
        QVERIFY(!dlg.findChild<TvSpinBox*>(QStringLiteral("screenInsetSpin")));
    }

    void tvselect_changes_rasterization() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        auto* raster = dlg.findChild<TvSelect*>(QStringLiteral("rasterizationSelect"));
        QVERIFY(raster);
        QCOMPARE(raster->currentValue(), QStringLiteral("1"));

        raster->setCurrentValue(QStringLiteral("2"));
        QCOMPARE(s.rasterization(), CrtSettings::PixelRasterization);
        raster->setCurrentValue(QStringLiteral("0"));
        QCOMPARE(s.rasterization(), CrtSettings::NoRasterization);
        QVERIFY(!s.scanlinesEnabled());
    }

    // Reported live: the Scanlines box was checked but Intensity still showed
    // 0.00. The toggle raises the model's intensity; the field must follow.
    void scanlines_toggle_refreshes_the_intensity_field() {
        CrtSettings s;
        s.setScanlinesEnabled(false);
        SettingsDialog dlg(&s);
        auto* check = dlg.findChild<TvCheckBox*>(QStringLiteral("scanlinesCheck"));
        auto* spin = dlg.findChild<TvSpinBox*>(QStringLiteral("rasterIntensitySpin"));
        QVERIFY(check && spin);
        QCOMPARE(spin->value(), 0.0);

        check->setChecked(true);
        QVERIFY(s.scanlinesEnabled());
        QCOMPARE(spin->value(), s.rasterizationIntensity());
        QVERIFY(spin->value() > 0.0);

        check->setChecked(false);
        QVERIFY(!s.scanlinesEnabled());
        QCOMPARE(spin->value(), 0.0);
    }

    void master_toggle_bypasses_without_losing_the_look() {
        CrtSettings s;
        s.setBloom(0.66);
        s.setScanlinePitch(4);
        SettingsDialog dlg(&s);
        auto* check = dlg.findChild<TvCheckBox*>(QStringLiteral("effectsEnabledCheck"));
        QVERIFY(check);
        QVERIFY(check->isChecked());

        check->setChecked(false);
        QVERIFY(!s.effectsEnabled());
        QCOMPARE(s.bloom(), 0.66);
        QCOMPARE(s.scanlinePitch(), 4);

        check->setChecked(true);
        QVERIFY(s.effectsEnabled());
        QCOMPARE(s.bloom(), 0.66);
    }

    void settings_modal_buttons_use_black_ink() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        const auto plates = dlg.findChildren<TvPlate*>(QStringLiteral("tvModalButton"));
        QCOMPARE(plates.size(), 2);
        for (auto* plate : plates)
            QCOMPARE(plate->ink(), QColor(Qt::black));

        // The profile plates sit on the TV blue and must use the same face,
        // black ink and 4 px block shadow as OK/Cancel.
        auto* save = dlg.findChild<TvPlate*>(QStringLiteral("profileSaveButton"));
        auto* remove = dlg.findChild<TvPlate*>(QStringLiteral("profileDeleteButton"));
        QVERIFY(save && remove);
        for (auto* plate : { save, remove }) {
            QCOMPARE(plate->ink(), QColor(Qt::black));
            QCOMPARE(plate->backdrop(), QColor(0x00, 0x00, 0xAA));
            QCOMPARE(plate->shadowOffset(), 4);
            QCOMPARE(plate->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
            QCOMPARE(plate->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
            QCOMPARE(plate->minimumSize(), QSize(96, 26));
            QCOMPARE(plate->maximumSize(), QSize(96, 26));
        }
    }

    void colour_swatches_show_and_pick_the_phosphor_colours() {
        CrtSettings s;
        s.setFontColor(QColor(0x12, 0x34, 0x56));
        s.setBackgroundColor(QColor(0x01, 0x02, 0x03));
        SettingsDialog dlg(&s);
        dlg.resize(dlg.sizeHint());
        dlg.show();

        auto* fontSwatch = dlg.findChild<TvColorSwatch*>(QStringLiteral("fontSwatch"));
        auto* bgSwatch = dlg.findChild<TvColorSwatch*>(QStringLiteral("bgSwatch"));
        QVERIFY(fontSwatch && bgSwatch);
        QCOMPARE(fontSwatch->color(), QColor(0x12, 0x34, 0x56));
        QCOMPARE(bgSwatch->color(), QColor(0x01, 0x02, 0x03));

        // The click opens the in-window picker; this timer answers it with a
        // new colour as soon as the nested round loop starts.
        QTimer::singleShot(0, &dlg, [&dlg] {
            auto* picker = dlg.findChild<ColorPickerModal*>();
            QVERIFY2(picker, "clicking the swatch did not open ColorPickerModal");
            picker->panel()->setColor(QColor(0xab, 0xcd, 0xef));
            picker->accept();
        });
        fontSwatch->click();

        QCOMPARE(s.fontColor(), QColor(0xab, 0xcd, 0xef));
        QCOMPARE(fontSwatch->color(), QColor(0xab, 0xcd, 0xef));
    }

    void profile_select_refreshes_the_effect_fields() {
        CrtSettings s;
        s.setBloom(0.2);
        s.setContrast(1.4);
        QVERIFY(s.saveProfile(QStringLiteral("Amber")));
        s.setBloom(0.9);
        s.setContrast(0.9);

        SettingsDialog dlg(&s);
        auto* select = dlg.findChild<TvSelect*>(QStringLiteral("profileSelect"));
        auto* bloomSpin = dlg.findChild<TvSpinBox*>(QStringLiteral("glowSpin"));
        auto* contrastSpin = dlg.findChild<TvSpinBox*>(QStringLiteral("contrastSpin"));
        QVERIFY(select && bloomSpin && contrastSpin);
        QCOMPARE(bloomSpin->value(), 0.9);

        // Saving left "Amber" selected; move to Custom so re-picking it fires.
        select->setCurrentValue(QString());
        select->setCurrentValue(QStringLiteral("Amber"));
        QCOMPARE(s.bloom(), 0.2);
        QCOMPARE(bloomSpin->value(), 0.2);
        QCOMPARE(contrastSpin->value(), 1.4);
    }

    void dialog_cancel_restores_every_effect_value() {
        CrtSettings s;
        s.setContrast(0.9);
        s.setBrightness(0.6);
        s.setAmbientLight(0.3);
        s.setStaticNoise(0.2);
        s.setScreenCurvature(0.1);
        s.setGlowingLine(0.15);
        s.setBurnIn(0.2);
        s.setBloom(0.3);
        s.setChromaColor(0.1);
        s.setSaturationColor(0.2);
        s.setJitter(0.25);
        s.setHorizontalSync(0.05);
        s.setFlickering(0.07);
        s.setRgbShift(0.04);
        s.setSweep(0.08);
        s.setRasterizationIntensity(0.33);
        s.setGlassSheen(0.12);
        s.setScreenRadius(0.14);
        s.setScreenInset(0.16);
        s.setBloomQuality(0.44);
        s.setBurnInQuality(0.55);
        s.setRasterization(CrtSettings::SubpixelRasterization);
        s.setScanlinePitch(3);
        s.setPhosphorEnabled(true);
        s.setEffectsEnabled(true);

        SettingsDialog dlg(&s);

        // Live edits a user could make through the open dialog.
        s.setContrast(1.9);
        s.setBrightness(0.9);
        s.setAmbientLight(0.9);
        s.setStaticNoise(0.9);
        s.setScreenCurvature(0.9);
        s.setGlowingLine(0.9);
        s.setBurnIn(0.9);
        s.setBloom(0.9);
        s.setChromaColor(0.9);
        s.setSaturationColor(0.9);
        s.setJitter(0.9);
        s.setHorizontalSync(0.9);
        s.setFlickering(0.9);
        s.setRgbShift(0.9);
        s.setSweep(0.9);
        s.setRasterizationIntensity(0.9);
        s.setGlassSheen(0.9);
        s.setScreenRadius(0.9);
        s.setScreenInset(0.9);
        s.setBloomQuality(0.9);
        s.setBurnInQuality(0.9);
        s.setRasterization(CrtSettings::NoRasterization);
        s.setScanlinePitch(4);
        s.setPhosphorEnabled(false);
        s.setEffectsEnabled(false);

        dlg.reject();

        QCOMPARE(s.contrast(), 0.9);
        QCOMPARE(s.brightness(), 0.6);
        QCOMPARE(s.ambientLight(), 0.3);
        QCOMPARE(s.staticNoise(), 0.2);
        QCOMPARE(s.screenCurvature(), 0.1);
        QCOMPARE(s.glowingLine(), 0.15);
        QCOMPARE(s.burnIn(), 0.2);
        QCOMPARE(s.bloom(), 0.3);
        QCOMPARE(s.chromaColor(), 0.1);
        QCOMPARE(s.saturationColor(), 0.2);
        QCOMPARE(s.jitter(), 0.25);
        QCOMPARE(s.horizontalSync(), 0.05);
        QCOMPARE(s.flickering(), 0.07);
        QCOMPARE(s.rgbShift(), 0.04);
        QCOMPARE(s.sweep(), 0.08);
        QCOMPARE(s.rasterizationIntensity(), 0.33);
        QCOMPARE(s.glassSheen(), 0.12);
        QCOMPARE(s.screenRadius(), 0.14);
        QCOMPARE(s.screenInset(), 0.16);
        QCOMPARE(s.bloomQuality(), 0.44);
        QCOMPARE(s.burnInQuality(), 0.55);
        QCOMPARE(s.rasterization(), CrtSettings::SubpixelRasterization);
        QCOMPARE(s.scanlinePitch(), 3);
        QCOMPARE(s.phosphorEnabled(), true);
        QCOMPARE(s.effectsEnabled(), true);
    }

    void profile_dialog_saves_and_applies_profiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CrtSettings s;
        s.setStorePath(dir.filePath("crt-kate.ini"));

        SettingsDialog dlg(&s);
        auto* select = dlg.findChild<TvSelect*>(QStringLiteral("profileSelect"));
        auto* edit = dlg.findChild<TvLineEdit*>(QStringLiteral("profileNameEdit"));
        auto* save = dlg.findChild<QPushButton*>(QStringLiteral("profileSaveButton"));
        auto* remove = dlg.findChild<QPushButton*>(QStringLiteral("profileDeleteButton"));
        QVERIFY(select && edit && save && remove);

        s.setBloom(0.2);
        s.setScanlinePitch(1);
        edit->setText(QStringLiteral("Amber"));
        save->click();
        QVERIFY(s.profileExists(QStringLiteral("Amber")));

        s.setBloom(0.9);
        s.setScanlinePitch(4);
        edit->setText(QStringLiteral("Green"));
        save->click();
        QVERIFY(s.profileExists(QStringLiteral("Green")));

        // Both profiles are offered in the selector.
        QVERIFY(select->count() >= 3);   // Custom + Amber + Green

        // Picking a profile from the selector applies it live.
        select->setCurrentValue(QStringLiteral("Amber"));
        QCOMPARE(s.bloom(), 0.2);
        QCOMPARE(s.scanlinePitch(), 1);
        select->setCurrentValue(QStringLiteral("Green"));
        QCOMPARE(s.bloom(), 0.9);
        QCOMPARE(s.scanlinePitch(), 4);

        // Delete removes it from the selector and from the store.
        select->setCurrentValue(QStringLiteral("Amber"));
        remove->click();
        QVERIFY(!s.profileExists(QStringLiteral("Amber")));
        QVERIFY(select->count() >= 2);
    }

    // The CRT tab must fit an 800 px-tall window without clipping.
    void crt_tab_fits_an_800px_window() {
        CrtSettings s;
        SettingsDialog dlg(&s);
        QVERIFY2(dlg.sizeHint().height() <= 800,
                 qPrintable(QStringLiteral("settings dialog wants %1 px")
                                .arg(dlg.sizeHint().height())));
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