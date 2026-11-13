// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class QScrollBar;
class TextScrollBar;

// App-wide converter: any plain QScrollBar created at runtime (by KTextEditor,
// QScrollArea, lists) is replaced in place with a TextScrollBar so every
// scrollbar gets the Turbo Vision ASCII-glyph look. Installed on the
// application by ShellWindow (which sets BorlandStyle).
class ScrollBarInstaller : public QObject
{
    Q_OBJECT
public:
    explicit ScrollBarInstaller(QObject* parent = nullptr);

    // Wires a painted TextScrollBar to the real scrollbar it replaces, in BOTH
    // directions: the proxy's own input (drag, arrows, wheel) drives \p source,
    // and \p source drives the proxy so the view scrolling keeps the thumb
    // honest. Ranges and page steps are mirrored too, so the thumb proportion
    // stays correct as the document grows.
    //
    // \p source is NOT deleted - it stays alive as the state owner, hidden
    // behind the proxy.
    static void link(QScrollBar* source, TextScrollBar* proxy);

protected:
    bool eventFilter(QObject* watched, QEvent* ev) override;
};
