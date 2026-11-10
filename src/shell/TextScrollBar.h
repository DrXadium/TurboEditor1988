// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QScrollBar>

// Turbo Vision text-mode scrollbar: renders DOS-style ASCII glyphs (▲/▼/◄/►
// arrow blocks at the ends, a textured ▒ track, and a solid █ thumb) instead
// of the default flat color bar. BorlandStyle::create installs this for all
// QScrollBar instances so the editor, minimap, and any list get the look.
class TextScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    explicit TextScrollBar(Qt::Orientation o, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;
};
