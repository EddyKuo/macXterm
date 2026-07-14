#include "ui/VaultDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace macxterm::ui {

QString VaultDialog::validate(Mode mode, const QString& pw, const QString& confirm) {
    if (pw.isEmpty()) return QStringLiteral("Password is required");
    if (mode == Mode::Create) {
        if (pw.size() < 8) return QStringLiteral("Master password must be at least 8 characters");
        if (pw != confirm) return QStringLiteral("Passwords do not match");
    }
    return {};
}

VaultDialog::VaultDialog(Mode mode, QWidget* parent) : QDialog(parent), m_mode(mode) {
    setWindowTitle(mode == Mode::Create ? QStringLiteral("Set Master Password")
                                        : QStringLiteral("Unlock Vault"));
    auto* form = new QFormLayout;
    m_pw = new QLineEdit(this);
    m_pw->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Master password"), m_pw);
    if (mode == Mode::Create) {
        m_confirm = new QLineEdit(this);
        m_confirm->setEchoMode(QLineEdit::Password);
        form->addRow(QStringLiteral("Confirm"), m_confirm);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &VaultDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString VaultDialog::password() const { return m_pw->text(); }

void VaultDialog::onAccept() {
    const QString err = validate(m_mode, m_pw->text(), m_confirm ? m_confirm->text() : QString());
    if (!err.isEmpty()) { QMessageBox::warning(this, windowTitle(), err); return; }
    accept();
}

} // namespace macxterm::ui
