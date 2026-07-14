#pragma once
#include "core/Settings.h"
#include <QDialog>

class QTabWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;

namespace macxterm::ui {

// Global settings dialog with General/Terminal/X11 tabs, mirroring MobaXterm's
// settings layout (UI_Spec, research §11). Reads/writes core::Settings.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const core::Settings& initial, QWidget* parent = nullptr);
    core::Settings settings() const;

private:
    QLineEdit* m_font = nullptr;
    QSpinBox*  m_fontSize = nullptr;
    QComboBox* m_scheme = nullptr;
    QSpinBox*  m_scrollback = nullptr;
    QCheckBox* m_x11Auto = nullptr;
    QCheckBox* m_x11Fwd = nullptr;
};

} // namespace macxterm::ui
