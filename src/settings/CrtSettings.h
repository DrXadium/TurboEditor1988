// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>
#include <QSet>
#include <QTimer>

// C++ mirror of cool-retro-term's ApplicationSettings effect model (Task 2's
// QML CrtSettings), with QSettings persistence and NOTIFY signals so a QML
// EffectChain can bind live to it. Single source of truth for CRT knobs.
class CrtSettings : public QObject
{
    Q_OBJECT
public:
    enum Rasterization {
        NoRasterization = 0,
        ScanlineRasterization = 1,
        PixelRasterization = 2,
        SubpixelRasterization = 3,
        ModernRasterization = 4,
    };
    Q_ENUM(Rasterization)

    explicit CrtSettings(QObject* parent = nullptr);

    // --- Effect floats (defaults from cool-retro-term ApplicationSettings) ---
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY contrastChanged)
    Q_PROPERTY(double brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(double ambientLight READ ambientLight WRITE setAmbientLight NOTIFY ambientLightChanged)
    Q_PROPERTY(double staticNoise READ staticNoise WRITE setStaticNoise NOTIFY staticNoiseChanged)
    Q_PROPERTY(double screenCurvature READ screenCurvature WRITE setScreenCurvature NOTIFY screenCurvatureChanged)
    Q_PROPERTY(double glowingLine READ glowingLine WRITE setGlowingLine NOTIFY glowingLineChanged)
    Q_PROPERTY(double burnIn READ burnIn WRITE setBurnIn NOTIFY burnInChanged)
    Q_PROPERTY(double bloom READ bloom WRITE setBloom NOTIFY bloomChanged)
    Q_PROPERTY(double chromaColor READ chromaColor WRITE setChromaColor NOTIFY chromaColorChanged)
    Q_PROPERTY(double saturationColor READ saturationColor WRITE setSaturationColor NOTIFY saturationColorChanged)
    Q_PROPERTY(double jitter READ jitter WRITE setJitter NOTIFY jitterChanged)
    Q_PROPERTY(double horizontalSync READ horizontalSync WRITE setHorizontalSync NOTIFY horizontalSyncChanged)
    Q_PROPERTY(double flickering READ flickering WRITE setFlickering NOTIFY flickeringChanged)
    Q_PROPERTY(double rgbShift READ rgbShift WRITE setRgbShift NOTIFY rgbShiftChanged)
    Q_PROPERTY(double sweep READ sweep WRITE setSweep NOTIFY sweepChanged)

    // Master bypass (user request 2026-11-26): off unloads the whole effect
    // chain; the values below are untouched, so turning it back on restores
    // exactly the look that was selected. Not part of a named profile.
    Q_PROPERTY(bool effectsEnabled READ effectsEnabled WRITE setEffectsEnabled
                   NOTIFY effectsEnabledChanged)

    Q_PROPERTY(Rasterization rasterization READ rasterization WRITE setRasterization NOTIFY rasterizationChanged)
    Q_PROPERTY(double rasterizationIntensity READ rasterizationIntensity WRITE setRasterizationIntensity NOTIFY rasterizationIntensityChanged)

    // --- Four-stage pipeline knobs (2026-11-26 spec) -----------------------
    Q_PROPERTY(bool phosphorEnabled READ phosphorEnabled WRITE setPhosphorEnabled NOTIFY phosphorEnabledChanged)
    Q_PROPERTY(int scanlinePitch READ scanlinePitch WRITE setScanlinePitch NOTIFY scanlinePitchChanged)
    Q_PROPERTY(double glassSheen READ glassSheen WRITE setGlassSheen NOTIFY glassSheenChanged)
    Q_PROPERTY(double screenRadius READ screenRadius WRITE setScreenRadius NOTIFY screenRadiusChanged)
    Q_PROPERTY(double screenInset READ screenInset WRITE setScreenInset NOTIFY screenInsetChanged)
    Q_PROPERTY(double bloomQuality READ bloomQuality WRITE setBloomQuality NOTIFY bloomQualityChanged)
    Q_PROPERTY(double burnInQuality READ burnInQuality WRITE setBurnInQuality NOTIFY burnInQualityChanged)

    // --- Named effect profiles ---------------------------------------------
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile WRITE setCurrentProfile NOTIFY currentProfileChanged)

    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor NOTIFY fontColorChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(int autosaveIntervalSeconds READ autosaveIntervalSeconds
                   WRITE setAutosaveIntervalSeconds NOTIFY autosaveIntervalSecondsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    // View > Show Minimap survives a restart (user request 2026-11-21).
    Q_PROPERTY(bool minimapVisible READ minimapVisible WRITE setMinimapVisible
                   NOTIFY minimapVisibleChanged)
    // File > Recent Files (user request 2026-11-24): the last five opened files
    // on the File dropdown, ON unless the user turns it off.
    Q_PROPERTY(bool showRecentFiles READ showRecentFiles WRITE setShowRecentFiles
                   NOTIFY showRecentFilesChanged)

    // --- Per-effect on/off accessors (map to the float model) ---
    Q_PROPERTY(bool scanlinesEnabled READ scanlinesEnabled WRITE setScanlinesEnabled NOTIFY scanlinesEnabledChanged)
    Q_PROPERTY(bool curvatureEnabled READ curvatureEnabled NOTIFY scanlinesEnabledChanged)
    Q_PROPERTY(bool glowEnabled READ glowEnabled NOTIFY bloomChanged)
    Q_PROPERTY(bool flickerEnabled READ flickerEnabled NOTIFY flickeringChanged)
    Q_PROPERTY(bool motionEnabled READ motionEnabled NOTIFY jitterChanged)
    Q_PROPERTY(bool burnInEnabled READ burnInEnabled NOTIFY burnInChanged)
    Q_PROPERTY(bool horizontalSyncEnabled READ horizontalSyncEnabled NOTIFY horizontalSyncChanged)
    Q_PROPERTY(bool rgbShiftEnabled READ rgbShiftEnabled NOTIFY rgbShiftChanged)
    Q_PROPERTY(bool sheenEnabled READ sheenEnabled NOTIFY glassSheenChanged)

    // Getters
    double contrast() const { return m_contrast; }
    double brightness() const { return m_brightness; }
    double ambientLight() const { return m_ambientLight; }
    double staticNoise() const { return m_staticNoise; }
    double screenCurvature() const { return m_screenCurvature; }
    double glowingLine() const { return m_glowingLine; }
    double burnIn() const { return m_burnIn; }
    double bloom() const { return m_bloom; }
    double chromaColor() const { return m_chromaColor; }
    double saturationColor() const { return m_saturationColor; }
    double jitter() const { return m_jitter; }
    double horizontalSync() const { return m_horizontalSync; }
    double flickering() const { return m_flickering; }
    double rgbShift() const { return m_rgbShift; }
    double sweep() const { return m_sweep; }
    bool effectsEnabled() const { return m_effectsEnabled; }
    Rasterization rasterization() const { return m_rasterization; }
    double rasterizationIntensity() const { return m_rasterizationIntensity; }
    bool phosphorEnabled() const { return m_phosphorEnabled; }
    int scanlinePitch() const { return m_scanlinePitch; }
    double glassSheen() const { return m_glassSheen; }
    double screenRadius() const { return m_screenRadius; }
    double screenInset() const { return m_screenInset; }
    double bloomQuality() const { return m_bloomQuality; }
    double burnInQuality() const { return m_burnInQuality; }
    QColor fontColor() const { return m_fontColor; }
    int fontSize() const { return m_fontSize; }
    int autosaveIntervalSeconds() const { return m_autosaveIntervalSeconds; }
    QColor backgroundColor() const { return m_backgroundColor; }
    bool minimapVisible() const { return m_minimapVisible; }
    void setMinimapVisible(bool on);
    bool showRecentFiles() const { return m_showRecentFiles; }
    void setShowRecentFiles(bool on);

    bool scanlinesEnabled() const;
    bool curvatureEnabled() const { return m_screenCurvature > 0; }
    bool glowEnabled() const { return m_bloom > 0 || m_glowingLine > 0; }
    bool flickerEnabled() const { return m_flickering > 0; }
    bool motionEnabled() const { return m_jitter > 0 || m_horizontalSync > 0; }
    bool burnInEnabled() const { return m_burnIn > 0; }
    bool horizontalSyncEnabled() const { return m_horizontalSync > 0; }
    bool rgbShiftEnabled() const { return m_rgbShift > 0; }
    bool sheenEnabled() const { return m_glassSheen > 0; }

    // --- Named effect profiles ---------------------------------------------
    QStringList profileNames() const;
    QString currentProfile() const { return m_currentProfile; }
    void setCurrentProfile(const QString& name);   // selection only, no apply
    Q_INVOKABLE bool profileExists(const QString& name) const;
    Q_INVOKABLE bool saveProfile(const QString& name);
    Q_INVOKABLE bool applyProfile(const QString& name);
    Q_INVOKABLE bool deleteProfile(const QString& name);

public slots:
    // Setters (each emits only on real change).
    void setContrast(double v);
    void setBrightness(double v);
    void setAmbientLight(double v);
    void setStaticNoise(double v);
    void setScreenCurvature(double v);
    void setGlowingLine(double v);
    void setBurnIn(double v);
    void setBloom(double v);
    void setChromaColor(double v);
    void setSaturationColor(double v);
    void setJitter(double v);
    void setHorizontalSync(double v);
    void setFlickering(double v);
    void setRgbShift(double v);
    void setSweep(double v);
    void setEffectsEnabled(bool on);
    void setRasterization(Rasterization v);
    void setRasterizationIntensity(double v);
    void setPhosphorEnabled(bool on);
    void setScanlinePitch(int v);
    void setGlassSheen(double v);
    void setScreenRadius(double v);
    void setScreenInset(double v);
    void setBloomQuality(double v);
    void setBurnInQuality(double v);
    void setFontColor(QColor v);
    void setFontSize(int v);
    void setAutosaveIntervalSeconds(int v);
    void setBackgroundColor(QColor v);

    // Per-effect toggles: off sets the driver float (or rasterization) to its
    // "disabled" state; on restores a sensible non-zero default.
    void setScanlinesEnabled(bool on);

    // Dev aid: CRT_KATE_BRIGHT temporarily overrides brightness for a capture.
    // It must NOT persist — restoreDevBrightness() puts the loaded value back
    // so a dev session can't contaminate the user's saved settings.
    void applyDevBrightness(double v);
    void restoreDevBrightness();

    // Persistence (QSettings). Default store: ~/.config/crt-kate/crt.ini.
    void setStorePath(const QString& path);
    QString storePath() const;   // current store location (public: shown in
                                 // the load-failure warning)
    bool load();
    bool save();

signals:
    void contrastChanged();
    void brightnessChanged();
    void ambientLightChanged();
    void staticNoiseChanged();
    void screenCurvatureChanged();
    void glowingLineChanged();
    void burnInChanged();
    void bloomChanged();
    void chromaColorChanged();
    void saturationColorChanged();
    void jitterChanged();
    void horizontalSyncChanged();
    void flickeringChanged();
    void rgbShiftChanged();
    void sweepChanged();
    void effectsEnabledChanged();
    void rasterizationChanged();
    void rasterizationIntensityChanged();
    void phosphorEnabledChanged();
    void scanlinePitchChanged();
    void glassSheenChanged();
    void screenRadiusChanged();
    void screenInsetChanged();
    void bloomQualityChanged();
    void burnInQualityChanged();
    void profilesChanged();
    void currentProfileChanged();
    void fontColorChanged();
    void fontSizeChanged();
    void autosaveIntervalSecondsChanged();
    void backgroundColorChanged();
    void minimapVisibleChanged();
    void showRecentFilesChanged();
    void scanlinesEnabledChanged();

private:
    void resetToDefaults();
    QVariantMap effectValues() const;
    void applyEffectValues(const QVariantMap& values);
    // Coalesced write of the live values (250 ms) so the last chosen effect set
    // survives an exit that never reaches aboutToQuit (SIGTERM, a crash, a
    // killed test instance).
    void scheduleSave();

    double m_contrast;
    double m_brightness;
    double m_ambientLight;
    double m_staticNoise;
    double m_screenCurvature;
    double m_glowingLine;
    double m_burnIn;
    double m_bloom;
    double m_chromaColor;
    double m_saturationColor;
    double m_jitter;
    double m_horizontalSync;
    double m_flickering;
    double m_rgbShift;
    double m_sweep;
    bool m_effectsEnabled;
    Rasterization m_rasterization;
    double m_rasterizationIntensity;
    bool m_phosphorEnabled;
    int m_scanlinePitch;
    double m_glassSheen;
    double m_screenRadius;
    double m_screenInset;
    double m_bloomQuality;
    double m_burnInQuality;
    QColor m_fontColor;
    int m_fontSize;
    int m_autosaveIntervalSeconds;
    QColor m_backgroundColor;
    bool m_minimapVisible = false;   // off unless the user turned it on
    bool m_showRecentFiles = true;   // the File menu's Recent Files submenu
    QMap<QString, QVariantMap> m_profiles;   // name -> effect values
    // Names deleted in THIS instance; a stale instance must not resurrect
    // them when save() merges profiles written since it loaded.
    QSet<QString> m_deletedProfiles;
    QString m_currentProfile;
    QString m_storePath;   // empty => default QSettings location
    double m_savedBrightness = -1.0;   // brightness before applyDevBrightness (>=0 when active)

    QTimer m_saveTimer;
    bool m_loading = false;
};