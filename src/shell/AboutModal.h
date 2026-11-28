// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "TvModal.h"
#include <QUrl>
#include <functional>

class QPushButton;
class QLabel;

// Help > About, on the house-style modal. It replaced QMessageBox::about,
// which opened a host-themed native window above the CRT scene.
class AboutModal : public TvModal
{
    Q_OBJECT
public:
    explicit AboutModal(QWidget* parent = nullptr);

    // Auto-update (2026-11-27): show an "vX.Y.Z is available" line. The
    // "Update now" plate is added ONLY when running as an AppImage (there is
    // no single file to swap otherwise) — a passive line in a dev/distro run.
    void setAvailableUpdate(const QString& version, const QUrl& appImageUrl,
                            const QUrl& sha256Url);

    // Called when the user clicks "Update now". main.cpp wires this to the
    // UpdateRunner flow (download, verify, swap, quit).
    using UpdateTrigger = std::function<void(const QString& appImageUrl,
                                             const QString& sha256Url)>;
    void setUpdateTrigger(UpdateTrigger trigger);

    // Spec §136-138: the plate is disabled while a download/verify runs (a
    // second click must not start a second download), and re-enabled with a
    // visible error line when the update fails.
    void setUpdateBusy(bool busy);
    void setUpdateError(const QString& message);

private:
    QLabel* m_updateLine = nullptr;
    QLabel* m_updateError = nullptr;
    QPushButton* m_updateButton = nullptr;
    UpdateTrigger m_updateTrigger;
    QUrl m_appImageUrl;
    QUrl m_sha256Url;
};
