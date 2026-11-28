// SPDX-License-Identifier: GPL-3.0-or-later
#include "CrtSettings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
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
// User decision 2026-11-26: keep the UI's own colours by default. Phosphor
// conversion is opt-in, so chroma and saturation start at 0.
constexpr double kChromaColor = 0.0;
constexpr double kSaturationColor = 0.0;
constexpr double kJitter = 0.2;
constexpr double kHorizontalSync = 0.08;
constexpr double kFlickering = 0.1;
constexpr double kRgbShift = 0.0;
constexpr double kSweep = 0.0;
constexpr double kRasterIntensity = 0.22;
// Four-stage pipeline defaults (2026-11-26 spec).
constexpr bool kPhosphorEnabled = false;
constexpr int kScanlinePitch = 2;
constexpr double kGlassSheen = 0.0;
constexpr double kScreenRadius = 0.0;
constexpr double kScreenInset = 0.0;
constexpr double kBloomQuality = 0.5;
constexpr double kBurnInQuality = 0.5;
const auto kFontColor = QColor(QStringLiteral("#ff8100"));
// TV default editor font (spec: 16px DOS-cell proportions).
const int kFontSize = 16;
// Autosave round: default cadence 120 s, bounded by the Settings spin box.
const int kAutosaveIntervalSeconds = 120;
const auto kBackgroundColor = QColor(QStringLiteral("#000000"));

// Non-zero values used when re-enabling an effect toggle.
constexpr double kScanlineIntensityOn = 0.22;
constexpr auto kScanlineRasterOn = CrtSettings::ScanlineRasterization;
} // namespace

CrtSettings::CrtSettings(QObject* parent) : QObject(parent)
{
    resetToDefaults();

    // Persist the live values shortly after any change. The app used to save
    // only in aboutToQuit, so a SIGTERM (the relaunch script, a killed test
    // instance) threw the user's last chosen effect set away. Coalescing keeps
    // a spinner drag to one write, not one per pixel step.
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        if (!m_loading)
            save();
    });
    using Signal = void (CrtSettings::*)();
    const Signal changed[] = {
        &CrtSettings::contrastChanged, &CrtSettings::brightnessChanged,
        &CrtSettings::ambientLightChanged, &CrtSettings::staticNoiseChanged,
        &CrtSettings::screenCurvatureChanged, &CrtSettings::glowingLineChanged,
        &CrtSettings::burnInChanged, &CrtSettings::bloomChanged,
        &CrtSettings::chromaColorChanged, &CrtSettings::saturationColorChanged,
        &CrtSettings::jitterChanged, &CrtSettings::horizontalSyncChanged,
        &CrtSettings::flickeringChanged, &CrtSettings::rgbShiftChanged,
        &CrtSettings::sweepChanged, &CrtSettings::effectsEnabledChanged,
        &CrtSettings::rasterizationChanged,
        &CrtSettings::rasterizationIntensityChanged,
        &CrtSettings::phosphorEnabledChanged, &CrtSettings::scanlinePitchChanged,
        &CrtSettings::glassSheenChanged, &CrtSettings::screenRadiusChanged,
        &CrtSettings::screenInsetChanged, &CrtSettings::bloomQualityChanged,
        &CrtSettings::burnInQualityChanged, &CrtSettings::fontColorChanged,
        &CrtSettings::fontSizeChanged,
        &CrtSettings::autosaveIntervalSecondsChanged,
        &CrtSettings::backgroundColorChanged,
        &CrtSettings::minimapVisibleChanged, &CrtSettings::showRecentFilesChanged,
        &CrtSettings::backupOnOpenChanged,
        &CrtSettings::showFullPathInTitleChanged,
        &CrtSettings::scanlinesEnabledChanged,
    };
    for (Signal signal : changed)
        connect(this, signal, this, &CrtSettings::scheduleSave);
}

