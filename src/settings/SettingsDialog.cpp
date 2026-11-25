// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "CrtSettings.h"
#include "src/AppIdentity.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvSpinBox.h"
#include "src/shell/TvTabBar.h"

#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

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

TvSpinBox* tvSpin(double value, double lo, double hi, double step, int decimals)
{
    auto* spin = new TvSpinBox;
    spin->setRange(lo, hi);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

} // namespace

SettingsDialog::SettingsDialog(CrtSettings* settings, QWidget* parent)
    : TvModal(parent)
    , m_settings(settings)
{
    // The window title is kept for anything that treats this as a window; the
    // visible title is the one drawn in the frame.
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
}

void SettingsDialog::snapshot()
{
    m_snapshot.scanlines = m_settings->scanlinesEnabled();
    m_snapshot.curvature = m_settings->screenCurvature();
    m_snapshot.brightness = m_settings->brightness();
    m_snapshot.contrast = m_settings->contrast();
    m_snapshot.bloom = m_settings->bloom();
    m_snapshot.flickering = m_settings->flickering();
    m_snapshot.noise = m_settings->staticNoise();
    m_snapshot.jitter = m_settings->jitter();
    m_snapshot.fontSize = m_settings->fontSize();
    m_snapshot.autosaveSeconds = m_settings->autosaveIntervalSeconds();
    m_snapshot.showRecentFiles = m_settings->showRecentFiles();
}

void SettingsDialog::restore()
{
    m_settings->setScreenCurvature(m_snapshot.curvature);
    m_settings->setBrightness(m_snapshot.brightness);
    m_settings->setContrast(m_snapshot.contrast);
    m_settings->setBloom(m_snapshot.bloom);
    m_settings->setFlickering(m_snapshot.flickering);
    m_settings->setStaticNoise(m_snapshot.noise);
    m_settings->setJitter(m_snapshot.jitter);
    m_settings->setScanlinesEnabled(m_snapshot.scanlines);
    m_settings->setFontSize(m_snapshot.fontSize);
    m_settings->setAutosaveIntervalSeconds(m_snapshot.autosaveSeconds);
    m_settings->setShowRecentFiles(m_snapshot.showRecentFiles);
}

void SettingsDialog::reject()
{
    restore();
    TvModal::reject();
}

