// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "CrtSettings.h"
#include "src/AppIdentity.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/ColorPickerModal.h"
#include "src/shell/TvLineEdit.h"
#include "src/shell/TvPlate.h"
#include "src/shell/TvSelect.h"
#include "src/shell/TvSpinBox.h"
#include "src/shell/TvTabBar.h"
#include "src/shell/TvColorSwatch.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <functional>

namespace {

// Row labels are the dialog's yellow ink on the gray panel. A QLabel is the one
// control that needs no repainting: the style engine already draws it in the
// right font, and the colour is a single property.
QLabel* tvLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("tvFieldLabel"));
    label->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));
    return label;
}

QLabel* sectionLabel(const QString& text)
{
    auto* label = tvLabel(text);
    // Section headings are black on the TV blue (2026-11-26, Xadium): white
    // and yellow headings went illegible with the CRT effects on.
    label->setStyleSheet(QStringLiteral("color: #000000; background: transparent;"));
    return label;
}

TvSpinBox* tvSpin(double value, double lo, double hi, double step, int decimals)
{
    auto* spin = new TvSpinBox;
    spin->setRange(lo, hi);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

TvPlate* tvButton(const QString& text, const QString& objectName)
{
    auto* plate = new TvPlate(text);
    plate->setObjectName(objectName);
    // Same plate as the modal's OK/Cancel (black ink, 4 px block shadow), but
    // this row sits on the TV blue, so its vacated band is blue rather than the
    // panel gray - otherwise a gray box appears behind Save/Delete.
    plate->setInk(Qt::black);
    plate->setBackdrop(QColor(0x00, 0x00, 0xAA));
    plate->setShadowOffset(4);
    plate->setCursor(Qt::ArrowCursor);
    // A little breathing room around the label (2026-11-26: Save/Delete read
    // cramped). The widget stylesheet's `min-width: 0` otherwise lets the
    // layout shrink the plate to its text; state the floor in the stylesheet
    // itself so the QSS polish cannot drop it.
    plate->setStyleSheet(QStringLiteral(
        "background: transparent; border: none; padding: 0px;"
        " min-width: 96px; min-height: 26px;"));
    plate->setFixedSize(96, 26);
    plate->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return plate;
}

// Every plain container in this body must paint the TV blue itself. The
// app-wide QSS does that normally, but the modal body is made transparent to
// let the panel through; any container the style skips then shows the
// unpainted white backing store (the 2026-11-26 report: white at the top
// right and bottom left of the CRT tab).
void paintWindowBlue(QWidget* w)
{
    w->setAutoFillBackground(true);
    QPalette p = w->palette();
    p.setColor(QPalette::Window, QColor(0x00, 0x00, 0xAA));
    w->setPalette(p);
}

} // namespace

SettingsDialog::SettingsDialog(CrtSettings* settings, QWidget* parent)
    : TvModal(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Settings \u2014 %1").arg(AppIdentity::displayName()));
    setObjectName(QStringLiteral("settingsDialog"));
    setTitle(tr("Settings \u2014 %1").arg(AppIdentity::displayName()));

    snapshot();

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("settingsBody"));
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* tabs = new TvTabBar;
    tabs->setObjectName(QStringLiteral("settingsTabs"));

    auto* stack = new QStackedWidget;
    stack->setObjectName(QStringLiteral("settingsStack"));
    stack->addWidget(buildCrtTab());
    stack->addWidget(buildAppearanceTab());

    tabs->addTab(tr("CRT"));
    tabs->addTab(tr("Editor"));
    connect(tabs, &TvTabBar::currentChanged, stack, &QStackedWidget::setCurrentIndex);

    layout->addWidget(tabs);
    layout->addWidget(stack, 1);

    setBody(body);
    addButton(tr("OK"), Accept);
    addButton(tr("Cancel"), Reject);

    // User request 2026-11-26: the OK/Cancel labels read black on the green.
    for (auto* plate : findChildren<TvPlate*>(QStringLiteral("tvModalButton")))
        plate->setInk(Qt::black);

    // Some controls are moved by the model, not by their own widget: the
    // Scanlines toggle raises rasterizationIntensity, and applying a profile
    // rewrites every value. Re-sync so the fields never show a stale number
    // (the screenshot that reported "Intensity 0.00" while Scanlines was on).
    connect(m_settings, &CrtSettings::rasterizationIntensityChanged,
            this, &SettingsDialog::syncControls);
    connect(m_settings, &CrtSettings::rasterizationChanged,
            this, &SettingsDialog::syncControls);
    connect(m_settings, &CrtSettings::scanlinesEnabledChanged,
            this, &SettingsDialog::syncControls);
    connect(m_settings, &CrtSettings::fontColorChanged,
            this, &SettingsDialog::syncControls);
    connect(m_settings, &CrtSettings::backgroundColorChanged,
            this, &SettingsDialog::syncControls);
    connect(m_settings, &CrtSettings::profilesChanged,
            this, &SettingsDialog::refreshProfiles);
    connect(m_settings, &CrtSettings::currentProfileChanged,
            this, &SettingsDialog::refreshProfiles);
}