void CrtSettings::scheduleSave()
{
    // Dev brightness override is deliberately not persisted; load() also emits
    // every NOTIFY, and those must not trigger a write.
    if (m_loading || m_savedBrightness >= 0.0)
        return;
    m_saveTimer.start();
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
    m_sweep = kSweep;
    m_effectsEnabled = true;
    m_rasterization = ScanlineRasterization;
    m_rasterizationIntensity = kRasterIntensity;
    m_phosphorEnabled = kPhosphorEnabled;
    m_scanlinePitch = kScanlinePitch;
    m_glassSheen = kGlassSheen;
    m_screenRadius = kScreenRadius;
    m_screenInset = kScreenInset;
    m_bloomQuality = kBloomQuality;
    m_burnInQuality = kBurnInQuality;
    m_fontColor = kFontColor;
    m_fontSize = kFontSize;
    m_autosaveIntervalSeconds = kAutosaveIntervalSeconds;
    m_backgroundColor = kBackgroundColor;
    m_minimapVisible = false;
    m_showRecentFiles = true;
    m_backupOnOpen = true;
    m_showFullPathInTitle = false;
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
CRT_SET(double, Sweep, m_sweep, sweepChanged)
CRT_SET(bool, EffectsEnabled, m_effectsEnabled, effectsEnabledChanged)
CRT_SET(bool, PhosphorEnabled, m_phosphorEnabled, phosphorEnabledChanged)
CRT_SET(Rasterization, Rasterization, m_rasterization, rasterizationChanged)
CRT_SET(double, RasterizationIntensity, m_rasterizationIntensity, rasterizationIntensityChanged)
CRT_SET(QColor, FontColor, m_fontColor, fontColorChanged)
CRT_SET(int, FontSize, m_fontSize, fontSizeChanged)
CRT_SET(QColor, BackgroundColor, m_backgroundColor, backgroundColorChanged)
CRT_SET(bool, MinimapVisible, m_minimapVisible, minimapVisibleChanged)
CRT_SET(bool, ShowRecentFiles, m_showRecentFiles, showRecentFilesChanged)
CRT_SET(bool, BackupOnOpen, m_backupOnOpen, backupOnOpenChanged)
CRT_SET(bool, ShowFullPathInTitle, m_showFullPathInTitle, showFullPathInTitleChanged)
// Clamped to the spin box range: 0/negative would mean "never" or a hot loop.
void CrtSettings::setAutosaveIntervalSeconds(int v)
{
    v = std::clamp(v, 1, 3600);
    if (m_autosaveIntervalSeconds == v)
        return;
    m_autosaveIntervalSeconds = v;
    emit autosaveIntervalSecondsChanged();
}
#undef CRT_SET

void CrtSettings::setScanlinePitch(int v)
{
    v = std::clamp(v, 1, 4);
    if (m_scanlinePitch == v)
        return;
    m_scanlinePitch = v;
    emit scanlinePitchChanged();
}

void CrtSettings::setGlassSheen(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (m_glassSheen == v)
        return;
    m_glassSheen = v;
    emit glassSheenChanged();
}

void CrtSettings::setScreenRadius(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (m_screenRadius == v)
        return;
    m_screenRadius = v;
    emit screenRadiusChanged();
}

void CrtSettings::setScreenInset(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    if (m_screenInset == v)
        return;
    m_screenInset = v;
    emit screenInsetChanged();
}

void CrtSettings::setBloomQuality(double v)
{
    v = std::clamp(v, 0.1, 1.0);
    if (m_bloomQuality == v)
        return;
    m_bloomQuality = v;
    emit bloomQualityChanged();
}

void CrtSettings::setBurnInQuality(double v)
{
    v = std::clamp(v, 0.1, 1.0);
    if (m_burnInQuality == v)
        return;
    m_burnInQuality = v;
    emit burnInQualityChanged();
}

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
    m_loading = true;
    QSettings s(storePath(), QSettings::IniFormat);
    // Unreadable/corrupt store: report the failure BEFORE touching any member
    // so current values (defaults on a fresh instance) survive intact — and the
    // caller can decide to warn instead of silently dropping saved settings.
    if (s.status() != QSettings::NoError) {
        s.clear();   // clear the sticky error so later saves still work
        m_loading = false;
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
    m_sweep = s.value(QStringLiteral("sweep"), kSweep).toDouble();
    m_effectsEnabled = s.value(QStringLiteral("effectsEnabled"), true).toBool();
    m_rasterization = static_cast<Rasterization>(
        std::clamp(s.value(QStringLiteral("rasterization"),
                           int(ScanlineRasterization)).toInt(), 0, 4));
    m_rasterizationIntensity = s.value(QStringLiteral("rasterizationIntensity"), kRasterIntensity).toDouble();
    m_phosphorEnabled = s.value(QStringLiteral("phosphorEnabled"), kPhosphorEnabled).toBool();
    m_scanlinePitch = std::clamp(
        s.value(QStringLiteral("scanlinePitch"), kScanlinePitch).toInt(), 1, 4);
    m_glassSheen = std::clamp(
        s.value(QStringLiteral("glassSheen"), kGlassSheen).toDouble(), 0.0, 1.0);
    m_screenRadius = std::clamp(
        s.value(QStringLiteral("screenRadius"), kScreenRadius).toDouble(), 0.0, 1.0);
    m_screenInset = std::clamp(
        s.value(QStringLiteral("screenInset"), kScreenInset).toDouble(), 0.0, 1.0);
    m_bloomQuality = std::clamp(
        s.value(QStringLiteral("bloomQuality"), kBloomQuality).toDouble(), 0.1, 1.0);
    m_burnInQuality = std::clamp(
        s.value(QStringLiteral("burnInQuality"), kBurnInQuality).toDouble(), 0.1, 1.0);
    m_fontColor = s.value(QStringLiteral("fontColor"), kFontColor).value<QColor>();
    m_fontSize = s.value(QStringLiteral("fontSize"), kFontSize).toInt();
    m_autosaveIntervalSeconds = std::clamp(
        s.value(QStringLiteral("autosaveIntervalSeconds"),
                kAutosaveIntervalSeconds).toInt(),
        1, 3600);
    m_backgroundColor = s.value(QStringLiteral("backgroundColor"), kBackgroundColor).value<QColor>();
    m_minimapVisible = s.value(QStringLiteral("minimapVisible"), false).toBool();
    m_showRecentFiles = s.value(QStringLiteral("showRecentFiles"), true).toBool();
    m_backupOnOpen = s.value(QStringLiteral("backupOnOpen"), true).toBool();
    m_showFullPathInTitle =
        s.value(QStringLiteral("showFullPathInTitle"), false).toBool();

    // Named profiles are one JSON object so arbitrary (slash-containing) names
    // never collide with QSettings' hierarchical key syntax.
    m_profiles.clear();
    m_deletedProfiles.clear();
    const QString profilesJson = s.value(QStringLiteral("profiles")).toString();
    if (!profilesJson.isEmpty()) {
        QJsonParseError err {};
        const QJsonDocument doc =
            QJsonDocument::fromJson(profilesJson.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.value().isObject())
                    m_profiles.insert(it.key(), it.value().toObject().toVariantMap());
            }
        }
    }
    m_currentProfile = s.value(QStringLiteral("currentProfile")).toString();
    if (!m_profiles.contains(m_currentProfile))
        m_currentProfile.clear();
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
    emit sweepChanged();
    emit effectsEnabledChanged();
    emit rasterizationChanged();
    emit rasterizationIntensityChanged();
    emit phosphorEnabledChanged();
    emit scanlinePitchChanged();
    emit glassSheenChanged();
    emit screenRadiusChanged();
    emit screenInsetChanged();
    emit bloomQualityChanged();
    emit burnInQualityChanged();
    emit profilesChanged();
    emit currentProfileChanged();
    emit fontColorChanged();
    emit fontSizeChanged();
    emit autosaveIntervalSecondsChanged();
    emit backgroundColorChanged();
    emit minimapVisibleChanged();
    emit showRecentFilesChanged();
    emit backupOnOpenChanged();
    emit showFullPathInTitleChanged();
    emit scanlinesEnabledChanged();
    m_loading = false;
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
    s.setValue(QStringLiteral("sweep"), m_sweep);
    s.setValue(QStringLiteral("effectsEnabled"), m_effectsEnabled);
    s.setValue(QStringLiteral("rasterization"), static_cast<int>(m_rasterization));
    s.setValue(QStringLiteral("rasterizationIntensity"), m_rasterizationIntensity);
    s.setValue(QStringLiteral("phosphorEnabled"), m_phosphorEnabled);
    s.setValue(QStringLiteral("scanlinePitch"), m_scanlinePitch);
    s.setValue(QStringLiteral("glassSheen"), m_glassSheen);
    s.setValue(QStringLiteral("screenRadius"), m_screenRadius);
    s.setValue(QStringLiteral("screenInset"), m_screenInset);
    s.setValue(QStringLiteral("bloomQuality"), m_bloomQuality);
    s.setValue(QStringLiteral("burnInQuality"), m_burnInQuality);
    s.setValue(QStringLiteral("fontColor"), m_fontColor);
    s.setValue(QStringLiteral("fontSize"), m_fontSize);
    s.setValue(QStringLiteral("autosaveIntervalSeconds"), m_autosaveIntervalSeconds);
    s.setValue(QStringLiteral("backgroundColor"), m_backgroundColor);
    s.setValue(QStringLiteral("minimapVisible"), m_minimapVisible);
    s.setValue(QStringLiteral("showRecentFiles"), m_showRecentFiles);
    s.setValue(QStringLiteral("backupOnOpen"), m_backupOnOpen);
    s.setValue(QStringLiteral("showFullPathInTitle"), m_showFullPathInTitle);

    // Merge profiles written by another instance since this one loaded: a
    // stale instance (a second window, a dev probe, a slow quit) must not
    // erase a profile the user just saved elsewhere. Names deleted in this
    // instance are honoured and stay gone.
    const QString onDiskJson = s.value(QStringLiteral("profiles")).toString();
    if (!onDiskJson.isEmpty()) {
        QJsonParseError err {};
        const QJsonDocument doc = QJsonDocument::fromJson(onDiskJson.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject onDisk = doc.object();
            for (auto it = onDisk.begin(); it != onDisk.end(); ++it) {
                if (!it.value().isObject())
                    continue;
                if (m_profiles.contains(it.key()) || m_deletedProfiles.contains(it.key()))
                    continue;
                m_profiles.insert(it.key(), it.value().toObject().toVariantMap());
            }
        }
    }

    QJsonObject profilesObj;
    for (auto it = m_profiles.cbegin(); it != m_profiles.cend(); ++it)
        profilesObj.insert(it.key(), QJsonObject::fromVariantMap(it.value()));
    s.setValue(QStringLiteral("profiles"),
               QString::fromUtf8(QJsonDocument(profilesObj).toJson(QJsonDocument::Compact)));
    s.setValue(QStringLiteral("currentProfile"), m_currentProfile);
    s.endGroup();
    s.sync();
    return s.status() == QSettings::NoError;
}

