// SPDX-License-Identifier: GPL-3.0-or-later
#include "ReloadDialog.h"

#include <QDir>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>

namespace {
QLabel* tvLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));
    return label;
}
} // namespace

ReloadDialog::ReloadDialog(Mode mode, const QString& filePath, QWidget* parent)
    : TvModal(parent)
    , m_mode(mode)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("reloadDialog"));
    const QString caption = mode == Mode::Modified ? tr("File Changed on Disk")
                                                   : tr("File Deleted on Disk");
    setWindowTitle(caption);
    setTitle(caption);

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("reloadBody"));
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(2 * cellWidth());
    grid->setVerticalSpacing(cellHeight());
    grid->setColumnStretch(1, 1);

    auto* fileCaption = tvLabel(tr("File:"), body);

    m_name = tvLabel(QString(), body);
    m_name->setObjectName(QStringLiteral("reloadFileName"));
    // Elide in the MIDDLE: the root and the file name both stay readable, and a
    // 300-character path cannot widen the modal past the shell it is clamped to.
    // TvModal's constructor polishes, so cellWidth() already reflects the TV
    // font here.
    const int maxWidth = 46 * cellWidth();
    m_name->setText(fontMetrics().elidedText(QDir::toNativeSeparators(filePath),
                                             Qt::ElideMiddle, maxWidth));
    m_name->setToolTip(filePath);

    m_hint = tvLabel(mode == Mode::Modified
                         ? tr("Reload discards unsaved changes.")
                         : tr("The buffer is still open; Save recreates the file."),
                     body);
    m_hint->setObjectName(QStringLiteral("reloadHint"));
    m_hint->setWordWrap(true);

    grid->addWidget(fileCaption, 0, 0);
    grid->addWidget(m_name, 0, 1);
    grid->addWidget(m_hint, 1, 0, 1, 2);
    // TvModal floors the dialog at 200px and hands the body ALL the leftover
    // height. Without an empty stretch row, the content rows would stretch to
    // swallow it (the Go to Line field-height bug). Push the slack below.
    grid->setRowStretch(2, 1);

    setBody(body);

    // Plates right to left: the first added sits rightmost.
    if (mode == Mode::Modified) {
        addButton(tr("Ignore"), Reject);
        addButton(tr("Reload"), Accept);
    } else {
        addButton(tr("Close Tab"), Reject);
        addButton(tr("Keep"), Accept);
    }
}
