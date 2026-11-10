// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QVector>
#include <QRect>

// Document tab strip rendered as a Turbo Vision window title line (Task 12):
// the frame's top edge, tabs as title-line segments, a green [■] close box and
// a green [↕] zoom box. Populated by EditorTabManager (Task 5); clicks emit
// tabActivated so the composition root can switch documents.
class TabStrip : public QWidget
{
    Q_OBJECT
public:
    explicit TabStrip(QWidget* parent = nullptr);
    void setTabs(const QStringList& titles, int active);

    // Box + tab geometry (exposed for tests / hit-testing).
    QRect closeBoxRect() const;
    QRect zoomBoxRect() const;
    QVector<QRect> tabRects() const;
    int activeIndex() const { return m_active; }
    QStringList titles() const { return m_titles; }

signals:
    void tabActivated(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    QStringList m_titles;
    int m_active = 0;
};