// --- Named effect profiles ---------------------------------------------------

QVariantMap CrtSettings::effectValues() const
{
    QVariantMap values;
    values.insert(QStringLiteral("contrast"), m_contrast);
    values.insert(QStringLiteral("brightness"), m_brightness);
    values.insert(QStringLiteral("ambientLight"), m_ambientLight);
    values.insert(QStringLiteral("staticNoise"), m_staticNoise);
    values.insert(QStringLiteral("screenCurvature"), m_screenCurvature);
    values.insert(QStringLiteral("glowingLine"), m_glowingLine);
    values.insert(QStringLiteral("burnIn"), m_burnIn);
    values.insert(QStringLiteral("bloom"), m_bloom);
    values.insert(QStringLiteral("chromaColor"), m_chromaColor);
    values.insert(QStringLiteral("saturationColor"), m_saturationColor);
    values.insert(QStringLiteral("jitter"), m_jitter);
    values.insert(QStringLiteral("horizontalSync"), m_horizontalSync);
    values.insert(QStringLiteral("flickering"), m_flickering);
    values.insert(QStringLiteral("rgbShift"), m_rgbShift);
    values.insert(QStringLiteral("sweep"), m_sweep);
    values.insert(QStringLiteral("rasterization"), static_cast<int>(m_rasterization));
    values.insert(QStringLiteral("rasterizationIntensity"), m_rasterizationIntensity);
    values.insert(QStringLiteral("phosphorEnabled"), m_phosphorEnabled);
    values.insert(QStringLiteral("scanlinePitch"), m_scanlinePitch);
    values.insert(QStringLiteral("glassSheen"), m_glassSheen);
    values.insert(QStringLiteral("screenRadius"), m_screenRadius);
    values.insert(QStringLiteral("screenInset"), m_screenInset);
    values.insert(QStringLiteral("bloomQuality"), m_bloomQuality);
    values.insert(QStringLiteral("burnInQuality"), m_burnInQuality);
    values.insert(QStringLiteral("fontColor"), m_fontColor.name(QColor::HexArgb));
    values.insert(QStringLiteral("backgroundColor"),
                  m_backgroundColor.name(QColor::HexArgb));
    return values;
}

