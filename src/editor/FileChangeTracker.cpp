// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileChangeTracker.h"

#include <QFileInfo>

FileSignature FileSignature::fromPath(const QString& path)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists())
        return {};
    return { true, info.lastModified().toMSecsSinceEpoch(), info.size() };
}

void FileChangeTracker::setBaseline(const QString& path, const FileSignature& sig)
{
    if (path.isEmpty())
        return;
    m_baseline.insert(path, sig);
}

FileChange FileChangeTracker::takeAction(const QString& path, const FileSignature& sig)
{
    const auto it = m_baseline.find(path);
    if (it == m_baseline.end())
        return FileChange::None;

    const FileSignature base = it.value();
    it.value() = sig;   // one prompt per physical change, however often we are called

    if (!sig.exists && base.exists)
        return FileChange::Deleted;      // it was there, now it is gone
    if (sig.exists && !base.exists)
        return FileChange::Modified;     // it came back: treat as a change to review
    if (sig == base)
        return FileChange::None;
    return FileChange::Modified;
}

void FileChangeTracker::forget(const QString& path)
{
    m_baseline.remove(path);
}

bool FileChangeTracker::isTracking(const QString& path) const
{
    return m_baseline.contains(path);
}
