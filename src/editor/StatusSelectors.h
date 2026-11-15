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
    // The four cell widths (natural text width, shrunk proportionally when the
    // widget is narrower than the content) so no label ever overflows the row.
    std::array<int, 4> cellWidths() const;

    void openDocTypeMenu();
    void openEncodingMenu();
    void openSpellMenu();
    // Wire a freshly-built menu's show/hide to the context-hint swap signals.
    void attachHint(QMenu* menu, const QString& hint);

    bool m_ovr = false;
    KTextEditor::Document* m_doc = nullptr;
    KTextEditor::View* m_view = nullptr;
    QString m_spell;
    Sonnet::Speller* m_speller = nullptr;   // not owned
    QMenu* m_menu = nullptr;   // owned by this widget (parented)
};
