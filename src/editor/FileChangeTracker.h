// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QString>

// What the caller should do about a watched path. None means "nothing to say".
enum class FileChange { None, Modified, Deleted };

// A file's identity as far as change detection is concerned: whether it exists,
// when it was last written, and how big it is. Value-comparable so the tracker
// can decide with one ==.
struct FileSignature {
    bool exists = false;
    qint64 mtimeMs = 0;
    qint64 size = -1;

    static FileSignature fromPath(const QString& path);
    // Written out rather than defaulted: crt-editor builds as C++17, and a
    // defaulted comparison is C++20-only.
    bool operator==(const FileSignature& other) const
    {
        return exists == other.exists && mtimeMs == other.mtimeMs && size == other.size;
    }
    bool operator!=(const FileSignature& other) const { return !(*this == other); }
};

// Pure comparison layer behind the external-change prompt. Kate's own
// modifiedOnDisk signal never fires in this build (its library carries no file
// watcher), so the app keeps the last-known signature of every watched path and
// asks this class what an event means.
//
// takeAction() ALWAYS moves the baseline to the signature it was given: that is
// what makes a prompt fire once per physical change instead of on every
// watcher echo, and it is also how an Ignore sticks.
class FileChangeTracker
{
public:
    void setBaseline(const QString& path, const FileSignature& sig);
    FileChange takeAction(const QString& path, const FileSignature& sig);
    void forget(const QString& path);
    bool isTracking(const QString& path) const;

private:
    QHash<QString, FileSignature> m_baseline;
};
