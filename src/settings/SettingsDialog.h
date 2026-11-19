// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"

class CrtSettings;

// Settings — Configure Kate, on the house-style modal: TV tab chips, TV
// checkbox and spin-box controls, green OK / Cancel plates.
//
// Edits still apply LIVE (the CRT preview is the whole point of the dialog);
// Cancel restores every value captured when the modal opened, so the button
// means what it says.
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

    CrtSettings* m_settings = nullptr;

    struct Snapshot {
        bool scanlines = true;
        // The per-effect toggles are DERIVED from their amounts in CrtSettings
        // (bloom > 0 means glow is on), so restoring the amounts restores the
        // toggles too.
        double curvature = 0.0;
        double brightness = 1.0;
        double contrast = 1.0;
        double bloom = 0.0;
        double flickering = 0.0;
        double noise = 0.0;
        double jitter = 0.0;
        int fontSize = 16;
        int autosaveSeconds = 120;
    };
    Snapshot m_snapshot;
};
