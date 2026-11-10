// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVector>

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
    QString lastSession() const;
    QStringList sessionNames() const;

private:
    QString filePathFor(const QString& name) const;
    void setLastSession(const QString& name);

    QString m_dir;
    QVector<Entry> m_entries;
};