void CrtSettings::applyEffectValues(const QVariantMap& values)
{
    const auto dbl = [&values](const QString& key, double current) {
        return values.contains(key) ? values.value(key).toDouble() : current;
    };
    const auto integer = [&values](const QString& key, int current) {
        return values.contains(key) ? values.value(key).toInt() : current;
    };
    const auto flag = [&values](const QString& key, bool current) {
        return values.contains(key) ? values.value(key).toBool() : current;
    };

    setContrast(dbl(QStringLiteral("contrast"), m_contrast));
    setBrightness(dbl(QStringLiteral("brightness"), m_brightness));
    setAmbientLight(dbl(QStringLiteral("ambientLight"), m_ambientLight));
    setStaticNoise(dbl(QStringLiteral("staticNoise"), m_staticNoise));
    setScreenCurvature(dbl(QStringLiteral("screenCurvature"), m_screenCurvature));
    setGlowingLine(dbl(QStringLiteral("glowingLine"), m_glowingLine));
    setBurnIn(dbl(QStringLiteral("burnIn"), m_burnIn));
    setBloom(dbl(QStringLiteral("bloom"), m_bloom));
    setChromaColor(dbl(QStringLiteral("chromaColor"), m_chromaColor));
    setSaturationColor(dbl(QStringLiteral("saturationColor"), m_saturationColor));
    setJitter(dbl(QStringLiteral("jitter"), m_jitter));
    setHorizontalSync(dbl(QStringLiteral("horizontalSync"), m_horizontalSync));
    setFlickering(dbl(QStringLiteral("flickering"), m_flickering));
    setRgbShift(dbl(QStringLiteral("rgbShift"), m_rgbShift));
    setSweep(dbl(QStringLiteral("sweep"), m_sweep));
    setRasterization(static_cast<Rasterization>(
        std::clamp(integer(QStringLiteral("rasterization"),
                           static_cast<int>(m_rasterization)), 0, 4)));
    setRasterizationIntensity(
        dbl(QStringLiteral("rasterizationIntensity"), m_rasterizationIntensity));
    setPhosphorEnabled(flag(QStringLiteral("phosphorEnabled"), m_phosphorEnabled));
    setScanlinePitch(integer(QStringLiteral("scanlinePitch"), m_scanlinePitch));
    setGlassSheen(dbl(QStringLiteral("glassSheen"), m_glassSheen));
    setScreenRadius(dbl(QStringLiteral("screenRadius"), m_screenRadius));
    setScreenInset(dbl(QStringLiteral("screenInset"), m_screenInset));
    setBloomQuality(dbl(QStringLiteral("bloomQuality"), m_bloomQuality));
    setBurnInQuality(dbl(QStringLiteral("burnInQuality"), m_burnInQuality));
    if (values.contains(QStringLiteral("fontColor"))) {
        const QColor colour(values.value(QStringLiteral("fontColor")).toString());
        if (colour.isValid())
            setFontColor(colour);
    }
    if (values.contains(QStringLiteral("backgroundColor"))) {
        const QColor colour(values.value(QStringLiteral("backgroundColor")).toString());
        if (colour.isValid())
            setBackgroundColor(colour);
    }
}

