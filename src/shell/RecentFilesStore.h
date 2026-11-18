// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QStringList>

// The most-recently-used file list behind the file dock's recents dropdown.
//
// Deliberately free of application dependencies: the caller injects the ini
// path, so crt-shell stays a plain widget library and the composition root
// keeps the "pin the legacy storage dir" knowledge in one place.
//
// Order is newest first. An exact duplicate moves to the front instead of
// repeating, and the list is capped at maxEntries. Every add() persists, so a
// crash never loses the history.
class RecentFilesStore
{
public:
    static constexpr int kDefaultMax = 10;

    explicit RecentFilesStore(const QString& settingsFile, int maxEntries = kDefaultMax);

    QString settingsFile() const { return m_file; }
    QStringList entries() const { return m_entries; }

    // Store `path` (made absolute) as the newest entry, then persist.
    void add(const QString& path);

private:
    void load();
    void save() const;

    QString m_file;
    int m_max;
    QStringList m_entries;
};
