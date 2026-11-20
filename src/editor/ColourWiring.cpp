// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColourWiring.h"
#include "BracketColorizer.h"
#include "StringColorizer.h"

#include <QTimer>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/movingrange.h>
#include <KTextEditor/Attribute>

namespace {
// Debounce: upstream uses 200 ms for text, 50 ms for scroll. One short timer is
// enough here (the viewport is small and repaint is cheap).
const int kDebounceMs = 40;
} // namespace

ColourWiring::ColourWiring(QObject* parent)
    : QObject(parent)
    , m_colors(BracketColorizer::defaultColors())
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &ColourWiring::repaint);
}

ColourWiring::~ColourWiring()
{
    dropRanges();
}

int ColourWiring::viewportFirstLine(KTextEditor::View* v)
{
    const int l = v->firstDisplayedLine();
    return l < 0 ? 0 : l;
}

int ColourWiring::viewportLastLine(KTextEditor::View* v)
{
    const int l = v->lastDisplayedLine();
    return l < 0 ? 0 : l;
}

void ColourWiring::setView(KTextEditor::View* view)
{
    if (m_view == view)
        return;
    // Disconnect the old view's document before dropping its ranges.
    if (m_view && m_view->document())
        disconnect(m_view->document(), nullptr, this, nullptr);
    if (m_view)
        disconnect(m_view, nullptr, this, nullptr);
    dropRanges();
    m_view = view;
    if (!m_view)
        return;
    KTextEditor::Document* doc = m_view->document();
    if (doc) {
        connect(doc, &KTextEditor::Document::textInserted,
                this, [this](KTextEditor::Document*, KTextEditor::Cursor, const QString&) {
                    onDocChanged();
                });
        connect(doc, &KTextEditor::Document::textRemoved,
                this, [this](KTextEditor::Document*, KTextEditor::Range, const QString&) {
                    onDocChanged();
                });
    }
    connect(m_view, &KTextEditor::View::verticalScrollPositionChanged,
            this, [this](KTextEditor::View*, KTextEditor::Cursor) { onDocChanged(); });
    if (m_enabled)
        m_debounce->start();
}

void ColourWiring::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    // Rebuild rather than drop: the two passes share one range list, so dropping
    // here would take the string colours with it.
    if (m_view)
        repaint();
}

void ColourWiring::setStringsEnabled(bool on)
{
    if (m_strings == on)
        return;
    m_strings = on;
    if (m_view)
        repaint();
}

void ColourWiring::onDocChanged()
{
    if ((m_enabled || m_strings) && m_view)
        m_debounce->start();
}

void ColourWiring::dropRanges()
{
    m_ranges.clear();
    m_lastCount = 0;
    m_lastStringCount = 0;
}

void ColourWiring::repaint()
{
    if (!m_view) {
        dropRanges();
        emit painted(0);
        emit stringsPainted(0);
        return;
    }
    KTextEditor::Document* doc = m_view->document();
    if (!doc) {
        dropRanges();
        emit painted(0);
        emit stringsPainted(0);
        return;
    }
    // Both passes are gated inside applyRanges, so one being off never removes
    // the other's colours.
    applyRanges();
    emit painted(m_lastCount);
    emit stringsPainted(m_lastStringCount);
}

void ColourWiring::applyRanges()
{
    KTextEditor::Document* doc = m_view->document();
    // Snapshot the document lines and the comment/string style probe.
    const int nLines = doc->lines();
    QVector<QString> text;
    text.reserve(nLines);
    for (int i = 0; i < nLines; ++i)
        text.append(doc->line(i));

    const auto styleProbe = [doc](int line, int col) {
        const auto st = doc->defaultStyleAt(KTextEditor::Cursor(line, col));
        return st == KSyntaxHighlighting::Theme::TextStyle::Comment
               || st == KSyntaxHighlighting::Theme::TextStyle::Char
               || st == KSyntaxHighlighting::Theme::TextStyle::String;
    };

    const int first = viewportFirstLine(m_view);
    const int last = viewportLastLine(m_view);

    // Pass 1: quoted spans, independent of the syntax mode, so a .txt or a fresh
    // untitled tab (mode None) gets coloured strings too.
    const QVector<StringColorizer::Span> spans =
        m_strings ? StringColorizer::findSpans(text, first, last, styleProbe)
                  : QVector<StringColorizer::Span>();

    // Rebuild from scratch (upstream reuses; ours is viewport-sized and cheap).
    dropRanges();

    // One range covers a whole quoted span; the attribute paints its extent.
    for (const auto& s : spans) {
        auto attr = KTextEditor::Attribute::Ptr(new KTextEditor::Attribute);
        attr->setForeground(s.color);
        auto* r = doc->newMovingRange(
            KTextEditor::Range(s.line, s.col, s.line, s.endCol + 1));
        r->setAttribute(attr);
        m_ranges.emplace_back(r);
    }
    m_lastStringCount = spans.size();

    // Pass 2: brackets. A bracket inside a quoted span is text, not structure,
    // so it must not be coloured - otherwise `s = "{a}"` in a mode-None document
    // would show bracket colours inside the green string.
    const auto bracketProbe = [&styleProbe, &spans](int line, int col) {
        if (styleProbe(line, col))
            return true;
        for (const auto& s : spans) {
            if (s.line == line && col >= s.col && col <= s.endCol)
                return true;
        }
        return false;
    };

    const auto coloured = m_enabled
        ? BracketColorizer::colorize(text, first, last, bracketProbe, m_colors.size())
        : QVector<BracketColorizer::Coloured>();
    m_ranges.reserve(m_ranges.size() + coloured.size() * 2);
    for (const auto& c : coloured) {
        auto attr = KTextEditor::Attribute::Ptr(new KTextEditor::Attribute);
        attr->setForeground(m_colors.at(c.color));
        auto* rOpen = doc->newMovingRange(
            KTextEditor::Range(c.line, c.col, c.line, c.col + 1));
        rOpen->setAttribute(attr);
        m_ranges.emplace_back(rOpen);
        if (c.closeLine >= 0) {
            auto* rClose = doc->newMovingRange(
                KTextEditor::Range(c.closeLine, c.closeCol,
                                   c.closeLine, c.closeCol + 1));
            rClose->setAttribute(attr);       // same colour: the pair
            m_ranges.emplace_back(rClose);
        }
    }
    m_lastCount = coloured.size();
}
