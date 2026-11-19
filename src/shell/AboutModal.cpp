// SPDX-License-Identifier: GPL-3.0-or-later
#include "AboutModal.h"
#include "src/AppIdentity.h"

#include <QLabel>
#include <QVBoxLayout>

AboutModal::AboutModal(QWidget* parent)
    : TvModal(parent)
{
    setTitle(tr("About %1").arg(AppIdentity::displayName()));
    setObjectName(QStringLiteral("aboutModal"));

    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* text = new QLabel;
    text->setObjectName(QStringLiteral("tvAboutText"));
    text->setTextFormat(Qt::PlainText);
    text->setWordWrap(true);
    text->setStyleSheet(QStringLiteral("color: #FFFF55; background: transparent;"));
    text->setText(tr("%1\n\nKate-class editing in a Borland-style shell with\n"
                     "cool-retro-term CRT effects.\n\n"
                     "Built on KTextEditor (KDE Frameworks) and Qt.")
                      .arg(AppIdentity::displayName()));
    layout->addWidget(text);
    layout->addStretch();

    setBody(body);
    addButton(tr("OK"), Accept);
}
