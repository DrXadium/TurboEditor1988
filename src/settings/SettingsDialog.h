// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>

class CrtSettings;
class QTabWidget;

// CRT settings dialog: two tabs —
//   CRT: per-effect checkboxes/sliders mirroring cool-retro-term.
//   Editor/Appearance: KTextEditor scheme + font + chrome theme + minimap toggle.
// Binds live to a CrtSettings instance (writes apply immediately via NOTIFY).
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(CrtSettings* settings, QWidget* parent = nullptr);

private:
    CrtSettings* m_settings = nullptr;
    QWidget* buildCrtTab();
    QWidget* buildAppearanceTab();
};