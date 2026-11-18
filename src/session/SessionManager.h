// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QDateTime>

// Named session manager: snapshots "which files were open, where the cursor
// was, which tab was active" into ~/.config/<app>/sessions/<name>.json so a
// later run can restore the same working set.
// Directory layout is injected (setSessionsDir) so tests use a temp dir.
class SessionManager : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QUrl fileUrl;
        int line = 0;
        int column = 0;
        int activeIndex = 0;
    };

    explicit SessionManager(QObject* parent = nullptr);

    void setSessionsDir(const QString& dir);
    QString sessionsDir() const;

    void setEntries(const QVector<Entry>& entries);
    QVector<Entry> entries() const;

    void saveSession(const QString& name);
    bool restoreSession(const QString& name);
    // Autosave round: write a timestamped snapshot "auto-YYYYMMDD-HHMMSS.json"
    // through the normal session path (so it restores like a named session).
    // The clock is injectable for tests; defaults to now. Returns the snapshot
    // name (without ".json"), or an empty string if the write failed.
    QString takeSnapshot(const QDateTime& when = QDateTime());
    QString lastSession() const;
    QStringList sessionNames() const;

private:
    QString filePathFor(const QString& name) const;
    void setLastSession(const QString& name);
    // Autosave round: enforce keep-last-10 on auto- snapshots only.
    void pruneAutoSnapshots();

    QString m_dir;
    QVector<Entry> m_entries;
};
