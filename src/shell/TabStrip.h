// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QVector>
#include <QRect>

// The file-tab row (mock 2026-11-14). A band above the document's border row:
//
//   [◄] [ name ■ ] [ name ■ ]                              [►][+][▼]
//
//  * the band is the scrollbar track's stipple design in grey, one cell tall;
//  * each tab is its own bracketed chip - bright white when active, dim grey
//    when not - with a BRIGHT GREEN square that closes that tab;
//  * ◄ / ► scroll the chip window (always present, even when nothing overflows);
//  * + opens a new document; ▼ lists every open tab in a Borland dropdown.
//
// Populated by EditorTabManager (Task 5). Clicks emit tabActivated / tabClosed /
// newTabRequested / tabListRequested so the composition root can act.
class TabStrip : public QWidget
{
    Q_OBJECT
public:
    explicit TabStrip(QWidget* parent = nullptr);
    void setTabs(const QStringList& titles, int active);

    // Geometry, in cell units (exposed for tests / hit-testing).
    QRect prevTabRect() const;     // ◄  - first cell of the row
    QRect nextTabRect() const;     // ►  - third cell from the right
    QRect newTabRect() const;      // +  - second cell from the right
    QRect tabListRect() const;     // ▼  - LAST cell (the scrollbar's own column)
    QVector<QRect> tabRects() const;   // visible chips, left to right
    QRect closeRect(int index) const;  // the green square inside chip `index`

    int firstVisibleTab() const { return m_firstVisible; }
    int activeIndex() const { return m_active; }
    QStringList titles() const { return m_titles; }

signals:
    void tabActivated(int index);
    void tabClosed(int index);
    void newTabRequested();
    void tabListRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void changeEvent(QEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    int cellW() const;
    // One character cell, re-measured when the QSS font lands: the strip is
    // exactly one cell tall, so a construction-time height would be wrong.
    void updateCellSize();
    int nameCells(const QString& title) const;
    // Cells a chip occupies: '[' ' ' name ' ' '■' ' ' ']'.
    int chipCells(const QString& title) const { return nameCells(title) + 6; }
    // Index of the leftmost visible chip (sanitised for the paint path).
    int firstShown() const;
    // Shift the chip window by `delta` chips, clamped at both ends.
    void scrollChips(int delta);
    // Keep the active tab inside the visible window.
    void ensureActiveVisible();

    QStringList m_titles;
    int m_active = 0;
    int m_firstVisible = 0;
};
