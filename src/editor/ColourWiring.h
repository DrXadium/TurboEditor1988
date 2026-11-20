// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVector>
#include <QColor>
#include <memory>
#include <vector>

namespace KTextEditor {
class View;
class Document;
class MovingRange;
}

class QTimer;

// Applies the colourizers' output to a live KTextEditor view as
// KTextEditor::MovingRanges: bracket pairs (BracketColorizer) and quoted spans
// (StringColorizer). One object owns the view-following, the debounce and the
// ranges, so neither pass pays for that machinery twice.
//
// Verified through the RENDERER (a red-attributed bracket produced red pixels in
// a stock view): KTextEditor::View::lineAttributes() is NOT the render path (it
// returned 0 blocks for a red-attributed bracket) and must not be tested.
//
// Repaint triggers: text insert/remove and vertical scroll, debounced. The
// colourizer is viewport-scoped (firstDisplayedLine..lastDisplayedLine) so only
// visible pairs cost work, exactly like upstream's plugin.
class ColourWiring : public QObject
{
    Q_OBJECT
public:
    explicit ColourWiring(QObject* parent = nullptr);
    ~ColourWiring() override;

    // Follow this view (drops the previous one). null detaches.
    void setView(KTextEditor::View* view);
    KTextEditor::View* view() const { return m_view; }

    // Toggle: on -> colourise, off -> drop all ranges (default ON).
    void setEnabled(bool on);
    bool enabled() const { return m_enabled; }

    // Quoted spans are coloured independently of the bracket toggle, because
    // they exist for a different reason: a document with no syntax mode gets no
    // theme colours at all, and the user wants strings coloured anyway.
    // Default ON and no menu entry.
    void setStringsEnabled(bool on);
    bool stringsEnabled() const { return m_strings; }

    // Recompute now (also the debounce target).
    void repaint();

signals:
    // Emitted after ranges are applied - the test seam that proves repaint ran.
    void painted(int pairCount);
    // The string pass, same seam.
    void stringsPainted(int spanCount);

private:
    void onDocChanged();
    void dropRanges();
    void applyRanges();
    static int viewportFirstLine(KTextEditor::View* v);
    static int viewportLastLine(KTextEditor::View* v);

    KTextEditor::View* m_view = nullptr;
    QVector<QColor> m_colors;
    std::vector<std::unique_ptr<KTextEditor::MovingRange>> m_ranges;
    QTimer* m_debounce = nullptr;
    bool m_enabled = true;
    bool m_strings = true;
    int m_lastCount = 0;
    int m_lastStringCount = 0;
};
