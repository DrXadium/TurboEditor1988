// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRegularExpression>
#include <algorithm>
#include "src/AppIdentity.h"

namespace {
const char kLastFile[] = "last-session";
// Autosave round: keep-last-10 timestamped snapshots (named sessions exempt).
constexpr int kMaxAutoSnapshots = 10;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    // Default store: <ConfigLocation>/<storageDirName>/sessions/<name>.json
    //
    // Pinned to the LEGACY storage id rather than AppConfigLocation: the app is
    // displayed as "TURBO Kate", but its sessions already live under
    // ~/.config/CRT-Kate and there is no migration. Using AppConfigLocation
    // here would follow the display name and orphan every saved session.
    m_dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + QLatin1Char('/') + AppIdentity::storageDirName()
            + QStringLiteral("/sessions");

    // Auto-save on quit: re-snapshot whatever the last restore/save named.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        const QString last = lastSession();
        if (!last.isEmpty() && !m_entries.isEmpty())
            saveSession(last);
    });
}

void SessionManager::setSessionsDir(const QString& dir) { m_dir = dir; }
QString SessionManager::sessionsDir() const { return m_dir; }

void SessionManager::setEntries(const QVector<Entry>& entries) { m_entries = entries; }
QVector<SessionManager::Entry> SessionManager::entries() const { return m_entries; }

QString SessionManager::filePathFor(const QString& name) const
{
    return m_dir + QLatin1Char('/') + name + QStringLiteral(".json");
}

void SessionManager::setLastSession(const QString& name)
{
    QDir().mkpath(m_dir);
    QFile f(m_dir + QLatin1Char('/') + QLatin1String(kLastFile));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(name.toUtf8());
}

QString SessionManager::takeSnapshot(const QDateTime& when)
{
    const QDateTime at = when.isValid() ? when : QDateTime::currentDateTime();
    const QString name = QStringLiteral("auto-%1-%2")
                             .arg(at.date().toString(QStringLiteral("yyyyMMdd")),
                                  at.time().toString(QStringLiteral("HHmmss")));
    saveSession(name);
    pruneAutoSnapshots();
    return name;
}

void SessionManager::pruneAutoSnapshots()
{
    // Keep the newest kMaxAutoSnapshots only; the timestamped names sort
    // chronologically, so an ascending name listing plus a forward delete gives
    // exactly that. Named sessions never match the auto- prefix.
    auto autos = QStringList(sessionNames())
                     .filter(QRegularExpression(QStringLiteral("^auto-")));
    std::sort(autos.begin(), autos.end());          // oldest first
    while (autos.size() > kMaxAutoSnapshots) {
        QFile::remove(filePathFor(autos.takeFirst()));
    }
}

QString SessionManager::lastSession() const
{
    QFile f(m_dir + QLatin1Char('/') + QLatin1String(kLastFile));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

void SessionManager::saveSession(const QString& name)
{
    QDir().mkpath(m_dir);
    QJsonArray arr;
    const auto entries = m_entries;
    for (const Entry& e : entries) {
        QJsonObject o;
        o.insert(QStringLiteral("fileUrl"), e.fileUrl.toString());
        o.insert(QStringLiteral("line"), e.line);
        o.insert(QStringLiteral("column"), e.column);
        o.insert(QStringLiteral("activeIndex"), e.activeIndex);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("entries"), arr);
    QFile f(filePathFor(name));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
        setLastSession(name);
    }
}

bool SessionManager::restoreSession(const QString& name)
{
    QFile f(filePathFor(name));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return false;
    QVector<Entry> out;
    const QJsonArray arr = doc.object().value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.fileUrl = QUrl(o.value(QStringLiteral("fileUrl")).toString());
        e.line = o.value(QStringLiteral("line")).toInt();
        e.column = o.value(QStringLiteral("column")).toInt();
        e.activeIndex = o.value(QStringLiteral("activeIndex")).toInt();
        out.append(e);
    }
    m_entries = out;
    setLastSession(name);
    return true;
}

QStringList SessionManager::sessionNames() const
{
    QStringList names;
    const QFileInfoList files = QDir(m_dir).entryInfoList(
        { QStringLiteral("*.json") }, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files)
        names << fi.completeBaseName();
    return names;
}
