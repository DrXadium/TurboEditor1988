// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecentFilesStore.h"
#include <QSettings>
#include <QFileInfo>

RecentFilesStore::RecentFilesStore(const QString& settingsFile, int maxEntries)
    : m_file(settingsFile)
    , m_max(maxEntries > 0 ? maxEntries : kDefaultMax)
{
    load();
}

void RecentFilesStore::load()
{
    m_entries.clear();
    QSettings s(m_file, QSettings::IniFormat);
    const QStringList stored = s.value(QStringLiteral("recents/files")).toStringList();
    for (const QString& path : stored) {
        if (path.trimmed().isEmpty() || m_entries.contains(path))
            continue;
        m_entries << path;
        if (m_entries.size() >= m_max)
            break;
    }
}

void RecentFilesStore::add(const QString& path)
{
    if (path.trimmed().isEmpty())
        return;
    // Absolute so a later run resolves the entry the same way this one did.
    const QString absolute = QFileInfo(path).absoluteFilePath();
    m_entries.removeAll(absolute);
    m_entries.prepend(absolute);
    while (m_entries.size() > m_max)
        m_entries.removeLast();
    save();
}

void RecentFilesStore::save() const
{
    QSettings s(m_file, QSettings::IniFormat);
    s.setValue(QStringLiteral("recents/files"), m_entries);
    s.sync();
}
