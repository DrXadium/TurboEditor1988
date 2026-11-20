// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"

#include <QString>

class QLabel;

// The house modal behind the external-change prompt (2026-11-20). One frame,
// two modes:
//   Modified - the file changed on disk; plates are Ignore / Reload, and the
//              body warns that Reload discards unsaved edits.
//   Deleted  - the file is gone; plates are Close Tab / Keep, because the
//              buffer may still be worth saving back to disk.
// The dialog applies nothing itself: the caller reads the exec() role.
class ReloadDialog : public TvModal
{
    Q_OBJECT
public:
    enum class Mode { Modified, Deleted };
    Q_ENUM(Mode)

    ReloadDialog(Mode mode, const QString& filePath, QWidget* parent = nullptr);

    Mode mode() const { return m_mode; }
    QString filePath() const { return m_filePath; }
    // The elided path shown in the body, and the mode's one-line warning.
    QLabel* fileNameLabel() const { return m_name; }
    QLabel* hintLabel() const { return m_hint; }

private:
    Mode m_mode;
    QString m_filePath;
    QLabel* m_name = nullptr;
    QLabel* m_hint = nullptr;
};
