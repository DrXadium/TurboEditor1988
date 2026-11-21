// SPDX-License-Identifier: GPL-3.0-or-later
#include "LineAffixDialog.h"

#include "src/shell/FieldCell.h"
#include "src/shell/TvCheckBox.h"
#include "src/shell/TvLineEdit.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

LineAffixDialog::LineAffixDialog(Mode mode, QWidget* parent)
    : TvModal(parent)
    , m_mode(mode)
{
    const QString title = mode == Mode::End ? tr("Modify End of Line")
                                            : tr("Modify Start of Line");
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
    // TvLineEdit (caret), same as Find/Replace and Go to Line - the shell is
    // never an active window, so a plain QLineEdit draws no caret here.
    auto* tv = new TvLineEdit(m_cell);
    tv->setObjectName(QStringLiteral("lineAffixField"));
    tv->setCaretColors(QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0xAA));
    m_field = tv;
    styleAsTvField(m_field);
    m_cell->setEdit(m_field);
    m_cell->setActive(true);   // the only field: it is always the typing target

    grid->addWidget(label, 0, 0);
    grid->addWidget(m_cell, 0, 1);

    if (mode == Mode::End) {
        m_box = new TvCheckBox(tr("Line break"), body);
        m_box->setObjectName(QStringLiteral("lineAffixBreak"));
        grid->addWidget(m_box, 1, 0, 1, 2);
        connect(m_box, &TvCheckBox::toggled, this, [this] { refreshFieldEnabled(); });
    }

    // Remove: one checkbox flips this dialog between add and strip. It owns the
    // field outright, and in End mode it also clears + disables Line break, so
    // the dialog can never report both operations at once.
    m_removeBox = new TvCheckBox(tr("Remove"), body);
    m_removeBox->setObjectName(QStringLiteral("lineAffixRemove"));
    grid->addWidget(m_removeBox, mode == Mode::End ? 2 : 1, 0, 1, 2);
    connect(m_removeBox, &TvCheckBox::toggled, this, [this](bool on) {
        if (on) {
            clearLineBreakIfRemove();
        } else if (m_breakWasCleared) {
            // Only restore what Remove actually took: if Line break was off
            // before Remove went on, it must stay off (a fresh dialog that
            // never ticked it must not start inserting newlines).
            if (m_box)
                m_box->setChecked(true);
            m_breakWasCleared = false;
        }
        refreshFieldEnabled();
    });

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

bool LineAffixDialog::wantsRemove() const
{
    return m_removeBox && m_removeBox->isChecked();
}

// Remove wins outright: strip-or-insert is the choice the user made, so Line
// break clears and can only come back when Remove is off. TvCheckBox::setChecked
// guards "if (m_checked == on) return" before emitting, so this never recurses.
// The flag records that Remove took a TICKED box, so the restore knows whether
// there is anything to give back.
void LineAffixDialog::clearLineBreakIfRemove()
{
    m_breakWasCleared = m_box && m_box->isChecked();
    if (m_breakWasCleared)
        m_box->setChecked(false);
}

// The field is live only when nothing else owns it: Remove off AND (Start mode
// OR Line break off).
void LineAffixDialog::refreshFieldEnabled()
{
    if (!m_field)
        return;
    const bool owned = wantsRemove() || (m_box && m_box->isChecked());
    m_field->setEnabled(!owned);
    // The cell paints off the edit's enabled state, but nothing repaints it for
    // us (Qt has no QWidget::enabledChanged), so ask for a paint here.
    if (m_cell)
        m_cell->refresh();
}
