// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>

// SHA-256 verification of a downloaded file against the release's .sha256
// digest, so a corrupted/partial download can never replace the installed app.
namespace Checksum {

// Lowercase hex SHA-256 of the file's bytes. Empty if the file can't be read.
QByteArray sha256File(const QString& path);

// Compare the file's SHA-256 to a hex digest (case-insensitive; the digest may
// come as "<hex>  <name>" — only the first token is used). False if the file is
// unreadable or the digests differ.
bool verifyFile(const QString& path, const QString& expectedHex);

} // namespace Checksum
