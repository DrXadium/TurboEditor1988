// SPDX-License-Identifier: GPL-3.0-or-later
#include "Checksum.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>

QByteArray Checksum::sha256File(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f))
        return {};
    return hash.result().toHex();
}

bool Checksum::verifyFile(const QString& path, const QString& expectedHex)
{
    const QByteArray actual = sha256File(path);
    if (actual.isEmpty())
        return false;                       // unreadable file, never a match
    // A .sha256 line is "<hex>  <name>"; take the first whitespace token.
    const QStringList parts =
        expectedHex.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                    Qt::SkipEmptyParts);
    const QString want = parts.value(0);
    if (want.isEmpty())
        return false;
    return actual == want.toUtf8().toLower();
}
