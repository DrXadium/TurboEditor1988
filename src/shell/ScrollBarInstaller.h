// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// App-wide converter: any plain QScrollBar created at runtime (by KTextEditor,
// QScrollArea, lists) is replaced in place with a TextScrollBar so every
// scrollbar gets the Turbo Vision ASCII-glyph look. Installed on the
// application by ShellWindow (which sets BorlandStyle).
class ScrollBarInstaller : public QObject
{
    Q_OBJECT
public:
    explicit ScrollBarInstaller(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* ev) override;
};