void SettingsDialog::snapshot()
{
    m_snapshot.contrast = m_settings->contrast();
    m_snapshot.brightness = m_settings->brightness();
    m_snapshot.ambientLight = m_settings->ambientLight();
    m_snapshot.staticNoise = m_settings->staticNoise();
    m_snapshot.screenCurvature = m_settings->screenCurvature();
    m_snapshot.glowingLine = m_settings->glowingLine();
    m_snapshot.burnIn = m_settings->burnIn();
    m_snapshot.bloom = m_settings->bloom();
    m_snapshot.chromaColor = m_settings->chromaColor();
    m_snapshot.saturationColor = m_settings->saturationColor();
    m_snapshot.jitter = m_settings->jitter();
    m_snapshot.horizontalSync = m_settings->horizontalSync();
    m_snapshot.flickering = m_settings->flickering();
    m_snapshot.rgbShift = m_settings->rgbShift();
    m_snapshot.sweep = m_settings->sweep();
    m_snapshot.rasterizationIntensity = m_settings->rasterizationIntensity();
    m_snapshot.glassSheen = m_settings->glassSheen();
    m_snapshot.screenRadius = m_settings->screenRadius();
    m_snapshot.screenInset = m_settings->screenInset();
    m_snapshot.bloomQuality = m_settings->bloomQuality();
    m_snapshot.burnInQuality = m_settings->burnInQuality();
    m_snapshot.rasterization = static_cast<int>(m_settings->rasterization());
    m_snapshot.scanlinePitch = m_settings->scanlinePitch();
    m_snapshot.phosphorEnabled = m_settings->phosphorEnabled();
    m_snapshot.effectsEnabled = m_settings->effectsEnabled();
    m_snapshot.scanlines = m_settings->scanlinesEnabled();
    m_snapshot.fontColor = m_settings->fontColor();
    m_snapshot.backgroundColor = m_settings->backgroundColor();
    m_snapshot.profile = m_settings->currentProfile();
    m_snapshot.fontSize = m_settings->fontSize();
    m_snapshot.autosaveSeconds = m_settings->autosaveIntervalSeconds();
    m_snapshot.showRecentFiles = m_settings->showRecentFiles();
    m_snapshot.backupOnOpen = m_settings->backupOnOpen();
}

