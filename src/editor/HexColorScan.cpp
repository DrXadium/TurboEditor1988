// SPDX-License-Identifier: GPL-3.0-or-later
#include "HexColorScan.h"

namespace {

bool isHexDigit(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'f')
        || (u >= u'A' && u <= u'F');
}

int hexValue(QChar c)
{
    const char16_t u = c.unicode();
    if (u >= u'0' && u <= u'9')
        return u - u'0';
    if (u >= u'a' && u <= u'f')
        return 10 + (u - u'a');
    return 10 + (u - u'A');
}

// Reads `count` hex digits at `from`. Returns -1 when any digit is missing or
// is not hex.
int readHex(const QString& s, int from, int count)
{
    if (from + count > s.size())
        return -1;
    int value = 0;
    for (int i = 0; i < count; ++i) {
        const QChar c = s.at(from + i);
        if (!isHexDigit(c))
            return -1;
        value = value * 16 + hexValue(c);
    }
    return value;
}

} // namespace

QVector<HexColorToken> scanHexColors(const QString& line)
{
    QVector<HexColorToken> out;
    const int n = line.size();
    for (int i = 0; i + 1 < n; ++i) {
        if (line.at(i) != QLatin1Char('#'))
            continue;

        // Six digits first: a long run must win over a short one, and a run
        // that continues past six digits is not a colour at all.
        const int six = readHex(line, i + 1, 6);
        if (six >= 0 && (i + 7 >= n || !isHexDigit(line.at(i + 7)))) {
            HexColorToken t;
            t.start = i;
            t.length = 7;
            t.color = QColor((six >> 16) & 0xff, (six >> 8) & 0xff, six & 0xff);
            out.append(t);
            i += 6;
            continue;
        }

        const int three = readHex(line, i + 1, 3);
        if (three >= 0 && (i + 4 >= n || !isHexDigit(line.at(i + 4)))) {
            // #abc means #aabbcc: each nibble is doubled.
            const int r = ((three >> 8) & 0xf) * 17;
            const int g = ((three >> 4) & 0xf) * 17;
            const int b = (three & 0xf) * 17;
            HexColorToken t;
            t.start = i;
            t.length = 4;
            t.color = QColor(r, g, b);
            out.append(t);
            i += 3;
        }
    }
    return out;
}

bool parseHexColor(const QString& text, QColor* out)
{
    if (!out)
        return false;
    QString s = text.trimmed();
    if (s.startsWith(QLatin1Char('#')))
        s.remove(0, 1);
    if (s.size() != 6 && s.size() != 3)
        return false;
    for (QChar c : s)
        if (!isHexDigit(c))
            return false;

    if (s.size() == 6) {
        const int v = readHex(s, 0, 6);
        *out = QColor((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
        return true;
    }
    const int v = readHex(s, 0, 3);
    *out = QColor(((v >> 8) & 0xf) * 17, ((v >> 4) & 0xf) * 17, (v & 0xf) * 17);
    return true;
}
