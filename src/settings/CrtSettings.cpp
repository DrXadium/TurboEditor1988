// SPDX-License-Identifier: GPL-3.0-or-later
#include "CrtSettings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include "src/AppIdentity.h"

namespace {
// cool-retro-term ApplicationSettings.qml defaults.
constexpr double kContrast = 0.80;
constexpr double kBrightness = 0.5;
constexpr double kAmbientLight = 0.2;
constexpr double kStaticNoise = 0.12;
constexpr double kScreenCurvature = 0.0;   // flat viewport by default (user:
                                           // no curved-CRT gimmick; scanlines/
                                           // bloom/texture still on)
constexpr double kGlowingLine = 0.2;
constexpr double kBurnIn = 0.25;
constexpr double kBloom = 0.55;
constexpr double kChromaColor = 0.25;
constexpr double kSaturationColor = 0.25;
constexpr double kJitter = 0.2;
constexpr double kHorizontalSync = 0.08;
constexpr double kFlickering = 0.1;
constexpr double kRgbShift = 0.0;
constexpr double kRasterIntensity = 0.22;
const auto kFontColor = QColor(QStringLiteral("#ff8100"));
// TV default editor font (spec: 16px DOS-cell proportions).
const int kFontSize = 16;
const auto kBackgroundColor = QColor(QStringLiteral("#000000"));

// Non-zero values used when re-enabling an effect toggle.
constexpr double kScanlineIntensityOn = 0.22;
constexpr auto kScanlineRasterOn = CrtSettings::ScanlineRasterization;
} // namespace

CrtSettings::CrtSettings(QObject* parent) : QObject(parent)
{
    resetToDefaults();
}

void CrtSettings::resetToDefaults()
{
    m_contrast = kContrast;
    m_brightness = kBrightness;
    m_ambientLight = kAmbientLight;
    m_staticNoise = kStaticNoise;
    m_screenCurvature = kScreenCurvature;
    m_glowingLine = kGlowingLine;
    m_burnIn = kBurnIn;
    m_bloom = kBloom;
    m_chromaColor = kChromaColor;
    m_saturationColor = kSaturationColor;
    m_jitter = kJitter;
    m_horizontalSync = kHorizontalSync;
    m_flickering = kFlickering;
    m_rgbShift = kRgbShift;
    m_rasterization = ScanlineRasterization;
    m_rasterizationIntensity = kRasterIntensity;
    m_fontColor = kFontColor;
    m_fontSize = kFontSize;
    m_backgroundColor = kBackgroundColor;
}

bool CrtSettings::scanlinesEnabled() const
{
    return m_rasterization != NoRasterization && m_rasterizationIntensity > 0.0;
}

// --- setters (emit only on change) ---
#define CRT_SET(type, name, member, sig)                    \
    void CrtSettings::set##name(type v)                     \
    {                                                       \
        if (member == v) return;                            \
        member = v;                                         \
        emit sig();                                         \
    }

CRT_SET(double, Contrast, m_contrast, contrastChanged)
CRT_SET(double, Brightness, m_brightness, brightnessChanged)
CRT_SET(double, AmbientLight, m_ambientLight, ambientLightChanged)
CRT_SET(double, StaticNoise, m_staticNoise, staticNoiseChanged)
CRT_SET(double, ScreenCurvature, m_screenCurvature, screenCurvatureChanged)
CRT_SET(double, GlowingLine, m_glowingLine, glowingLineChanged)
CRT_SET(double, BurnIn, m_burnIn, burnInChanged)
CRT_SET(double, Bloom, m_bloom, bloomChanged)
CRT_SET(double, ChromaColor, m_chromaColor, chromaColorChanged)
CRT_SET(double, SaturationColor, m_saturationColor, saturationColorChanged)
CRT_SET(double, Jitter, m_jitter, jitterChanged)
CRT_SET(double, HorizontalSync, m_horizontalSync, horizontalSyncChanged)
CRT_SET(double, Flickering, m_flickering, flickeringChanged)
CRT_SET(double, RgbShift, m_rgbShift, rgbShiftChanged)
CRT_SET(Rasterization, Rasterization, m_rasterization, rasterizationChanged)
CRT_SET(double, RasterizationIntensity, m_rasterizationIntensity, rasterizationIntensityChanged)
CRT_SET(QColor, FontColor, m_fontColor, fontColorChanged)
CRT_SET(int, FontSize, m_fontSize, fontSizeChanged)
CRT_SET(QColor, BackgroundColor, m_backgroundColor, backgroundColorChanged)
#undef CRT_SET

void CrtSettings::setScanlinesEnabled(bool on)
{
    if (on) {
        if (m_rasterization == NoRasterization)
            m_rasterization = kScanlineRasterOn;
        if (m_rasterizationIntensity <= 0.0)
            m_rasterizationIntensity = kScanlineIntensityOn;
        emit rasterizationChanged();
        emit rasterizationIntensityChanged();
    } else {
        // Keep rasterization mode but zero intensity (matches EffectChain's
        // scanlinesEnabled gating: rasterization != none && intensity > 0).
        if (m_rasterizationIntensity != 0.0) {
            m_rasterizationIntensity = 0.0;
            emit rasterizationIntensityChanged();
        }
    }
    emit scanlinesEnabledChanged();
}

void CrtSettings::setStorePath(const QString& path)
{
    m_storePath = path;
}

