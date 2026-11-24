// SPDX-License-Identifier: GPL-3.0-or-later
#include "HexSwatchProvider.h"

#include <ktexteditor/document.h>
#include <ktexteditor/inlinenote.h>
#include <ktexteditor/view.h>

#include <QFontMetrics>
#include <QPainter>

namespace {
const QColor kInk(0xFF, 0xFF, 0xFF);      // swatch outline, house white
const QColor kBgUnknown(0x00, 0x00, 0xAA); // window blue behind a transparent colour
} // namespace

HexSwatchProvider::HexSwatchProvider(KTextEditor::View* view, QObject* parent)
    : KTextEditor::InlineNoteProvider(), m_view(view)
{
    // The provider base takes no parent argument; adopt one the QObject way so
    // a caller can tie its lifetime to the view.
    if (parent)
        setParent(parent);
    if (!m_view)
        return;
    // Any edit can move or create a token, so the per-line cache is dropped and
    // the view is told to re-ask. KTextEditor re-queries only the lines a
    // provider names, so asking for a full reset is the safe, cheap answer for
    // a buffer this size.
    if (KTextEditor::Document* doc = m_view->document()) {
        connect(doc, &KTextEditor::Document::textChanged, this, [this]() {
            m_cache.clear();
            emit inlineNotesReset();
        });
    }
}

QVector<HexColorToken> HexSwatchProvider::tokensForLine(int line) const
{
    if (const auto it = m_cache.constFind(line); it != m_cache.constEnd())
        return it.value();
    QVector<HexColorToken> tokens;
    if (m_view && m_view->document()) {
        auto* doc = m_view->document();
        if (line >= 0 && line < doc->lines())
            tokens = scanHexColors(doc->line(line));
    }
    m_cache.insert(line, tokens);
    return tokens;
}

QList<int> HexSwatchProvider::inlineNotes(int line) const
{
    QList<int> columns;
    const QVector<HexColorToken> tokens = tokensForLine(line);
    columns.reserve(tokens.size());
    for (const HexColorToken& t : tokens)
        columns.append(t.start + t.length);   // just after the token
    return columns;
}

QSize HexSwatchProvider::inlineNoteSize(const KTextEditor::InlineNote& note) const
{
    // Square on the character-cell grid: as tall as the line, with one pixel of
    // daylight on each side so the box never touches the glyphs around it.
    const int h = qMax(1, note.lineHeight() - 2);
    return QSize(h, h);
}

void HexSwatchProvider::paintInlineNote(const KTextEditor::InlineNote& note, QPainter& painter,
                                        Qt::LayoutDirection) const
{
    const int line = note.position().line();
    HexColorToken token;
    if (!tokenAt(line, note.index(), &token))
        return;

    const QRect r(0, 0, note.width(), note.lineHeight() - 2);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    // Outline first, then the fill inside it: a hard 1-pixel white frame reads
    // as part of the VGA chrome, and keeps a black or blue swatch visible
    // against the window blue behind it.
    painter.fillRect(r, kInk);
    const QRect inner = r.adjusted(1, 1, -1, -1);
    if (inner.isValid()) {
        const QColor fill = token.color.isValid() ? token.color : kBgUnknown;
        painter.fillRect(inner, fill);
    }
    painter.restore();
}

void HexSwatchProvider::inlineNoteActivated(const KTextEditor::InlineNote& note,
                                            Qt::MouseButtons buttons, const QPoint& globalPos)
{
    if (!(buttons & Qt::LeftButton))
        return;
    const int line = note.position().line();
    HexColorToken token;
    if (!tokenAt(line, note.index(), &token))
        return;

    QRect cellRect;
    if (m_view) {
        // The viewport point the user clicked, in the view's own coordinates.
        const QPoint local = m_view->mapFromGlobal(globalPos);
        cellRect = QRect(local.x() - note.width(), local.y(), note.width(),
                         note.lineHeight());
    }
    emit swatchClicked(token, line, cellRect);
}

bool HexSwatchProvider::tokenAt(int line, int index, HexColorToken* out) const
{
    if (!out)
        return false;
    const QVector<HexColorToken> tokens = tokensForLine(line);
    if (index < 0 || index >= tokens.size())
        return false;
    *out = tokens.at(index);
    return true;
}
