// SPDX-License-Identifier: GPL-3.0-or-later
#include "GoToLineDialog.h"

#include "src/shell/FieldCell.h"
#include "src/shell/TvLineEdit.h"

#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>

GoToLineDialog::GoToLineDialog(int lineCount, QWidget* parent)
    : TvModal(parent)
    , m_lineCount(qMax(1, lineCount))
{
    setObjectName(QStringLiteral("goToLineDialog"));
    setWindowTitle(tr("Go to Line"));
    setTitle(tr("Go to Line"));

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("goLineBody"));
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2 * cellWidth());
    grid->setVerticalSpacing(cellHeight());
    grid->setColumnStretch(1, 1);

    auto* label = new QLabel(tr("Line:"), body);
    // Black label ink on the gray panel (2026-11-26, user request).
    label->setStyleSheet(QStringLiteral("color: #000000; background: transparent;"));

    m_cell = new FieldCell(body);
    // TvLineEdit: the shell is never an active window, so a plain QLineEdit
    // draws no caret (2026-11-21 - "none of the input fields shows the mouse
    // cursor position"). The DOS block caret is drawn by the field itself.
    auto* tv = new TvLineEdit(m_cell);
    tv->setObjectName(QStringLiteral("goLineField"));
    tv->setCaretColors(QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0xAA));
    m_field = tv;
    styleAsTvField(m_field);
    m_cell->setEdit(m_field);
    m_cell->setActive(true);   // the only field: it is always the typing target
    // Digits only, inside the document's range: a keystroke that would make an
    // out-of-range number is refused rather than corrected after the fact.
    m_field->setValidator(new QIntValidator(1, m_lineCount, m_field));
    m_field->setText(QString());   // empty on open (rarely want the current line)

    grid->addWidget(label, 0, 0);
    grid->addWidget(m_cell, 0, 1);
    // TvModal's sizeHint floors the dialog at 200px and layoutChildren hands the
    // body all of it. This dialog has ONE content row, so without an empty
    // stretch row below, the grid stretches the FieldCell to swallow ~90px of
    // slack - a one-digit entry field rendered 112px tall. Push the slack into
    // this invisible row instead; the field keeps its one-line height.
    grid->setRowStretch(1, 1);

    setBody(body);

    // Plates right to left: Cancel added first, so it sits rightmost.
    addButton(tr("Cancel"), Reject);
    addButton(tr("OK"), Accept);
}

int GoToLineDialog::line() const
{
    if (!m_field)
        return 1;
    bool ok = false;
    const int value = m_field->text().toInt(&ok);
    if (!ok)
        return 1;                       // empty or non-numeric: first line
    return qBound(1, value, m_lineCount);
}
