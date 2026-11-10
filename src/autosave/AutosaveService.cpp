// SPDX-License-Identifier: GPL-3.0-or-later
#include "AutosaveService.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

AutosaveService::AutosaveService(QObject* parent)
    : AutosaveService(QString(), parent)
{
}

AutosaveService::AutosaveService(const QString& autosaveDir, QObject* parent)
    : QObject(parent)
{
    m_dir = autosaveDir.isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                      + QStringLiteral("/crt-kate/autosave")
                : autosaveDir;
    m_timer.setInterval(120000);
    connect(&m_timer, &QTimer::timeout, this, &AutosaveService::runOneTick);
    // Recover anything left behind by a previous (crashed) run.
    scanExisting();
}

void AutosaveService::setAutosaveDir(const QString& dir) { m_dir = dir; }
QString AutosaveService::autosaveDir() const { return m_dir; }

QString AutosaveService::pathForId(int id) const
{
    return m_dir + QLatin1Char('/') + QString::number(id) + QStringLiteral(".txt");
}

void AutosaveService::registerDocument(const QUrl& url, const QString& bufferText)
{
    m_buffers.insert(url, bufferText);
}

void AutosaveService::updateBuffer(const QUrl& url, const QString& bufferText)
{
    m_buffers.insert(url, bufferText);
}

void AutosaveService::markModified(const QUrl& url) { m_modified.insert(url); }
void AutosaveService::markSaved(const QUrl& url) { m_modified.remove(url); }

void AutosaveService::start(int intervalMs)
{
    m_timer.setInterval(intervalMs);
    m_timer.start();
}

void AutosaveService::stop() { m_timer.stop(); }

void AutosaveService::runOneTick()
{
    QDir().mkpath(m_dir);
    for (const QUrl& url : std::as_const(m_modified)) {
        const QString text = m_buffers.value(url);
        RecoveryFile rec;
        rec.id = m_nextId++;
        rec.originalUrl = url;
        rec.autosavePath = pathForId(rec.id);
        rec.savedAtMs = QDateTime::currentMSecsSinceEpoch();
        QFile f(rec.autosavePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(text.toUtf8());
            f.close();
            m_pending.insert(rec.id, rec);
            emit recoveryAvailable(rec);
        }
    }
}

QList<AutosaveService::RecoveryFile> AutosaveService::pendingRecovery() const
{
    return m_pending.values();
}

bool AutosaveService::acceptRecovery(int id)
{
    if (!m_pending.contains(id))
        return false;
    QFile::remove(m_pending.value(id).autosavePath);
    m_pending.remove(id);
    return true;
}

bool AutosaveService::discardRecovery(int id)
{
    if (!m_pending.contains(id))
        return false;
    QFile::remove(m_pending.value(id).autosavePath);
    m_pending.remove(id);
    return true;
}

void AutosaveService::scanExisting()
{
    QDir d(m_dir);
    if (!d.exists())
        return;
    const QFileInfoList files = d.entryInfoList({ QStringLiteral("*.txt") },
                                                 QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        RecoveryFile rec;
        rec.id = fi.completeBaseName().toInt();
        rec.autosavePath = fi.absoluteFilePath();
        rec.savedAtMs = fi.lastModified().toMSecsSinceEpoch();
        // originalUrl is unknown for a cold-start scan (the doc isn't open yet);
        // the composition layer maps it when the user accepts recovery.
        if (rec.id >= m_nextId)
            m_nextId = rec.id + 1;
        m_pending.insert(rec.id, rec);
    }
}
