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

// Applies BracketColorizer's output to a live KTextEditor view as
// KTextEditor::MovingRanges, so each bracket pair paints with its colour.
//
// Verified through the RENDERER (a red-attributed bracket produced red pixels in
// a stock view): KTextEditor::View::lineAttributes() is NOT the render path (it
// returned 0 blocks for a red-attributed bracket) and must not be tested.
//
// Repaint triggers: text insert/remove and vertical scroll, debounced. The
// colourizer is viewport-scoped (firstDisplayedLine..lastDisplayedLine) so only
// visible pairs cost work, exactly like upstream's plugin.
class BracketWiring : public QObject
{
    Q_OBJECT
public:
    explicit BracketWiring(QObject* parent = nullptr);
    ~BracketWiring() override;

    // Follow this view (drops the previous one). null detaches.
    void setView(KTextEditor::View* view);
    KTextEditor::View* view() const { return m_view; }

    // Toggle: on -> colourise, off -> drop all ranges (default ON).
    void setEnabled(bool on);
    bool enabled() const { return m_enabled; }

    // Recompute now (also the debounce target).
    void repaint();

signals:
    // Emitted after ranges are applied - the test seam that proves repaint ran.
    void painted(int pairCount);

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
    int m_lastCount = 0;
};
