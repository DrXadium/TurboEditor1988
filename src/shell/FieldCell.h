// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

class QLineEdit;

// A TV field cell: the QLineEdit sits inset by a 2 px ring, and the cell paints
// that ring itself. The shell is never an active window, so a QLineEdit never
// draws a caret here - the yellow ring is the "you are typing here" mark, the
// same treatment the file dock gives its fields. Lifted out of
// FindReplaceDialog.cpp (2026-11-19) so the line-affix dialog shares one look.
class FieldCell : public QWidget
{
public:
    explicit FieldCell(QWidget* parent = nullptr);

    // The field must be tall enough for a full line of VGA text plus the ring,
    // or the grid squeezes it to 12 px and the glyphs are clipped.
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void setEdit(QLineEdit* edit);
    QLineEdit* edit() const { return m_edit; }
    bool isActive() const { return m_active; }
    void setActive(bool on);
    // Repaint after the edit's enabled state changed. Qt has no
    // QWidget::enabledChanged, so whoever flips the edit calls this; without it
    // the cell keeps its last paint and a dead field looks live.
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    QLineEdit* m_edit = nullptr;
    bool m_active = false;
};

// Style `edit` as the flat TV field: blue box, white text, NO border - the rule
// that removed the white-border defect the user reported on the settings fields.
void styleAsTvField(QLineEdit* edit);
