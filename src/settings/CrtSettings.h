// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QColor>
#include <QString>

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

    Q_PROPERTY(Rasterization rasterization READ rasterization WRITE setRasterization NOTIFY rasterizationChanged)
    Q_PROPERTY(double rasterizationIntensity READ rasterizationIntensity WRITE setRasterizationIntensity NOTIFY rasterizationIntensityChanged)

    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor NOTIFY fontColorChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)

    // --- Per-effect on/off accessors (map to the float model) ---
    Q_PROPERTY(bool scanlinesEnabled READ scanlinesEnabled WRITE setScanlinesEnabled NOTIFY scanlinesEnabledChanged)
    Q_PROPERTY(bool curvatureEnabled READ curvatureEnabled NOTIFY scanlinesEnabledChanged)
    Q_PROPERTY(bool glowEnabled READ glowEnabled NOTIFY bloomChanged)
    Q_PROPERTY(bool flickerEnabled READ flickerEnabled NOTIFY flickeringChanged)
    Q_PROPERTY(bool motionEnabled READ motionEnabled NOTIFY jitterChanged)

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
    Rasterization rasterization() const { return m_rasterization; }
    double rasterizationIntensity() const { return m_rasterizationIntensity; }
    QColor fontColor() const { return m_fontColor; }
    int fontSize() const { return m_fontSize; }
    QColor backgroundColor() const { return m_backgroundColor; }

    bool scanlinesEnabled() const;
    bool curvatureEnabled() const { return m_screenCurvature > 0; }
    bool glowEnabled() const { return m_bloom > 0 || m_glowingLine > 0; }
    bool flickerEnabled() const { return m_flickering > 0; }
    bool motionEnabled() const { return m_jitter > 0 || m_horizontalSync > 0; }

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
    void setRasterization(Rasterization v);
    void setRasterizationIntensity(double v);
    void setFontColor(QColor v);
    void setFontSize(int v);
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
    void rasterizationChanged();
    void rasterizationIntensityChanged();
    void fontColorChanged();
    void fontSizeChanged();
    void backgroundColorChanged();
    void scanlinesEnabledChanged();

private:
    void resetToDefaults();

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
    Rasterization m_rasterization;
    double m_rasterizationIntensity;
    QColor m_fontColor;
    int m_fontSize;
    QColor m_backgroundColor;
    QString m_storePath;   // empty => default QSettings location
    double m_savedBrightness = -1.0;   // brightness before applyDevBrightness (>=0 when active)
};