QWidget* SettingsDialog::buildCrtTab()
{
    auto* page = new QWidget;
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(0, 1);
    int row = 0;

    // Scanlines: a bare toggle, the label lives in the control.
    auto* scan = new TvCheckBox(tr("Scanlines"));
    scan->setObjectName(QStringLiteral("scanlinesCheck"));
    scan->setChecked(m_settings->scanlinesEnabled());
    connect(scan, &TvCheckBox::toggled, m_settings, &CrtSettings::setScanlinesEnabled);
    grid->addWidget(scan, row++, 0, 1, 2);

    // Curvature: toggle + amount.
    auto* curv = new TvCheckBox(tr("Screen curvature"));
    curv->setObjectName(QStringLiteral("curvatureCheck"));
    curv->setChecked(m_settings->curvatureEnabled());
    auto* curvSpin = tvSpin(m_settings->screenCurvature(), 0.0, 1.0, 0.05, 2);
    curvSpin->setObjectName(QStringLiteral("curvatureSpin"));
    curvSpin->setEnabled(curv->isChecked());
    connect(curv, &TvCheckBox::toggled, curvSpin, &QWidget::setEnabled);
    connect(curv, &TvCheckBox::toggled, this, [this](bool on) {
        m_settings->setScreenCurvature(on ? 0.3 : 0.0);
    });
    connect(curvSpin, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setScreenCurvature);
    grid->addWidget(curv, row, 0);
    grid->addWidget(curvSpin, row++, 1);

    // Brightness / contrast: plain values.
    auto* bright = tvSpin(m_settings->brightness(), 0.0, 1.0, 0.05, 2);
    bright->setObjectName(QStringLiteral("brightnessSpin"));
    connect(bright, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setBrightness);
    grid->addWidget(tvLabel(tr("Brightness")), row, 0);
    grid->addWidget(bright, row++, 1);

    auto* contrast = tvSpin(m_settings->contrast(), 0.0, 2.0, 0.05, 2);
    contrast->setObjectName(QStringLiteral("contrastSpin"));
    connect(contrast, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setContrast);
    grid->addWidget(tvLabel(tr("Contrast")), row, 0);
    grid->addWidget(contrast, row++, 1);

    // Glow: toggle + amount.
    auto* glow = new TvCheckBox(tr("Glow / bloom"));
    glow->setObjectName(QStringLiteral("glowCheck"));
    glow->setChecked(m_settings->glowEnabled());
    auto* glowSpin = tvSpin(m_settings->bloom(), 0.0, 1.0, 0.05, 2);
    glowSpin->setObjectName(QStringLiteral("glowSpin"));
    glowSpin->setEnabled(glow->isChecked());
    connect(glow, &TvCheckBox::toggled, glowSpin, &QWidget::setEnabled);
    connect(glow, &TvCheckBox::toggled, this, [this](bool on) {
        m_settings->setBloom(on ? 0.55 : 0.0);
    });
    connect(glowSpin, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setBloom);
    grid->addWidget(glow, row, 0);
    grid->addWidget(glowSpin, row++, 1);

    // Flicker: toggle + amount.
    auto* flick = new TvCheckBox(tr("Flickering"));
    flick->setObjectName(QStringLiteral("flickerCheck"));
    flick->setChecked(m_settings->flickerEnabled());
    auto* flickSpin = tvSpin(m_settings->flickering(), 0.0, 1.0, 0.02, 2);
    flickSpin->setObjectName(QStringLiteral("flickerSpin"));
    flickSpin->setEnabled(flick->isChecked());
    connect(flick, &TvCheckBox::toggled, flickSpin, &QWidget::setEnabled);
    connect(flick, &TvCheckBox::toggled, this, [this](bool on) {
        m_settings->setFlickering(on ? 0.1 : 0.0);
    });
    connect(flickSpin, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setFlickering);
    grid->addWidget(flick, row, 0);
    grid->addWidget(flickSpin, row++, 1);

    // Static noise: plain value.
    auto* noise = tvSpin(m_settings->staticNoise(), 0.0, 1.0, 0.02, 2);
    noise->setObjectName(QStringLiteral("noiseSpin"));
    connect(noise, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setStaticNoise);
    grid->addWidget(tvLabel(tr("Static noise")), row, 0);
    grid->addWidget(noise, row++, 1);

    // Motion (jitter): toggle + amount.
    auto* motion = new TvCheckBox(tr("Motion (jitter)"));
    motion->setObjectName(QStringLiteral("motionCheck"));
    motion->setChecked(m_settings->motionEnabled());
    auto* motionSpin = tvSpin(m_settings->jitter(), 0.0, 1.0, 0.05, 2);
    motionSpin->setObjectName(QStringLiteral("motionSpin"));
    motionSpin->setEnabled(motion->isChecked());
    connect(motion, &TvCheckBox::toggled, motionSpin, &QWidget::setEnabled);
    connect(motion, &TvCheckBox::toggled, this, [this](bool on) {
        m_settings->setJitter(on ? 0.2 : 0.0);
    });
    connect(motionSpin, &TvSpinBox::valueChanged, m_settings, &CrtSettings::setJitter);
    grid->addWidget(motion, row, 0);
    grid->addWidget(motionSpin, row++, 1);

    grid->setRowStretch(row, 1);
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

    // Recent files on the File menu (user request 2026-11-24). Live like every
    // other control here; Cancel puts it back through restore().
    auto* recents = new TvCheckBox(tr("Recent files on the File menu"));
    recents->setObjectName(QStringLiteral("showRecentFilesCheck"));
    recents->setChecked(m_settings->showRecentFiles());
    connect(recents, &TvCheckBox::toggled, m_settings, &CrtSettings::setShowRecentFiles);
    grid->addWidget(recents, 2, 0, 1, 2);

    auto* note = tvLabel(tr("Editor scheme, chrome theme and minimap options are "
                            "configured by the theme and editor settings."));
    note->setWordWrap(true);
    grid->addWidget(note, 3, 0, 1, 2);
    grid->setRowStretch(4, 1);
    return page;
}
