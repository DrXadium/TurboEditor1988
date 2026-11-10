// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QTimer>

// Periodic autosave + crash recovery.
// Registered documents that are marked modified get snapshotted to
// <autosaveDir>/<id>.txt on each tick. pendingRecovery() lists snapshots on
// disk (discovered at construction — i.e. from a previous crashed run), and
// accept/discard clears them.
//
// Design note: the "modified" flag is owned by the editor (KTextEditor). This
// service only knows a URL + whether to snapshot; the composition root calls
// markModified() when a document changes.
class AutosaveService : public QObject
{
    Q_OBJECT
public:
    struct RecoveryFile {
        int id = 0;
        QUrl originalUrl;
        QString autosavePath;
        qint64 savedAtMs = 0;
    };

    explicit AutosaveService(QObject* parent = nullptr);
    // Construct already pointed at a store (cold start: scans immediately).
    explicit AutosaveService(const QString& autosaveDir, QObject* parent = nullptr);

    void setAutosaveDir(const QString& dir);
    QString autosaveDir() const;
    // (Re)scan the store for recovery files left by a previous run.
    void scanExisting();

    void registerDocument(const QUrl& url, const QString& bufferText);
    void updateBuffer(const QUrl& url, const QString& bufferText);
    void markModified(const QUrl& url);
    void markSaved(const QUrl& url);

    void start(int intervalMs = 120000);
    void stop();
    // Deterministic single pass (used by tests); the timer calls this.
    void runOneTick();

    QList<RecoveryFile> pendingRecovery() const;
    bool acceptRecovery(int id);
    bool discardRecovery(int id);

signals:
    void recoveryAvailable(const AutosaveService::RecoveryFile& file);

private:
    QString pathForId(int id) const;

    QString m_dir;
    QHash<QUrl, QString> m_buffers;
    QSet<QUrl> m_modified;
    QHash<int, RecoveryFile> m_pending;
    int m_nextId = 1;
    QTimer m_timer;
};
