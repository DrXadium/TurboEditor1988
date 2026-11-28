// SPDX-License-Identifier: GPL-3.0-or-later
#include "VersionCompare.h"
#include <QRegularExpression>
#include <QVector>

namespace {

// "v0.1.4-2-gabc123" -> {"0","1","4"}; unparseable -> empty.
QVector<int> segments(const QString& v)
{
    static const QRegularExpression re(QStringLiteral("^v?(\\d+(?:\\.\\d+)*)(?:[-+].*)?$"));
    const auto m = re.match(v.trimmed());
    if (!m.hasMatch())
        return {};
    QVector<int> out;
    for (const QString& part : m.captured(1).split(QLatin1Char('.')))
        out.append(part.toInt());
    return out;
}

} // namespace

bool VersionCompare::isNewer(const QString& candidate, const QString& current)
{
    const QVector<int> a = segments(candidate);
    const QVector<int> b = segments(current);
    if (a.isEmpty() || b.isEmpty())
        return false;                     // unparseable never wins
    const int n = qMin(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i])
            return a[i] > b[i];
    }
    // Equal prefix: longer (more specific) is newer, e.g. 0.1.4 > 0.1.
    return a.size() > b.size();
}
