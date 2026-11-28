// SPDX-License-Identifier: GPL-3.0-or-later
#include "AboutModal.h"
#include "src/AppIdentity.h"
#include "src/update/UpdateRunner.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AboutModal::AboutModal(QWidget* parent)
    : TvModal(parent)
{
    setTitle(tr("About %1").arg(AppIdentity::displayName()));
    setObjectName(QStringLiteral("aboutModal"));

    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("aboutBody"));
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* text = new QLabel;
    text->setObjectName(QStringLiteral("tvAboutText"));
    text->setTextFormat(Qt::PlainText);
    text->setWordWrap(true);
    // BLACK ink on the grey house panel (user request 2026-11-24). The panel
    // is #AAAAAA, so this is the readable, DOS-dialog ink.
    text->setStyleSheet(QStringLiteral("color: #000000; background: transparent;"));
    // The user's own copy, verbatim. The line breaks are only wrapping hints -
    // the label word-wraps to the modal's width.
    text->setText(tr("%1\n\n"
                     "A Borland-style text editor with cool retro effects, built on\n"
                     "KTextEditor (KDE Frameworks) and QT and leveraging graphical\n"
                     "effects from cool-retro-term\n\n"
                     "With love to the early days of the internet, when anything was\n"
                     "possible. We need to get there again.\n\n"
                     "Dr. Xadium")
                      .arg(AppIdentity::displayName()));
    layout->addWidget(text);
    layout->addStretch();

    setBody(body);
    addButton(tr("OK"), Accept);
}

void AboutModal::setAvailableUpdate(const QString& version, const QUrl& appImageUrl,
                                    const QUrl& sha256Url)
{
    m_appImageUrl = appImageUrl;
    m_sha256Url = sha256Url;

    auto* body = this->body();
    auto* layout = qobject_cast<QVBoxLayout*>(body->layout());
    if (!layout)
        return;

    if (!m_updateLine) {
        m_updateLine = new QLabel(body);
        m_updateLine->setObjectName(QStringLiteral("tvAboutUpdateLine"));
        m_updateLine->setTextFormat(Qt::PlainText);
        m_updateLine->setWordWrap(true);
        // BLACK ink on the grey panel, like the About body text.
        m_updateLine->setStyleSheet(
            QStringLiteral("color: #000000; background: transparent; font-weight: bold;"));
        // Insert above the trailing stretch so it sits under the body text.
        layout->insertWidget(layout->count() - 1, m_updateLine);
    }
    m_updateLine->setText(tr("A new version, %1, is available.").arg(version));

    // The swap plate only exists when there is a file to swap (AppImage run).
    if (!m_updateButton && UpdateRunner::isAppImageRun()) {
        m_updateButton = addActionButton(tr("Update now"));
        connect(m_updateButton, &QPushButton::clicked, this, [this]() {
            if (m_updateTrigger)
                m_updateTrigger(m_appImageUrl.toString(), m_sha256Url.toString());
        });
    }
}

void AboutModal::setUpdateTrigger(UpdateTrigger trigger)
{
    m_updateTrigger = std::move(trigger);
}
