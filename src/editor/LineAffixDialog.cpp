// SPDX-License-Identifier: GPL-3.0-or-later
#include "LineAffixDialog.h"

#include "src/shell/FieldCell.h"
#include "src/shell/TvCheckBox.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

LineAffixDialog::LineAffixDialog(Mode mode, QWidget* parent)
    : TvModal(parent)
    , m_mode(mode)
{
    const QString title = mode == Mode::End ? tr("Add to End of Line")
                                            : tr("Add to Start of Line");
    setObjectName(QStringLiteral("lineAffixDialog"));
    setWindowTitle(title);
    setTitle(title);

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("lineAffixBody"));
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2 * cellWidth());
    grid->setVerticalSpacing(cellHeight());
    grid->setColumnStretch(1, 1);

    auto* label = new QLabel(tr("Text:"), body);
    label->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));

    m_cell = new FieldCell(body);
    m_field = new QLineEdit(m_cell);
    m_field->setObjectName(QStringLiteral("lineAffixField"));
    styleAsTvField(m_field);
    m_cell->setEdit(m_field);
    m_cell->setActive(true);   // the only field: it is always the typing target

    grid->addWidget(label, 0, 0);
    grid->addWidget(m_cell, 0, 1);

    if (mode == Mode::End) {
        m_box = new TvCheckBox(tr("Line break"), body);
        m_box->setObjectName(QStringLiteral("lineAffixBreak"));
        grid->addWidget(m_box, 1, 0, 1, 2);
        connect(m_box, &TvCheckBox::toggled, this,
                [this](bool on) { m_field->setEnabled(!on); });
    }

    setBody(body);

    // Plates are laid out right to left: Cancel is added first, so it sits
    // rightmost, exactly as in the Find/Replace dialog.
    addButton(tr("Cancel"), Reject);
    addButton(tr("OK"), Accept);
}

QString LineAffixDialog::text() const
{
    return m_field ? m_field->text() : QString();
}

bool LineAffixDialog::wantsLineBreak() const
{
    return m_box && m_box->isChecked();
}
