// SPDX-License-Identifier: GPL-3.0-or-later
#include "BlockFinder.h"

namespace BlockFinder {

namespace {

bool isSkipped(const SkipProbe& skip, int line, int col)
{
    return skip && skip(line, col);
}

// The matching closer for an opening bracket, honouring nesting and skipping
// text the highlighter owns. Returns false when it never closes.
bool matchBracket(const QVector<QString>& lines, int startLine, int startCol,
                  QChar open, QChar close, const SkipProbe& skip, Block& out)
{
    int depth = 0;
    for (int line = startLine; line < lines.size(); ++line) {
        const QString& text = lines.at(line);
        const int from = (line == startLine) ? startCol : 0;
        for (int col = from; col < text.size(); ++col) {
            if (isSkipped(skip, line, col))
                continue;
            const QChar ch = text.at(col);
            if (ch == open) {
                ++depth;
            } else if (ch == close) {
                --depth;
                if (depth == 0) {
                    if (line == startLine)
                        return false;      // single-line: not foldable
                    out.valid = true;
                    out.startLine = startLine;
                    out.startCol = startCol;
                    out.endLine = line;
                    out.endCol = col + 1;
                    return true;
                }
            }
        }
    }
    return false;                       // never closes
}

QChar closerFor(QChar open)
{
    if (open == QLatin1Char('{'))
        return QLatin1Char('}');
    if (open == QLatin1Char('('))
        return QLatin1Char(')');
    return QLatin1Char(']');
}

bool isBracketOpen(QChar ch)
{
    return ch == QLatin1Char('{') || ch == QLatin1Char('(') || ch == QLatin1Char('[');
}

// Elements that never have a closing tag, so they can never open a block.
bool isVoidElement(const QString& name)
{
    static const char* const kVoid[] = {
        "area", "base", "br", "col", "embed", "hr", "img",
        "input", "link", "meta", "param", "source", "track", "wbr",
    };
    for (const char* v : kVoid) {
        if (name == QLatin1String(v))
            return true;
    }
    return false;
}

// Read a tag starting at the '<' at `col`. Fills `name`, `selfClosing` and
// `tagEnd` (the index just past the closing '>'). Returns false when this is not
// a start tag (a comment, a DOCTYPE, or a closing tag).
bool parseOpenTag(const QString& text, int col, QString& name, bool& selfClosing,
                  int& tagEnd)
{
    if (col >= text.size() || text.at(col) != QLatin1Char('<'))
        return false;
    int i = col + 1;
    if (i >= text.size())
        return false;
    // <!-- comment -->, <!DOCTYPE>, and <?xml ... ?> are not elements.
    if (text.at(i) == QLatin1Char('!') || text.at(i) == QLatin1Char('?'))
        return false;
    // </tag> closes, it does not open.
    if (text.at(i) == QLatin1Char('/'))
        return false;

    const int nameStart = i;
    while (i < text.size()
           && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('-')
               || text.at(i) == QLatin1Char(':')))
        ++i;
    if (i == nameStart)
        return false;
    name = text.mid(nameStart, i - nameStart).toLower();

    // Find the end of the tag, honouring quoted attribute values so a '>' inside
    // an attribute does not end it early.
    QChar quote;
    selfClosing = false;
    while (i < text.size()) {
        const QChar ch = text.at(i);
        if (!quote.isNull()) {
            if (ch == quote)
                quote = QChar();
        } else if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            quote = ch;
        } else if (ch == QLatin1Char('>')) {
            selfClosing = (i > col + 1 && text.at(i - 1) == QLatin1Char('/'));
            tagEnd = i + 1;
            return true;
        }
        ++i;
    }
    return false;                       // the tag does not finish on this line
}

// Match `<name>` opened at `startCol` to its closing `</name>`, counting nesting
// of the same tag name. Returns false when it never closes.
bool matchTag(const QVector<QString>& lines, int startLine, int startCol,
              const QString& name, const SkipProbe& skip, Block& out)
{
    int depth = 0;
    const QString closeTag = QStringLiteral("</") + name;
    for (int line = startLine; line < lines.size(); ++line) {
        const QString& text = lines.at(line);
        const int from = (line == startLine) ? startCol : 0;
        for (int col = from; col < text.size(); ++col) {
            if (isSkipped(skip, line, col))
                continue;
            if (text.at(col) != QLatin1Char('<'))
                continue;

            if (text.mid(col, closeTag.size()).toLower() == closeTag) {
                --depth;
                if (depth == 0) {
                    if (line == startLine)
                        return false;      // single-line: not foldable
                    int tagEnd = text.indexOf(QLatin1Char('>'), col);
                    if (tagEnd < 0)
                        tagEnd = text.size() - 1;
                    out.valid = true;
                    out.startLine = startLine;
                    out.startCol = startCol;
                    out.endLine = line;
                    out.endCol = tagEnd + 1;
                    return true;
                }
                continue;
            }

            QString openName;
            bool selfClosing = false;
            int tagEnd = 0;
            if (!parseOpenTag(text, col, openName, selfClosing, tagEnd))
                continue;
            if (openName != name)
                continue;
            if (selfClosing || isVoidElement(openName))
                continue;
            ++depth;
        }
    }
    return false;                       // never closes
}

} // namespace

Block findBlockStartingAt(const QVector<QString>& lines, int line,
                          const SkipProbe& skip)
{
    Block none;
    if (line < 0 || line >= lines.size())
        return none;
    const QString& text = lines.at(line);

    // Walk the line and remember the LAST opener that produced a real (multi-line)
    // block. That is what makes `int main(void) {` fold the brace body rather than
    // the argument list, while a multi-line call still folds on its paren.
    Block best;
    for (int col = 0; col < text.size(); ++col) {
        if (isSkipped(skip, line, col))
            continue;
        const QChar ch = text.at(col);

        if (isBracketOpen(ch)) {
            Block candidate;
            if (matchBracket(lines, line, col, ch, closerFor(ch), skip, candidate))
                best = candidate;
            continue;
        }

        if (ch == QLatin1Char('<')) {
            QString name;
            bool selfClosing = false;
            int tagEnd = 0;
            if (!parseOpenTag(text, col, name, selfClosing, tagEnd))
                continue;
            if (selfClosing || isVoidElement(name))
                continue;
            Block candidate;
            if (matchTag(lines, line, col, name, skip, candidate))
                best = candidate;
        }
    }
    return best;
}

} // namespace BlockFinder
