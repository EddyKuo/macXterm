#pragma once
#include "term/ColorScheme.h"
#include <QDialog>

class QPushButton;

namespace macxterm::ui {

// Graphical terminal color-scheme editor. Starts from a built-in scheme and lets
// the user recolor the foreground, background and the 16 ANSI palette entries.
// Emits schemeChosen() live so the caller can preview it on the active pane.
class ColorSchemeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColorSchemeDialog(const term::ColorScheme& initial, QWidget* parent = nullptr);
    term::ColorScheme scheme() const { return m_scheme; }

signals:
    void schemeChosen(const term::ColorScheme& scheme);

private:
    void refreshSwatches();
    void pickColor(int which);   // -1 fg, -2 bg, 0..15 ansi

    term::ColorScheme m_scheme;
    QPushButton* m_fgBtn = nullptr;
    QPushButton* m_bgBtn = nullptr;
    QPushButton* m_ansiBtn[16] = {};
};

} // namespace macxterm::ui
