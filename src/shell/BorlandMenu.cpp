// SPDX-License-Identifier: GPL-3.0-or-later
#include "BorlandMenu.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHideEvent>

namespace {
const QColor kMenuGray(0xAA, 0xAA, 0xAA);
const QColor kSelectGreen(0x00, 0xAA, 0x00);   // spec 6 highlight
const QColor kBlack(0x00, 0x00, 0x00);
const QColor kHotRed(0xAA, 0x00, 0x00);
const QColor kWhite(0xFF, 0xFF, 0xFF);

// QAction carries no font and no "plain row" flag, and the spell section must
// not invent a subclass of QMenu just to hold them. Action PROPERTIES hold
// both values on the action itself, so every BorlandMenu sees them - the
// context menu and its popout alike. (Qt6 QAction has no role-based setData.)
constexpr const char* kRowFontProp = "borlandRowFont";
constexpr const char* kRowNoMnemonicProp = "borlandRowNoMnemonic";
constexpr const char* kRowBoldProp = "borlandRowBold";
} // namespace

void BorlandMenu::setRowFont(QAction* a, const QFont& font)
{
    if (a)
        a->setProperty(kRowFontProp, font);
}

QFont BorlandMenu::rowFont(QAction* a, const QFont& fallback)
{
    if (a) {
        const QVariant v = a->property(kRowFontProp);
        if (v.isValid() && v.canConvert<QFont>())
            return v.value<QFont>();
    }
    return fallback;
}

void BorlandMenu::setRowNoMnemonic(QAction* a, bool on)
{
    if (a)
        a->setProperty(kRowNoMnemonicProp, on);
}

bool BorlandMenu::rowNoMnemonic(QAction* a)
{
    return a && a->property(kRowNoMnemonicProp).toBool();
}

void BorlandMenu::setRowBold(QAction* a, bool on)
{
    if (a)
        a->setProperty(kRowBoldProp, on);
}

bool BorlandMenu::rowBold(QAction* a)
{
    return a && a->property(kRowBoldProp).toBool();
}

BorlandMenu::BorlandMenu(QWidget* parent) : QMenu(parent)
{
    setWindowFlags(windowFlags() | Qt::NoDropShadowWindowHint);
    // Reference screenshot: the gray panel carries a SINGLE black border set
    // WELL INSIDE the panel edge (the gray runs outside it on all four sides),
    // with a little padding between that border and the item text. Both the
    // inset and the padding are content margins, so Qt sizes the popup to
    // include them and every actionGeometry is already inside the border.
    const QFontMetrics fm = fontMetrics();
    const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    const int m = borderInset() + borderWidth() + 2;   // gray margin + stroke + padding
    setContentsMargins(m, m, m, m);
}

// --- scrolling geometry ------------------------------------------------------

QList<QAction*> BorlandMenu::visibleActions() const
{
    QList<QAction*> out;
    const auto all = actions();
    for (QAction* a : all)
        if (a->isVisible())
            out.append(a);
    return out;
}

// Uniform row height for the self-scrolling layout. Derived from the font, NOT
// from QMenu's actionGeometry: real menus mix item heights (a separator is
// thinner than an item) and use asymmetric margins, so a row model built from
// the first action overestimates the content - which made ordinary menus think
// they overflowed and sent them down the scroll path.
int BorlandMenu::scrollRowHeight() const
{
    return qMax(8, fontMetrics().height() + 6);
}

// One row's height: our own uniform row when scrolling, QMenu's own item height
// otherwise (short menus keep QMenu's exact layout).
int BorlandMenu::rowHeight() const
{
    if (scrolls())
        return scrollRowHeight();
    const auto vis = visibleActions();
    if (vis.isEmpty())
        return qMax(1, fontMetrics().height());
    const int h = actionGeometry(vis.first()).height();
    return h > 0 ? h : qMax(1, fontMetrics().height());
}

