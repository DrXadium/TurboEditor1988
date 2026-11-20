// SPDX-License-Identifier: GPL-3.0-or-later
#include "BracketWiring.h"
#include "BracketColorizer.h"

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

BracketWiring::BracketWiring(QObject* parent)
    : QObject(parent)
    , m_colors(BracketColorizer::defaultColors())
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &BracketWiring::repaint);
}

BracketWiring::~BracketWiring()
{
    dropRanges();
}

int BracketWiring::viewportFirstLine(KTextEditor::View* v)
{
    const int l = v->firstDisplayedLine();
    return l < 0 ? 0 : l;
}

int BracketWiring::viewportLastLine(KTextEditor::View* v)
{
    const int l = v->lastDisplayedLine();
    return l < 0 ? 0 : l;
}

void BracketWiring::setView(KTextEditor::View* view)
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

void BracketWiring::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    if (!on)
        dropRanges();
    else if (m_view)
        m_debounce->start();
}

void BracketWiring::onDocChanged()
{
    if (m_enabled && m_view)
        m_debounce->start();
}

void BracketWiring::dropRanges()
{
    m_ranges.clear();
    m_lastCount = 0;
}

void BracketWiring::repaint()
{
    if (!m_view || !m_enabled) {
        dropRanges();
        emit painted(0);
        return;
    }
    KTextEditor::Document* doc = m_view->document();
    if (!doc) {
        dropRanges();
        emit painted(0);
        return;
    }
    applyRanges();
    emit painted(m_lastCount);
}

void BracketWiring::applyRanges()
{
    KTextEditor::Document* doc = m_view->document();
    // Snapshot the document lines and the comment/string style probe.
    const int nLines = doc->lines();
    QVector<QString> text;
    text.reserve(nLines);
    for (int i = 0; i < nLines; ++i)
        text.append(doc->line(i));

    const auto probe = [doc](int line, int col) {
        const auto st = doc->defaultStyleAt(KTextEditor::Cursor(line, col));
        return st == KSyntaxHighlighting::Theme::TextStyle::Comment
               || st == KSyntaxHighlighting::Theme::TextStyle::Char
               || st == KSyntaxHighlighting::Theme::TextStyle::String;
    };

    const int first = viewportFirstLine(m_view);
    const int last = viewportLastLine(m_view);
    const auto coloured = BracketColorizer::colorize(text, first, last, probe,
                                                     m_colors.size());
    // Rebuild from scratch (upstream reuses; ours is viewport-sized and cheap).
    dropRanges();
    m_ranges.reserve(coloured.size() * 2);
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