void SettingsDialog::restore()
{
    // Profile selection first; it only records a name (no values are applied).
    m_settings->setCurrentProfile(m_snapshot.profile);

    m_settings->setContrast(m_snapshot.contrast);
    m_settings->setBrightness(m_snapshot.brightness);
    m_settings->setAmbientLight(m_snapshot.ambientLight);
    m_settings->setStaticNoise(m_snapshot.staticNoise);
    m_settings->setScreenCurvature(m_snapshot.screenCurvature);
    m_settings->setGlowingLine(m_snapshot.glowingLine);
    m_settings->setBurnIn(m_snapshot.burnIn);
    m_settings->setBloom(m_snapshot.bloom);
    m_settings->setChromaColor(m_snapshot.chromaColor);
    m_settings->setSaturationColor(m_snapshot.saturationColor);
    m_settings->setJitter(m_snapshot.jitter);
    m_settings->setHorizontalSync(m_snapshot.horizontalSync);
    m_settings->setFlickering(m_snapshot.flickering);
    m_settings->setRgbShift(m_snapshot.rgbShift);
    m_settings->setSweep(m_snapshot.sweep);
    m_settings->setRasterizationIntensity(m_snapshot.rasterizationIntensity);
    m_settings->setGlassSheen(m_snapshot.glassSheen);
    m_settings->setScreenRadius(m_snapshot.screenRadius);
    m_settings->setScreenInset(m_snapshot.screenInset);
    m_settings->setBloomQuality(m_snapshot.bloomQuality);
    m_settings->setBurnInQuality(m_snapshot.burnInQuality);
    m_settings->setRasterization(
        static_cast<CrtSettings::Rasterization>(m_snapshot.rasterization));
    m_settings->setScanlinePitch(m_snapshot.scanlinePitch);
    m_settings->setPhosphorEnabled(m_snapshot.phosphorEnabled);
    m_settings->setEffectsEnabled(m_snapshot.effectsEnabled);
    m_settings->setFontColor(m_snapshot.fontColor);
    m_settings->setBackgroundColor(m_snapshot.backgroundColor);

    m_settings->setFontSize(m_snapshot.fontSize);
    m_settings->setAutosaveIntervalSeconds(m_snapshot.autosaveSeconds);
    m_settings->setShowRecentFiles(m_snapshot.showRecentFiles);
    m_settings->setBackupOnOpen(m_snapshot.backupOnOpen);

    syncControls();
}

void SettingsDialog::reject()
{
    restore();
    TvModal::reject();
}

void SettingsDialog::refreshProfiles()
{
    if (!m_profileSelect)
        return;
    const QSignalBlocker blocker(m_profileSelect);
    m_profileSelect->clear();
    m_profileSelect->addItem(tr("\u2014 Custom \u2014"), QString());
    const QStringList names = m_settings->profileNames();
    for (const QString& name : names)
        m_profileSelect->addItem(name, name);
    m_profileSelect->setCurrentValue(m_settings->currentProfile());
}

void SettingsDialog::syncControls()
{
    if (!m_settings)
        return;

    const auto setSpin = [this](const char* name, double value) {
        if (auto* spin = findChild<TvSpinBox*>(QString::fromLatin1(name))) {
            const QSignalBlocker blocker(spin);
            spin->setValue(value);
        }
    };
    const auto setCheck = [this](const char* name, bool on) {
        if (auto* check = findChild<TvCheckBox*>(QString::fromLatin1(name))) {
            const QSignalBlocker blocker(check);
            check->setChecked(on);
        }
    };
    const auto setSpinEnabled = [this](const char* name, bool on) {
        if (auto* spin = findChild<TvSpinBox*>(QString::fromLatin1(name)))
            spin->setEnabled(on);
    };

    setCheck("effectsEnabledCheck", m_settings->effectsEnabled());
    setCheck("scanlinesCheck", m_settings->scanlinesEnabled());
    setCheck("glowCheck", m_settings->bloom() > 0);
    setSpin("glowSpin", m_settings->bloom());
    setSpinEnabled("glowSpin", m_settings->bloom() > 0);

    setCheck("burnInCheck", m_settings->burnIn() > 0);
    setSpin("burnInSpin", m_settings->burnIn());
    setSpinEnabled("burnInSpin", m_settings->burnIn() > 0);

    setCheck("flickerCheck", m_settings->flickering() > 0);
    setSpin("flickerSpin", m_settings->flickering());
    setSpinEnabled("flickerSpin", m_settings->flickering() > 0);

    setCheck("motionCheck", m_settings->jitter() > 0);
    setSpin("motionSpin", m_settings->jitter());
    setSpinEnabled("motionSpin", m_settings->jitter() > 0);

    setCheck("phosphorCheck", m_settings->phosphorEnabled());
    setCheck("backupOnOpenCheck", m_settings->backupOnOpen());
    setSpin("chromaSpin", m_settings->chromaColor());
    setSpin("saturationSpin", m_settings->saturationColor());
    setSpinEnabled("chromaSpin", m_settings->phosphorEnabled());
    setSpinEnabled("saturationSpin", m_settings->phosphorEnabled());

    setSpin("brightnessSpin", m_settings->brightness());
    setSpin("contrastSpin", m_settings->contrast());
    setSpin("noiseSpin", m_settings->staticNoise());
    setSpin("glowLineSpin", m_settings->glowingLine());
    setSpin("rasterIntensitySpin", m_settings->rasterizationIntensity());
    setSpin("scanlinePitchSpin", m_settings->scanlinePitch());
    setSpin("ambientSpin", m_settings->ambientLight());
    setSpin("hSyncSpin", m_settings->horizontalSync());
    setSpin("rgbShiftSpin", m_settings->rgbShift());
    setSpin("glassSheenSpin", m_settings->glassSheen());
    setSpin("bloomQualitySpin", m_settings->bloomQuality());
    setSpin("burnInQualitySpin", m_settings->burnInQuality());

    if (auto* raster = findChild<TvSelect*>(QStringLiteral("rasterizationSelect"))) {
        const QSignalBlocker blocker(raster);
        raster->setCurrentValue(QString::number(static_cast<int>(m_settings->rasterization())));
    }

    if (auto* edit = findChild<TvLineEdit*>(QStringLiteral("profileNameEdit"))) {
        const QSignalBlocker blocker(edit);
        edit->setText(m_settings->currentProfile());
    }
    if (m_profileSelect) {
        const QSignalBlocker blocker(m_profileSelect);
        m_profileSelect->setCurrentValue(m_settings->currentProfile());
    }

    if (auto* swatch = findChild<TvColorSwatch*>(QStringLiteral("fontSwatch")))
        swatch->setColor(m_settings->fontColor());
    if (auto* swatch = findChild<TvColorSwatch*>(QStringLiteral("bgSwatch")))
        swatch->setColor(m_settings->backgroundColor());
}