// Top of the first row, in content coordinates (QMenu's own top margin).
int BorlandMenu::contentTop() const
{
    if (scrolls())
        return borderInset() + borderWidth() + 2;   // our own symmetric margin
    const auto vis = visibleActions();
    if (vis.isEmpty())
        return contentsMargins().top();
    return actionGeometry(vis.first()).top();
}

int BorlandMenu::contentHeight() const
{
    const int n = visibleActions().size();
    if (n == 0)
        return height();
    return 2 * contentTop() + n * rowHeight();
}

// A menu scrolls only when its height was DELIBERATELY capped (popupAnchored
// calls setMaximumHeight) and the single-column content does not fit in that
// cap. Deciding this from the current height alone misfired for every ordinary
// menu whose margins happen not to be symmetric.
bool BorlandMenu::scrolls() const
{
    if (maximumHeight() >= QWIDGETSIZE_MAX)
        return false;
    const int n = visibleActions().size();
    if (n == 0)
        return false;
    const int top = borderInset() + borderWidth() + 2;
    return 2 * top + n * scrollRowHeight() > maximumHeight();
}

int BorlandMenu::maxScroll() const
{
    return qMax(0, contentHeight() - height());
}

QRect BorlandMenu::rowRect(int index) const
{
    return QRect(0, contentTop() + index * rowHeight(), width(), rowHeight());
}

int BorlandMenu::rowAt(int viewportY) const
{
    if (!scrolls())
        return -1;
    const int rel = viewportY + m_scroll - contentTop();
    if (rel < 0)
        return -1;
    const int idx = rel / rowHeight();
    return idx < visibleActions().size() ? idx : -1;
}

void BorlandMenu::scrollBy(int dy)
{
    const int next = qBound(0, m_scroll + dy, maxScroll());
    if (next == m_scroll)
        return;
    m_scroll = next;
    update();
}

void BorlandMenu::ensureVisible(int index)
{
    if (index < 0 || !scrolls())
        return;
    const QRect r = rowRect(index);
    const int top = r.top() - contentTop();          // relative to the content top
    const int viewport = height() - 2 * contentTop(); // usable rows area
    if (top < m_scroll)
        scrollBy(top - m_scroll);
    else if (top + r.height() > m_scroll + viewport)
        scrollBy(top + r.height() - (m_scroll + viewport));
}

void BorlandMenu::activateRow(int index)
{
    const auto vis = visibleActions();
    if (index < 0 || index >= vis.size())
        return;
    QAction* a = vis.at(index);
    if (!a || a->isSeparator() || !a->isEnabled())
        return;
    setActiveAction(a);
    emit triggered(a);
    hide();
}

void BorlandMenu::paintScrollBar(QPainter& p) const
{
    const QRect thumb = scrollThumbRect();
    if (thumb.isEmpty())
        return;
    // Solid black thumb on the gray panel: reads like the menu's own border.
    p.fillRect(thumb, kBlack);
}

// The track column, just inside the border. Empty when the menu does not
// scroll, so callers can test containment without a separate flag.
QRect BorlandMenu::scrollBarRect() const
{
    if (!scrolls())
        return QRect();
    const int trackW = qMax(3, qMax(1, fontMetrics().horizontalAdvance(QLatin1Char('M'))) / 2);
    const int x = width() - borderInset() - borderWidth() - trackW - 1;
    const int y0 = borderInset() + borderWidth() + 1;
    const int trackH = height() - 2 * (borderInset() + borderWidth() + 1);
    if (trackH <= 0)
        return QRect();
    return QRect(x, y0, trackW, trackH);
}

QRect BorlandMenu::scrollThumbRect() const
{
    const QRect track = scrollBarRect();
    if (track.isEmpty())
        return QRect();
    const int thumbH = qBound(8, int(qint64(track.height()) * height() / qMax(1, contentHeight())),
                              track.height());
    const int maxS = qMax(1, maxScroll());
    const int thumbY = track.top() + int(qint64(track.height() - thumbH) * m_scroll / maxS);
    return QRect(track.left(), thumbY, track.width(), thumbH);
}

