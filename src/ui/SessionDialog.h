#pragma once
#include "core/Session.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;

namespace macxterm::ui {

// Session editor dialog (UI_Spec Session Manager). Thin wrapper over
// core::SessionForm — the field↔Session mapping and validation are tested there;
// this class only wires widgets. Accept is blocked while validation fails.
class SessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDialog(QWidget* parent = nullptr);

    void setSession(const core::Session& s);
    core::Session session() const;

private slots:
    void onAccept();

private:
    QLineEdit* m_name = nullptr;
    QComboBox* m_type = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox*  m_port = nullptr;
    QLineEdit* m_user = nullptr;
};

} // namespace macxterm::ui