QWidget* SettingsDialog::buildCrtTab()
{
    auto* page = new QWidget;
    paintWindowBlue(page);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);

    // --- Master bypass ------------------------------------------------------
    auto* master = new TvCheckBox(tr("Enable CRT effects"));
    master->setObjectName(QStringLiteral("effectsEnabledCheck"));
    master->setChecked(m_settings->effectsEnabled());
    connect(master, &TvCheckBox::toggled, m_settings, &CrtSettings::setEffectsEnabled);
    outer->addWidget(master);

    // --- Named effect profiles ---------------------------------------------
    outer->addWidget(sectionLabel(tr("Effects profile")));

    auto* profileRow = new QHBoxLayout;
    profileRow->setSpacing(8);
    m_profileSelect = new TvSelect;
    m_profileSelect->setObjectName(QStringLiteral("profileSelect"));
    m_profileSelect->addItem(tr("\u2014 Custom \u2014"), QString());
    const QStringList profileNames = m_settings->profileNames();
    for (const QString& name : profileNames)
        m_profileSelect->addItem(name, name);
    m_profileSelect->setCurrentValue(m_settings->currentProfile());
    profileRow->addWidget(m_profileSelect, 1, Qt::AlignVCenter);

    auto* nameEdit = new TvLineEdit;
    nameEdit->setObjectName(QStringLiteral("profileNameEdit"));
    nameEdit->setText(m_settings->currentProfile());
    nameEdit->setMinimumWidth(110);
    nameEdit->setMaximumWidth(170);
    profileRow->addWidget(nameEdit, 0, Qt::AlignVCenter);

    auto* saveProfile = tvButton(tr("Save"), QStringLiteral("profileSaveButton"));
    auto* deleteProfile = tvButton(tr("Delete"), QStringLiteral("profileDeleteButton"));
    profileRow->addWidget(saveProfile, 0, Qt::AlignVCenter);
    profileRow->addWidget(deleteProfile, 0, Qt::AlignVCenter);
    outer->addLayout(profileRow);

    auto* hint = tvLabel(tr("Type a name and press Save to create a profile."));
    hint->setWordWrap(true);
    outer->addWidget(hint);

    // Selecting a profile applies it live; Cancel restores the opened values.
    connect(m_profileSelect, &TvSelect::currentValueChanged, this,
            [this](const QString& name) {
                if (name.isEmpty()) {
                    m_settings->setCurrentProfile(QString());
                    return;
                }
                if (m_settings->applyProfile(name)) {
                    syncControls();
                    if (auto* edit = findChild<TvLineEdit*>(
                            QStringLiteral("profileNameEdit"))) {
                        const QSignalBlocker blocker(edit);
                        edit->setText(name);
                    }
                }
            });

    connect(saveProfile, &QPushButton::clicked, this, [this] {
        auto* edit = findChild<TvLineEdit*>(QStringLiteral("profileNameEdit"));
        if (!edit)
            return;
        const QString name = edit->text().trimmed();
        if (name.isEmpty())
            return;
        m_settings->saveProfile(name);
        refreshProfiles();
    });

    connect(deleteProfile, &QPushButton::clicked, this, [this] {
        const QString name = m_profileSelect ? m_profileSelect->currentValue() : QString();
        if (name.isEmpty())
            return;
        m_settings->deleteProfile(name);
        refreshProfiles();
        if (auto* edit = findChild<TvLineEdit*>(QStringLiteral("profileNameEdit"))) {
            const QSignalBlocker blocker(edit);
            edit->clear();
        }
    });

    // --- Two columns ---------------------------------------------------------
    auto* columns = new QHBoxLayout;
    columns->setSpacing(18);

    auto* left = new QWidget;
    paintWindowBlue(left);
    auto* beam = new QGridLayout(left);
    beam->setContentsMargins(0, 0, 0, 0);
    beam->setHorizontalSpacing(8);
    beam->setVerticalSpacing(4);
    beam->setColumnStretch(0, 1);
    int row = 0;

    auto addValueRow = [this](QGridLayout* grid, int& r, const QString& text,
                              const char* spinName, double value, double lo,
                              double hi, double step, int decimals,
                              std::function<void(double)> apply) {
        auto* spin = tvSpin(value, lo, hi, step, decimals);
        spin->setObjectName(QString::fromLatin1(spinName));
        connect(spin, &TvSpinBox::valueChanged, this, [apply](double v) { apply(v); });
        grid->addWidget(tvLabel(text), r, 0);
        grid->addWidget(spin, r++, 1);
    };

    auto addToggleRow = [this](QGridLayout* grid, int& r, const QString& text,
                               const char* checkName, const char* spinName,
                               bool on, double value, double lo, double hi,
                               double step, std::function<void(bool)> toggle,
                               std::function<void(double)> apply) {
        auto* check = new TvCheckBox(text);
        check->setObjectName(QString::fromLatin1(checkName));
        check->setChecked(on);
        auto* spin = tvSpin(value, lo, hi, step, 2);
        spin->setObjectName(QString::fromLatin1(spinName));
        spin->setEnabled(on);
        connect(check, &TvCheckBox::toggled, spin, &QWidget::setEnabled);
        connect(check, &TvCheckBox::toggled, this, [toggle](bool v) { toggle(v); });
        connect(spin, &TvSpinBox::valueChanged, this, [apply](double v) { apply(v); });
        grid->addWidget(check, r, 0);
        grid->addWidget(spin, r++, 1);
    };

    beam->addWidget(sectionLabel(tr("Beam")), row++, 0, 1, 2);

    // The original on/off scanline toggle stays: it is the quick switch, and
    // the Rasterization select below chooses the mask shape. Toggling it off
    // zeroes intensity (rasterization mode is kept).
    auto* scanlines = new TvCheckBox(tr("Scanlines"));
    scanlines->setObjectName(QStringLiteral("scanlinesCheck"));
    scanlines->setChecked(m_settings->scanlinesEnabled());
    connect(scanlines, &TvCheckBox::toggled, m_settings,
            &CrtSettings::setScanlinesEnabled);
    beam->addWidget(scanlines, row++, 0, 1, 2);

    auto* raster = new TvSelect;
    raster->setObjectName(QStringLiteral("rasterizationSelect"));
    raster->addItem(tr("None"), QStringLiteral("0"));
    raster->addItem(tr("Scanlines"), QStringLiteral("1"));
    raster->addItem(tr("Pixels"), QStringLiteral("2"));
    raster->addItem(tr("Subpixels"), QStringLiteral("3"));
    raster->addItem(tr("Modern"), QStringLiteral("4"));
    raster->setCurrentValue(QString::number(static_cast<int>(m_settings->rasterization())));
    connect(raster, &TvSelect::currentValueChanged, this, [this](const QString& value) {
        m_settings->setRasterization(
            static_cast<CrtSettings::Rasterization>(value.toInt()));
    });
    beam->addWidget(tvLabel(tr("Rasterization")), row, 0);
    beam->addWidget(raster, row++, 1);

    addValueRow(beam, row, tr("Scanline pitch"), "scanlinePitchSpin",
                m_settings->scanlinePitch(), 1, 4, 1, 0, [this](double v) {
                    m_settings->setScanlinePitch(int(qRound(v)));
                });
    addValueRow(beam, row, tr("Intensity"), "rasterIntensitySpin",
                m_settings->rasterizationIntensity(), 0.0, 1.0, 0.02, 2,
                [this](double v) { m_settings->setRasterizationIntensity(v); });
    addToggleRow(beam, row, tr("Bloom"), "glowCheck", "glowSpin",
                 m_settings->bloom() > 0, m_settings->bloom(), 0.0, 1.0, 0.05,
                 [this](bool on) { m_settings->setBloom(on ? 0.55 : 0.0); },
                 [this](double v) { m_settings->setBloom(v); });
    addToggleRow(beam, row, tr("Burn-in"), "burnInCheck", "burnInSpin",
                 m_settings->burnIn() > 0, m_settings->burnIn(), 0.0, 1.0, 0.05,
                 [this](bool on) { m_settings->setBurnIn(on ? 0.25 : 0.0); },
                 [this](double v) { m_settings->setBurnIn(v); });
    addValueRow(beam, row, tr("Brightness"), "brightnessSpin",
                m_settings->brightness(), 0.0, 1.0, 0.05, 2,
                [this](double v) { m_settings->setBrightness(v); });
    addValueRow(beam, row, tr("Contrast"), "contrastSpin",
                m_settings->contrast(), 0.0, 2.0, 0.05, 2,
                [this](double v) { m_settings->setContrast(v); });
    beam->setRowStretch(row++, 1);

    auto* right = new QWidget;
    paintWindowBlue(right);
    auto* glass = new QGridLayout(right);
    glass->setContentsMargins(0, 0, 0, 0);
    glass->setHorizontalSpacing(8);
    glass->setVerticalSpacing(4);
    glass->setColumnStretch(0, 1);
    int grow = 0;
    glass->addWidget(sectionLabel(tr("Glass & motion")), grow++, 0, 1, 2);

    addValueRow(glass, grow, tr("Ambient light"), "ambientSpin",
                m_settings->ambientLight(), 0.0, 1.0, 0.05, 2,
                [this](double v) { m_settings->setAmbientLight(v); });
    addValueRow(glass, grow, tr("Sweep line"), "glowLineSpin",
                m_settings->glowingLine(), 0.0, 1.0, 0.05, 2,
                [this](double v) { m_settings->setGlowingLine(v); });
    addValueRow(glass, grow, tr("Static noise"), "noiseSpin",
                m_settings->staticNoise(), 0.0, 1.0, 0.02, 2,
                [this](double v) { m_settings->setStaticNoise(v); });
    addToggleRow(glass, grow, tr("Flickering"), "flickerCheck", "flickerSpin",
                 m_settings->flickering() > 0, m_settings->flickering(), 0.0, 1.0,
                 0.02, [this](bool on) { m_settings->setFlickering(on ? 0.1 : 0.0); },
                 [this](double v) { m_settings->setFlickering(v); });
    addValueRow(glass, grow, tr("Horizontal sync"), "hSyncSpin",
                m_settings->horizontalSync(), 0.0, 1.0, 0.02, 2,
                [this](double v) { m_settings->setHorizontalSync(v); });
    addToggleRow(glass, grow, tr("Motion (jitter)"), "motionCheck", "motionSpin",
                 m_settings->jitter() > 0, m_settings->jitter(), 0.0, 1.0, 0.05,
                 [this](bool on) { m_settings->setJitter(on ? 0.2 : 0.0); },
                 [this](double v) { m_settings->setJitter(v); });
    addValueRow(glass, grow, tr("RGB shift"), "rgbShiftSpin",
                m_settings->rgbShift(), 0.0, 1.0, 0.05, 2,
                [this](double v) { m_settings->setRgbShift(v); });
    addValueRow(glass, grow, tr("Glass sheen"), "glassSheenSpin",
                m_settings->glassSheen(), 0.0, 1.0, 0.05, 2,
                [this](double v) { m_settings->setGlassSheen(v); });
    glass->setRowStretch(grow++, 1);

    columns->addWidget(left, 1);
    columns->addWidget(right, 1);
    outer->addLayout(columns);

    // --- Colour + quality ---------------------------------------------------
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(18);

    auto* colourBox = new QWidget;
    paintWindowBlue(colourBox);
    auto* colour = new QGridLayout(colourBox);
    colour->setContentsMargins(0, 0, 0, 0);
    colour->setHorizontalSpacing(8);
    colour->setVerticalSpacing(4);
    colour->setColumnStretch(0, 1);
    int crow = 0;
    colour->addWidget(sectionLabel(tr("Colour")), crow++, 0, 1, 2);

    auto* phosphor = new TvCheckBox(tr("Phosphor conversion"));
    phosphor->setObjectName(QStringLiteral("phosphorCheck"));
    phosphor->setChecked(m_settings->phosphorEnabled());
    connect(phosphor, &TvCheckBox::toggled, m_settings, &CrtSettings::setPhosphorEnabled);
    connect(phosphor, &TvCheckBox::toggled, this, [this](bool on) {
        if (auto* spin = findChild<TvSpinBox*>(QStringLiteral("chromaSpin")))
            spin->setEnabled(on);
        if (auto* spin = findChild<TvSpinBox*>(QStringLiteral("saturationSpin")))
            spin->setEnabled(on);
    });
    colour->addWidget(phosphor, crow++, 0, 1, 2);

    auto* chroma = tvSpin(m_settings->chromaColor(), 0.0, 1.0, 0.05, 2);
    chroma->setObjectName(QStringLiteral("chromaSpin"));
    chroma->setEnabled(m_settings->phosphorEnabled());
    connect(chroma, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setChromaColor);
    colour->addWidget(tvLabel(tr("Chroma")), crow, 0);
    colour->addWidget(chroma, crow++, 1);

    auto* saturation = tvSpin(m_settings->saturationColor(), 0.0, 1.0, 0.05, 2);
    saturation->setObjectName(QStringLiteral("saturationSpin"));
    saturation->setEnabled(m_settings->phosphorEnabled());
    connect(saturation, &TvSpinBox::valueChanged, m_settings,
            &CrtSettings::setSaturationColor);
    colour->addWidget(tvLabel(tr("Saturation")), crow, 0);
    colour->addWidget(saturation, crow++, 1);

    const auto pickColour = [this](const QColor& initial, bool background) {
        ColorPickerModal picker(this);
        picker.setInitialColor(initial);
        if (TvModal::exec(this, &picker) == TvModal::Accept) {
            if (background)
                m_settings->setBackgroundColor(picker.pickedColor());
            else
                m_settings->setFontColor(picker.pickedColor());
        }
    };

    auto* fontSwatch = new TvColorSwatch;
    fontSwatch->setObjectName(QStringLiteral("fontSwatch"));
    fontSwatch->setFixedSize(fontSwatch->sizeHint());
    fontSwatch->setColor(m_settings->fontColor());
    connect(fontSwatch, &TvColorSwatch::clicked, this,
            [this, pickColour] { pickColour(m_settings->fontColor(), false); });
    colour->addWidget(tvLabel(tr("Phosphor font")), crow, 0);
    colour->addWidget(fontSwatch, crow++, 1);

    auto* bgSwatch = new TvColorSwatch;
    bgSwatch->setObjectName(QStringLiteral("bgSwatch"));
    bgSwatch->setFixedSize(bgSwatch->sizeHint());
    bgSwatch->setColor(m_settings->backgroundColor());
    connect(bgSwatch, &TvColorSwatch::clicked, this,
            [this, pickColour] { pickColour(m_settings->backgroundColor(), true); });
    colour->addWidget(tvLabel(tr("Phosphor background")), crow, 0);
    colour->addWidget(bgSwatch, crow++, 1);
    colour->setRowStretch(crow++, 1);

    auto* qualityBox = new QWidget;
    paintWindowBlue(qualityBox);
    auto* quality = new QGridLayout(qualityBox);
    quality->setContentsMargins(0, 0, 0, 0);
    quality->setHorizontalSpacing(8);
    quality->setVerticalSpacing(4);
    quality->setColumnStretch(0, 1);
    int qrow = 0;
    quality->addWidget(sectionLabel(tr("Quality")), qrow++, 0, 1, 2);

    auto* bloomQuality = tvSpin(m_settings->bloomQuality(), 0.1, 1.0, 0.05, 2);
    bloomQuality->setObjectName(QStringLiteral("bloomQualitySpin"));
    connect(bloomQuality, &TvSpinBox::valueChanged, m_settings,
            &CrtSettings::setBloomQuality);
    quality->addWidget(tvLabel(tr("Bloom quality")), qrow, 0);
    quality->addWidget(bloomQuality, qrow++, 1);

    auto* burnQuality = tvSpin(m_settings->burnInQuality(), 0.1, 1.0, 0.05, 2);
    burnQuality->setObjectName(QStringLiteral("burnInQualitySpin"));
    connect(burnQuality, &TvSpinBox::valueChanged, m_settings,
            &CrtSettings::setBurnInQuality);
    quality->addWidget(tvLabel(tr("Burn-in quality")), qrow, 0);
    quality->addWidget(burnQuality, qrow++, 1);
    quality->setRowStretch(qrow++, 1);

    bottom->addWidget(colourBox, 1);
    bottom->addWidget(qualityBox, 1);
    outer->addLayout(bottom);
    outer->addStretch(1);

    return page;
}

