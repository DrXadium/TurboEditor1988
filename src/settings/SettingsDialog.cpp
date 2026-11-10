// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "CrtSettings.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace {
// A slider+spin row bound to a CrtSettings double property, with an optional
// enabling checkbox (per-effect on/off maps to a driver float).
QDoubleSpinBox* makeSpin(double value, double min, double max, double step)
{
    auto* sb = new QDoubleSpinBox;
    sb->setRange(min, max);
    sb->setSingleStep(step);
    sb->setDecimals(2);
    sb->setValue(value);
    return sb;
}
} // namespace

SettingsDialog::SettingsDialog(CrtSettings* settings, QWidget* parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Settings — CRT-Kate"));
    setObjectName(QStringLiteral("settingsDialog"));

    auto* outer = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;
    tabs->addTab(buildCrtTab(), tr("CRT"));
    tabs->addTab(buildAppearanceTab(), tr("Editor / Appearance"));
    outer->addWidget(tabs);

    auto* buttonBox = new QHBoxLayout;
    auto* closeBtn = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(closeBtn, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonBox->addStretch();
    buttonBox->addWidget(closeBtn);
    outer->addLayout(buttonBox);
}

QWidget* SettingsDialog::buildCrtTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);

    // Scanlines (per-effect toggle).
    auto* scan = new QCheckBox(tr("Scanlines"));
    scan->setObjectName(QStringLiteral("scanlinesCheck"));
    scan->setChecked(m_settings->scanlinesEnabled());
    connect(scan, &QCheckBox::toggled, m_settings, &CrtSettings::setScanlinesEnabled);
    form->addRow(scan);

    // Curvature.
    auto* curv = new QCheckBox(tr("Screen curvature"));
    curv->setObjectName(QStringLiteral("curvatureCheck"));
    curv->setChecked(m_settings->curvatureEnabled());
    auto* curvSpin = makeSpin(m_settings->screenCurvature(), 0.0, 1.0, 0.05);
    curvSpin->setObjectName(QStringLiteral("curvatureSpin"));
    curvSpin->setEnabled(curv->isChecked());
    connect(curv, &QCheckBox::toggled, curvSpin, &QWidget::setEnabled);
    connect(curv, &QCheckBox::toggled, this, [this](bool on) {
        m_settings->setScreenCurvature(on ? 0.3 : 0.0);
    });
    connect(curvSpin, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setScreenCurvature);
    form->addRow(curv, curvSpin);

    // Brightness / contrast sliders.
    auto* bright = makeSpin(m_settings->brightness(), 0.0, 1.0, 0.05);
    bright->setObjectName(QStringLiteral("brightnessSpin"));
    connect(bright, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setBrightness);
    form->addRow(tr("Brightness"), bright);

    auto* contrast = makeSpin(m_settings->contrast(), 0.0, 2.0, 0.05);
    connect(contrast, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setContrast);
    form->addRow(tr("Contrast"), contrast);

    // Glow.
    auto* glow = new QCheckBox(tr("Glow / bloom"));
    glow->setChecked(m_settings->glowEnabled());
    auto* glowSpin = makeSpin(m_settings->bloom(), 0.0, 1.0, 0.05);
    glowSpin->setEnabled(glow->isChecked());
    connect(glow, &QCheckBox::toggled, glowSpin, &QWidget::setEnabled);
    connect(glow, &QCheckBox::toggled, this, [this](bool on) {
        m_settings->setBloom(on ? 0.55 : 0.0);
    });
    connect(glowSpin, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setBloom);
    form->addRow(glow, glowSpin);

    // Flicker.
    auto* flick = new QCheckBox(tr("Flickering"));
    flick->setChecked(m_settings->flickerEnabled());
    auto* flickSpin = makeSpin(m_settings->flickering(), 0.0, 1.0, 0.02);
    flickSpin->setEnabled(flick->isChecked());
    connect(flick, &QCheckBox::toggled, flickSpin, &QWidget::setEnabled);
    connect(flick, &QCheckBox::toggled, this, [this](bool on) {
        m_settings->setFlickering(on ? 0.1 : 0.0);
    });
    connect(flickSpin, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setFlickering);
    form->addRow(flick, flickSpin);

    // Static noise.
    auto* noise = makeSpin(m_settings->staticNoise(), 0.0, 1.0, 0.02);
    connect(noise, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setStaticNoise);
    form->addRow(tr("Static noise"), noise);

    // Motion (jitter).
    auto* motion = new QCheckBox(tr("Motion (jitter)"));
    motion->setChecked(m_settings->motionEnabled());
    auto* motionSpin = makeSpin(m_settings->jitter(), 0.0, 1.0, 0.05);
    motionSpin->setEnabled(motion->isChecked());
    connect(motion, &QCheckBox::toggled, motionSpin, &QWidget::setEnabled);
    connect(motion, &QCheckBox::toggled, this, [this](bool on) {
        m_settings->setJitter(on ? 0.2 : 0.0);
    });
    connect(motionSpin, &QDoubleSpinBox::valueChanged, m_settings, &CrtSettings::setJitter);
    form->addRow(motion, motionSpin);

    return w;
}

QWidget* SettingsDialog::buildAppearanceTab()
{
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);
    form->addRow(new QLabel(
        tr("Editor scheme, font, chrome theme and minimap options are wired "
           "in later tasks (KTextEditor / theming).")));
    return w;
}