QStringList CrtSettings::profileNames() const
{
    QStringList names = m_profiles.keys();
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return names;
}

bool CrtSettings::profileExists(const QString& name) const
{
    return m_profiles.contains(name.trimmed());
}

void CrtSettings::setCurrentProfile(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty() && !m_profiles.contains(trimmed))
        return;
    if (m_currentProfile == trimmed)
        return;
    m_currentProfile = trimmed;
    emit currentProfileChanged();
}

bool CrtSettings::saveProfile(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;

    m_profiles.insert(trimmed, effectValues());
    m_deletedProfiles.remove(trimmed);
    const bool selectionChanged = m_currentProfile != trimmed;
    m_currentProfile = trimmed;
    emit profilesChanged();
    if (selectionChanged)
        emit currentProfileChanged();
    save();   // an explicit profile action persists immediately
    return true;
}

bool CrtSettings::applyProfile(const QString& name)
{
    const QString trimmed = name.trimmed();
    const auto it = m_profiles.constFind(trimmed);
    if (it == m_profiles.constEnd())
        return false;

    applyEffectValues(it.value());
    const bool selectionChanged = m_currentProfile != trimmed;
    m_currentProfile = trimmed;
    if (selectionChanged)
        emit currentProfileChanged();
    save();   // remember which profile the user selected
    return true;
}

bool CrtSettings::deleteProfile(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (m_profiles.remove(trimmed) == 0)
        return false;
    m_deletedProfiles.insert(trimmed);

    const bool selectionCleared = m_currentProfile == trimmed;
    if (selectionCleared)
        m_currentProfile.clear();
    emit profilesChanged();
    if (selectionCleared)
        emit currentProfileChanged();
    save();
    return true;
}