QString CrtSettings::storePath() const
{
    if (!m_storePath.isEmpty())
        return m_storePath;
    // Pinned to the LEGACY storage id (not AppConfigLocation): preferences
    // already live at ~/.config/CRT-Kate/crt.ini and are not migrated, so
    // following the "TURBO Kate" display name would silently lose them.
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QLatin1Char('/') + AppIdentity::storageDirName();
    QDir().mkpath(dir);
    return dir + QStringLiteral("/crt.ini");
}

bool CrtSettings::load()
{
    QSettings s(storePath(), QSettings::IniFormat);
    // Unreadable/corrupt store: report the failure BEFORE touching any member
    // so current values (defaults on a fresh instance) survive intact — and the
    // caller can decide to warn instead of silently dropping saved settings.
    if (s.status() != QSettings::NoError) {
        s.clear();   // clear the sticky error so later saves still work
        return false;
    }
    s.beginGroup(QStringLiteral("crt"));
    m_contrast = s.value(QStringLiteral("contrast"), kContrast).toDouble();
    m_brightness = s.value(QStringLiteral("brightness"), kBrightness).toDouble();
    m_ambientLight = s.value(QStringLiteral("ambientLight"), kAmbientLight).toDouble();
    m_staticNoise = s.value(QStringLiteral("staticNoise"), kStaticNoise).toDouble();
    m_screenCurvature = s.value(QStringLiteral("screenCurvature"), kScreenCurvature).toDouble();
    m_glowingLine = s.value(QStringLiteral("glowingLine"), kGlowingLine).toDouble();
    m_burnIn = s.value(QStringLiteral("burnIn"), kBurnIn).toDouble();
    m_bloom = s.value(QStringLiteral("bloom"), kBloom).toDouble();
    m_chromaColor = s.value(QStringLiteral("chromaColor"), kChromaColor).toDouble();
    m_saturationColor = s.value(QStringLiteral("saturationColor"), kSaturationColor).toDouble();
    m_jitter = s.value(QStringLiteral("jitter"), kJitter).toDouble();
    m_horizontalSync = s.value(QStringLiteral("horizontalSync"), kHorizontalSync).toDouble();
    m_flickering = s.value(QStringLiteral("flickering"), kFlickering).toDouble();
    m_rgbShift = s.value(QStringLiteral("rgbShift"), kRgbShift).toDouble();
    m_rasterization = static_cast<Rasterization>(
        s.value(QStringLiteral("rasterization"), ScanlineRasterization).toInt());
    m_rasterizationIntensity = s.value(QStringLiteral("rasterizationIntensity"), kRasterIntensity).toDouble();
    m_fontColor = s.value(QStringLiteral("fontColor"), kFontColor).value<QColor>();
    m_fontSize = s.value(QStringLiteral("fontSize"), kFontSize).toInt();
    m_backgroundColor = s.value(QStringLiteral("backgroundColor"), kBackgroundColor).value<QColor>();
    s.endGroup();

    // Notify all listeners of the bulk reload (values likely changed).
    emit contrastChanged();
    emit brightnessChanged();
    emit ambientLightChanged();
    emit staticNoiseChanged();
    emit screenCurvatureChanged();
    emit glowingLineChanged();
    emit burnInChanged();
    emit bloomChanged();
    emit chromaColorChanged();
    emit saturationColorChanged();
    emit jitterChanged();
    emit horizontalSyncChanged();
    emit flickeringChanged();
    emit rgbShiftChanged();
    emit rasterizationChanged();
    emit rasterizationIntensityChanged();
    emit fontColorChanged();
    emit fontSizeChanged();
    emit backgroundColorChanged();
    emit scanlinesEnabledChanged();
    return true;
}

void CrtSettings::applyDevBrightness(double v)
{
    if (m_savedBrightness < 0.0)
        m_savedBrightness = m_brightness;   // remember the user's value once
    setBrightness(v);
}

void CrtSettings::restoreDevBrightness()
{
    if (m_savedBrightness < 0.0)
        return;
    setBrightness(m_savedBrightness);
    m_savedBrightness = -1.0;
}

bool CrtSettings::save()
{
    QSettings s(storePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("crt"));
    s.setValue(QStringLiteral("contrast"), m_contrast);
    s.setValue(QStringLiteral("brightness"), m_brightness);
    s.setValue(QStringLiteral("ambientLight"), m_ambientLight);
    s.setValue(QStringLiteral("staticNoise"), m_staticNoise);
    s.setValue(QStringLiteral("screenCurvature"), m_screenCurvature);
    s.setValue(QStringLiteral("glowingLine"), m_glowingLine);
    s.setValue(QStringLiteral("burnIn"), m_burnIn);
    s.setValue(QStringLiteral("bloom"), m_bloom);
    s.setValue(QStringLiteral("chromaColor"), m_chromaColor);
    s.setValue(QStringLiteral("saturationColor"), m_saturationColor);
    s.setValue(QStringLiteral("jitter"), m_jitter);
    s.setValue(QStringLiteral("horizontalSync"), m_horizontalSync);
    s.setValue(QStringLiteral("flickering"), m_flickering);
    s.setValue(QStringLiteral("rgbShift"), m_rgbShift);
    s.setValue(QStringLiteral("rasterization"), static_cast<int>(m_rasterization));
    s.setValue(QStringLiteral("rasterizationIntensity"), m_rasterizationIntensity);
    s.setValue(QStringLiteral("fontColor"), m_fontColor);
    s.setValue(QStringLiteral("fontSize"), m_fontSize);
    s.setValue(QStringLiteral("backgroundColor"), m_backgroundColor);
    s.endGroup();
    s.sync();
    return s.status() == QSettings::NoError;
}