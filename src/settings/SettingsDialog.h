// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"
#include <QColor>
#include <QString>

class CrtSettings;
class TvSelect;

// Settings — Configure Kate, on the house-style modal: TV tab chips, TV
// checkbox and spin-box controls, green OK / Cancel plates.
//
// Edits still apply LIVE (the CRT preview is the whole point of the dialog);
// Cancel restores every value captured when the modal opened, so the button
// means what it says. Named effect profiles are explicit saves and persist
// immediately; Cancel does not delete a profile the user deliberately saved.
class SettingsDialog : public TvModal
{
    Q_OBJECT
public:
    explicit SettingsDialog(CrtSettings* settings, QWidget* parent = nullptr);

    void reject() override;

private:
    QWidget* buildCrtTab();
    QWidget* buildAppearanceTab();
    void snapshot();
    void restore();

    // Re-seed every control from CrtSettings without feeding the change back
    // (used after a profile is applied).
    void syncControls();
    void refreshProfiles();

    CrtSettings* m_settings = nullptr;
    TvSelect* m_profileSelect = nullptr;

    struct Snapshot {
        // Every pipeline knob, so Cancel really means cancel.
        double contrast = 0.8;
        double brightness = 0.5;
        double ambientLight = 0.2;
        double staticNoise = 0.0;
        double screenCurvature = 0.0;
        double glowingLine = 0.0;
        double burnIn = 0.0;
        double bloom = 0.0;
        double chromaColor = 0.0;
        double saturationColor = 0.0;
        double jitter = 0.0;
        double horizontalSync = 0.0;
        double flickering = 0.0;
        double rgbShift = 0.0;
        double sweep = 0.0;
        double rasterizationIntensity = 0.0;
        double glassSheen = 0.0;
        double screenRadius = 0.0;
        double screenInset = 0.0;
        double bloomQuality = 0.5;
        double burnInQuality = 0.5;
        int rasterization = 1;
        int scanlinePitch = 2;
        bool phosphorEnabled = false;
        bool effectsEnabled = true;
        bool scanlines = true;
        QColor fontColor;
        QColor backgroundColor;
        QString profile;
        int fontSize = 16;
        int autosaveSeconds = 120;
        bool showRecentFiles = true;
        bool backupOnOpen = true;
        bool showFullPathInTitle = false;
    };
    Snapshot m_snapshot;
};
