// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <KTextEditor/InlineNoteProvider>
#include <QColor>
#include <QHash>
#include <QVector>

#include "HexColorScan.h"

namespace KTextEditor {
class View;
class InlineNote;
}

// Paints a colour swatch right after every #rgb / #rrggbb token in a view and
// reports clicks on them (Xadium 2026-11-23: "add the colored box after HTML
// color codes that kate has").
//
// An INLINE NOTE, not a decoration painted over the text: KTextEditor re-places
// notes itself, so a swatch keeps sitting on its token across wrapping, folding
// and horizontal scrolling, and a click arrives as inlineNoteActivated() with
// the note's own line and index - no hit-testing of our own.
class HexSwatchProvider : public KTextEditor::InlineNoteProvider
{
    Q_OBJECT
public:
    explicit HexSwatchProvider(KTextEditor::View* view, QObject* parent = nullptr);

    // Columns at which a swatch sits: the column just AFTER each token, so on
    // "#00ff00" the note is at column 7 (end of line included).
    QList<int> inlineNotes(int line) const override;

    // A square on the text grid, so it reads as a character-cell control.
    QSize inlineNoteSize(const KTextEditor::InlineNote& note) const override;

    // Hard-edged house box: white outline, the token's colour inside. No
    // antialiasing - the CRT pass and the VGA face are both 1-bit.
    void paintInlineNote(const KTextEditor::InlineNote& note, QPainter& painter,
                         Qt::LayoutDirection direction) const override;

    // A click opens the picker (the shell owns that; this only reports it).
    void inlineNoteActivated(const KTextEditor::InlineNote& note, Qt::MouseButtons buttons,
                             const QPoint& globalPos) override;

    // The token behind note `index` on `line`. False when the line or index has
    // no token (a stale note after an edit).
    bool tokenAt(int line, int index, HexColorToken* out) const;

signals:
    // `cellRect` is the swatch in viewport coordinates, so the caller can hang
    // the picker next to the box it clicked.
    void swatchClicked(const HexColorToken& token, int line, const QRect& cellRect);

private:
    QVector<HexColorToken> tokensForLine(int line) const;

    KTextEditor::View* m_view = nullptr;
    // One entry per line a note was asked about. Dropped on every text change:
    // a per-line diff is not worth the bookkeeping for text this size.
    mutable QHash<int, QVector<HexColorToken>> m_cache;
};