// --- events ------------------------------------------------------------------

void BorlandMenu::wheelEvent(QWheelEvent* ev)
{
    if (!scrolls()) {
        QMenu::wheelEvent(ev);
        return;
    }
    scrollBy(-ev->angleDelta().y());
    ev->accept();
}

void BorlandMenu::mousePressEvent(QMouseEvent* ev)
{
    if (!scrolls()) {
        QMenu::mousePressEvent(ev);
        return;
    }
    // The scrollbar owns its column: a press there must NEVER pick a row (that
    // is what made "clicking on the scroll" pick an entry instead of scrolling).
    const QRect track = scrollBarRect();
    if (track.contains(ev->pos())) {
        const QRect thumb = scrollThumbRect();
        if (thumb.contains(ev->pos())) {
            m_draggingThumb = true;
            m_dragGrabOffset = ev->pos().y() - thumb.top();
        } else {
            // Track click: page one viewport toward the click.
            const int page = qMax(1, height() - 2 * contentTop());
            scrollBy(ev->pos().y() < thumb.top() ? -page : page);
        }
        ev->accept();
        return;
    }
    const int idx = rowAt(ev->pos().y());
    if (idx >= 0)
        activateRow(idx);
    ev->accept();
}

void BorlandMenu::mouseReleaseEvent(QMouseEvent* ev)
{
    if (!scrolls()) {
        QMenu::mouseReleaseEvent(ev);
        return;
    }
    m_draggingThumb = false;
    ev->accept();
}

void BorlandMenu::mouseMoveEvent(QMouseEvent* ev)
{
    if (!scrolls()) {
        QMenu::mouseMoveEvent(ev);
        return;
    }
    const QRect track = scrollBarRect();
    if (m_draggingThumb) {
        // Map the thumb position back onto the scroll range, keeping the grab
        // point under the cursor so the thumb does not jump on the first move.
        const QRect thumb = scrollThumbRect();
        const int span = qMax(1, track.height() - thumb.height());
        const int top = qBound(0, ev->pos().y() - m_dragGrabOffset - track.top(), span);
        const int next = qBound(0, int(qint64(top) * maxScroll() / span), maxScroll());
        if (next != m_scroll) {
            m_scroll = next;
            update();
        }
        ev->accept();
        return;
    }
    // Hovering the scrollbar must not highlight (or later pick) the row that
    // happens to sit beside it.
    if (track.contains(ev->pos())) {
        ev->accept();
        return;
    }
    const int idx = rowAt(ev->pos().y());
    const auto vis = visibleActions();
    if (idx >= 0 && idx < vis.size())
        setActiveAction(vis.at(idx));
    update();
    ev->accept();
}

void BorlandMenu::keyPressEvent(QKeyEvent* ev)
{
    if (!scrolls()) {
        QMenu::keyPressEvent(ev);
        return;
    }
    const auto vis = visibleActions();
    int cur = -1;
    for (int i = 0; i < vis.size(); ++i)
        if (vis.at(i) == activeAction()) { cur = i; break; }

    auto step = [&](int dir) {
        int i = cur;
        for (int n = 0; n < vis.size(); ++n) {
            i += dir;
            if (i < 0 || i >= vis.size())
                return;
            if (!vis.at(i)->isSeparator() && vis.at(i)->isEnabled()) {
                setActiveAction(vis.at(i));
                ensureVisible(i);
                update();
                return;
            }
        }
    };

    switch (ev->key()) {
    case Qt::Key_Down:   step(+1); return;
    case Qt::Key_Up:     step(-1); return;
    case Qt::Key_Home:
        for (int i = 0; i < vis.size(); ++i)
            if (!vis.at(i)->isSeparator() && vis.at(i)->isEnabled()) {
                setActiveAction(vis.at(i)); ensureVisible(i); update(); break;
            }
        return;
    case Qt::Key_End:
        for (int i = vis.size() - 1; i >= 0; --i)
            if (!vis.at(i)->isSeparator() && vis.at(i)->isEnabled()) {
                setActiveAction(vis.at(i)); ensureVisible(i); update(); break;
            }
        return;
    case Qt::Key_PageDown: scrollBy(height() - 2 * contentTop()); return;
    case Qt::Key_PageUp:   scrollBy(-(height() - 2 * contentTop())); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (QAction* a = activeAction()) {
            emit triggered(a);
            hide();
        }
        return;
    case Qt::Key_Escape:
        hide();
        return;
    default:
        QMenu::keyPressEvent(ev);
        return;
    }
}

