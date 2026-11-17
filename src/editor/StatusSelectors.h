// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <array>

namespace KTextEditor { class Document; class View; }
namespace Sonnet { class Speller; }
class QMenu;

// The four status-line indicators that live on the F-key row, right of the
// F-key text: spell language, encoding, document type (highlighting mode), and
// INS/OVR. Each is an independent click target that opens a TV-styled
// scrollable dropdown. Hosted by FKeyStatusBar (see the spec:
// docs/superpowers/specs/2026-11-14-status-line-selectors-design.md).
//
// The widget paints its own row segment (transparent background over the
// bar's gray) and is laid out by FKeyStatusBar. OFFSCREEN RENDER RULE: it must
// be show()n or QWidget::render skips it.
class StatusSelectors : public QWidget
{
    Q_OBJECT
public:
    explicit StatusSelectors(QWidget* parent = nullptr);

    // INS/OVR: OVR is drawn red (#AA0000), INS uses the bar's normal ink.
    void setOverwrite(bool ovr);
    bool overwrite() const { return m_ovr; }

    // The active document (doc-type + encoding) and view (overwrite) these
    // indicators reflect. Null clears them to placeholders.
    void setDocument(KTextEditor::Document* doc);
    void setView(KTextEditor::View* view);
    KTextEditor::Document* document() const { return m_doc; }
    KTextEditor::View* view() const { return m_view; }

    // The spell dictionary code (e.g. "en_US"); empty = unset.
    void setSpellLanguage(const QString& code);
    QString spellLanguage() const { return m_spell; }
    // The speller these indicators read the dictionary list from (not owned).
    void setSpeller(Sonnet::Speller* speller);
    Sonnet::Speller* speller() const { return m_speller; }

    // Label text accessors, exposed for tests.
    QString docTypeText() const;
    QString encodingText() const;
    QString spellText() const;
    QString overwriteText() const;   // "INS" or "OVR"

    // Click geometry (widget-local centre of each indicator) - exposed so tests
    // can drive the dropdowns with a synthetic click.
    QPoint docTypeCenter() const;
    QPoint encodingCenter() const;
    QPoint spellCenter() const;

    // The menu most recently opened by a click, for tests to inspect.
    QMenu* openMenu() const { return m_menu; }
    // The top-left the last dropdown was ASKED to open at (window-global, in
    // the same space as mapToGlobal). Exposed for tests: under Wayland the
    // compositor owns popup placement, so menu->pos() cannot be asserted, but
    // our own anchor math (where the far-left bug lived) can.
    QPoint lastPopupPos() const { return m_lastPopupPos; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    // Emitted when the user clicks the INS/OVR indicator (toggles overwrite).
    void overwriteToggled(bool ovr);
    // Emitted when the user picks a spell dictionary (the language code).
    void spellLanguageChanged(const QString& code);
    // A dropdown opened (spec 7: the F-key row swaps to a context hint while
    // it is open). The hint names what the dropdown is for.
    void dropdownOpened(const QString& hint);
    // The dropdown closed - the F-key row and the indicators are restored.
    void dropdownClosed();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    // Geometry of each indicator's click target, left to right.
    QRect spellRect() const;
    QRect encodingRect() const;
    QRect docTypeRect() const;
    QRect overwriteRect() const;

public:
    // Public, read-only geometry: tests assert that a dropdown hangs off the
    // indicator it was opened from.
    QRect docTypeCellRect() const { return docTypeRect(); }

private:
    // The four cell widths (natural text width, shrunk proportionally when the
    // widget is narrower than the content) so no label ever overflows the row.
    std::array<int, 4> cellWidths() const;

    void openDocTypeMenu();
    void openEncodingMenu();
    void openSpellMenu();
    // Wire a freshly-built menu's show/hide to the context-hint swap signals.
    void attachHint(QMenu* menu, const QString& hint);
    // Size + place a dropdown so it hangs off its indicator cell (right edge on
    // the cell's right edge, opening upward from the status row) and is CLAMPED
    // fully inside the window. Also caps the height so a long list scrolls
    // instead of running off the screen. Uses the EFFECTIVE size (sizeHint
    // bounded by the max-width/height caps) - the natural sizeHint is what made
    // the old clamp shove the popup to the window's left edge.
    void popupAnchored(QMenu* menu, const QRect& cell);

    bool m_ovr = false;
    KTextEditor::Document* m_doc = nullptr;
    KTextEditor::View* m_view = nullptr;
    QString m_spell;
    Sonnet::Speller* m_speller = nullptr;   // not owned
    QMenu* m_menu = nullptr;   // owned by this widget (parented)
    QPoint m_lastPopupPos;     // where the last dropdown was asked to open
};
