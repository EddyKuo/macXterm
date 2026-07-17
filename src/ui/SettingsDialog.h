#pragma once
#include "core/Settings.h"
#include <QDialog>

class QTabWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QFontComboBox;
class QLabel;

namespace macxterm::ui {

// Global settings dialog with General/Terminal/X11 tabs, mirroring MobaXterm's
// settings layout (UI_Spec, research §11). Reads/writes core::Settings.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const core::Settings& initial, QWidget* parent = nullptr);
    core::Settings settings() const;

private:
    // Re-render the font preview and recompute the Unicode-coverage line from the
    // currently-selected family + size.
    void updateFontPreview();

    QFontComboBox* m_font = nullptr;
    QSpinBox*  m_fontSize = nullptr;
    QComboBox* m_scheme = nullptr;
    QSpinBox*  m_scrollback = nullptr;
    QCheckBox* m_x11Auto = nullptr;
    QCheckBox* m_x11Fwd = nullptr;
    QLabel*    m_preview = nullptr;
    QLabel*    m_coverage = nullptr;
};

} // namespace macxterm::ui