void BorlandMenu::hideEvent(QHideEvent* ev)
{
    m_scroll = 0;   // a re-opened popup always starts at the top
    QMenu::hideEvent(ev);
}

// --- painting ----------------------------------------------------------------

void BorlandMenu::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // Solid gray panel, flat fills only.
    p.fillRect(rect(), kMenuGray);
    // Black ASCII border INSET inside the panel (gray runs outside it on every
    // side). Drawn as four filled strips rather than a stroked rect: a 2px pen
    // straddles the path and lands on half pixels, which shows up as a ragged
    // 1/2px line at this size.
    const int inset = borderInset();
    const int bw = borderWidth();
    const int right = width() - inset - bw;
    const int bottom = height() - inset - bw;
    p.fillRect(QRect(inset, inset, width() - 2 * inset, bw), kBlack);          // top
    p.fillRect(QRect(inset, bottom, width() - 2 * inset, bw), kBlack);         // bottom
    p.fillRect(QRect(inset, inset, bw, height() - 2 * inset), kBlack);         // left
    p.fillRect(QRect(right, inset, bw, height() - 2 * inset), kBlack);         // right

    const QFontMetrics fm = fontMetrics();
    const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
    // Item band spans the area just inside the stroke, edge to edge.
    const int contentLeft = inset + bw;
    // When the list scrolls a scrollbar column is reserved on the right so text
    // never runs under the thumb.
    const bool sc = scrolls();
    const int sbW = sc ? qMax(3, cell / 2) + 2 : 0;
    const int contentRight = right - 1 - sbW;

    const auto vis = visibleActions();
    if (sc) {
        p.save();
        p.setClipRect(QRect(inset + bw, inset + bw,
                            width() - 2 * (inset + bw), height() - 2 * (inset + bw)));
        p.translate(0, -m_scroll);
    }

    for (int i = 0; i < vis.size(); ++i) {
        QAction* a = vis.at(i);
        const QRect r = sc ? rowRect(i) : actionGeometry(a);

        // Separator: one full-width single black line across the content area.
        if (a->isSeparator()) {
            // Divider matches the border stroke thickness (reference: the rule
            // and the box read as the same weight).
            p.fillRect(QRect(contentLeft, r.center().y() - bw / 2,
                             contentRight - contentLeft + 1, bw), kBlack);
            continue;
        }

        const bool selected = (a == activeAction()) && a->isEnabled();
        // Spec 6: the highlighted row is a solid green bar spanning the FULL
        // width of the dropdown, exactly one cell tall.
        // The highlight sits inside the border with a small gray gap on BOTH
        // sides (reference screenshot: ~3 px of panel gray either side of the
        // green bar), so it never butts up against the stroke.
        const int hi = highlightInset();
        p.fillRect(QRect(contentLeft + hi, r.top(),
                         contentRight - contentLeft + 1 - 2 * hi, r.height()),
                   selected ? kSelectGreen : kMenuGray);

        // A row may carry its OWN font (the misspelled word is bold). Metrics
        // for text and its underline must come from THAT face, or the bold row
        // would be positioned with the regular face's ascent.
        const QFont rowF = rowFont(a, font());
        const bool plainRow = rowNoMnemonic(a);
        const bool boldRow = rowBold(a);
        const QFontMetrics rowFm(rowF);
        p.setFont(rowF);
        const int baseline = r.y() + (r.height() - rowFm.height()) / 2 + rowFm.ascent();

        // Task 26 (user): the \u2713 goes at the RIGHT end of the item, one cell in
        // from the content edge. Drawn at the panel's left edge it collided with
        // the label; the label's column stays clean now, so the check column is
        // reserved on the right instead.
        const bool showCheck = a->isCheckable() && a->isChecked();
        int shortcutRight = contentRight - cell;
        if (showCheck) {
            const int cw = fm.horizontalAdvance(QStringLiteral("\u2713"));
            p.setPen(kBlack);
            p.drawText(QPoint(contentRight - cell - cw + 1, baseline),
                       QStringLiteral("\u2713"));
            // A shortcut on a checked item moves left of the check, so the two
            // never overlap.
            shortcutRight -= cw + cell;
        }

        // "label\tshortcut" (Qt's convention for menu display text). The
        // shortcut is drawn black, right-aligned, one cell of right padding.
        QString label = a->text();
        QString shortcut;
        const int tab = label.indexOf(QLatin1Char('\t'));
        if (tab >= 0) {
            shortcut = label.mid(tab + 1);
            label.truncate(tab);
        } else if (!a->shortcut().isEmpty()) {
            shortcut = a->shortcut().toString(QKeySequence::NativeText);
        }

        int x = contentLeft + cell;
        bool firstVisible = true;
        const QColor textColor = selected || a->isEnabled() ? kBlack : kBlack;
        if (plainRow) {
            // A word, not a command: draw it literally in black (or white on
            // the green bar) with no hotkey colour, no underline, and '&' left
            // as the letter it is.
            p.setPen(selected ? kWhite : textColor);
            p.drawText(QPoint(x, baseline), label);
            // Double-strike: the fixed-pitch TV face has no wider bold, so
            // bold is fatter - draw the same glyphs 1px right and the two
            // strokes fuse into a heavy word.
            if (boldRow)
                p.drawText(QPoint(x + 1, baseline), label);
            firstVisible = false;
        }
        for (int ci = 0; ci < label.size(); ++ci) {
            if (plainRow)
                break;
            const QChar c = label.at(ci);
            if (c == QLatin1Char('&')) {
                if (ci + 1 < label.size() && label.at(ci + 1) == QLatin1Char('&')) {
                    p.setPen(firstVisible ? kHotRed : (selected ? kWhite : textColor));
                    p.drawText(QPoint(x, baseline), QStringLiteral("&"));
                    x += rowFm.horizontalAdvance(QLatin1Char('&'));
                    firstVisible = false;
                    ++ci;
                }
                continue;   // mnemonic marker
            }
            // Spec 6: the hotkey letter stays RED on the green bar too.
            p.setPen(firstVisible ? kHotRed : (selected ? kWhite : textColor));
            p.drawText(QPoint(x, baseline), QString(c));
            // Screenshot styling: the hotkey letter is underlined.
            if (firstVisible) {
                const int w = rowFm.horizontalAdvance(c);
                p.drawLine(x, baseline + 2, x + w - 1, baseline + 2);
            }
            x += rowFm.horizontalAdvance(c);
            firstVisible = false;
        }

        p.setFont(font());   // restore: shortcuts use the menu's own face
        if (!shortcut.isEmpty() && !plainRow) {
            p.setPen(kBlack);
            const int sw = fm.horizontalAdvance(shortcut);
            p.drawText(QPoint(shortcutRight - sw, baseline), shortcut);
        }
    }

    if (sc) {
        p.restore();
        paintScrollBar(p);
    }
}
