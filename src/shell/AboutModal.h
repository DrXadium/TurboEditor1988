// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "TvModal.h"

// Help > About, on the house-style modal. It replaced QMessageBox::about,
// which opened a host-themed native window above the CRT scene.
class AboutModal : public TvModal
{
    Q_OBJECT
public:
    explicit AboutModal(QWidget* parent = nullptr);
};