QWidget* SettingsDialog::buildAppearanceTab()
{
    auto* page = new QWidget;
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(0, 1);

    auto* sizeSpin = new TvSpinBox;
    sizeSpin->setObjectName(QStringLiteral("fontSizeSpin"));
    sizeSpin->setRange(8, 48);
    sizeSpin->setDecimals(0);
    sizeSpin->setSingleStep(1);
    sizeSpin->setValue(m_settings->fontSize());
    connect(sizeSpin, &TvSpinBox::valueChanged, this, [this](double v) {
        m_settings->setFontSize(int(qRound(v)));
    });
    grid->addWidget(tvLabel(tr("Editor font size")), 0, 0);
    grid->addWidget(sizeSpin, 0, 1);

    auto* intervalSpin = new TvSpinBox;
    intervalSpin->setObjectName(QStringLiteral("autosaveIntervalSpin"));
    intervalSpin->setRange(1, 3600);
    intervalSpin->setDecimals(0);
    intervalSpin->setSingleStep(1);
    intervalSpin->setSuffix(tr(" s"));
    intervalSpin->setValue(m_settings->autosaveIntervalSeconds());
    connect(intervalSpin, &TvSpinBox::valueChanged, this, [this](double v) {
        m_settings->setAutosaveIntervalSeconds(int(qRound(v)));
    });
    grid->addWidget(tvLabel(tr("Autosave interval")), 1, 0);
    grid->addWidget(intervalSpin, 1, 1);

    auto* recents = new TvCheckBox(tr("Recent files on the File menu"));
    recents->setObjectName(QStringLiteral("showRecentFilesCheck"));
    recents->setChecked(m_settings->showRecentFiles());
    connect(recents, &TvCheckBox::toggled, m_settings, &CrtSettings::setShowRecentFiles);
    grid->addWidget(recents, 2, 0, 1, 2);

    // Automatic .bak backup (user request 2026-11-27): a sibling copy is
    // written next to a file whenever it is opened. ON by default.
    auto* backup = new TvCheckBox(tr("Back up files as .bak when opened"));
    backup->setObjectName(QStringLiteral("backupOnOpenCheck"));
    backup->setChecked(m_settings->backupOnOpen());
    connect(backup, &TvCheckBox::toggled, m_settings, &CrtSettings::setBackupOnOpen);
    grid->addWidget(backup, 3, 0, 1, 2);

    auto* note = tvLabel(tr("Editor scheme, chrome theme and minimap options are "
                            "configured by the theme and editor settings."));
    note->setWordWrap(true);
    grid->addWidget(note, 4, 0, 1, 2);
    grid->setRowStretch(5, 1);
    return page;
}
