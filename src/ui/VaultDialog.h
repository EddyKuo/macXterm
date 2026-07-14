#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

namespace macxterm::ui {

// Master-password dialog for the credential vault (UI_Spec flow E). In "create"
// mode it asks for a password twice and enforces a match + minimum length; in
// "unlock" mode it asks once. The validation rule is a static, testable helper.
class VaultDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Create, Unlock };
    explicit VaultDialog(Mode mode, QWidget* parent = nullptr);

    QString password() const;

    // Returns empty if OK, else an error. For Create, `confirm` must match and
    // the password must be at least 8 chars; for Unlock, just non-empty.
    static QString validate(Mode mode, const QString& pw, const QString& confirm);

private slots:
    void onAccept();

private:
    Mode m_mode;
    QLineEdit* m_pw = nullptr;
    QLineEdit* m_confirm = nullptr;
};

} // namespace macxterm::ui
