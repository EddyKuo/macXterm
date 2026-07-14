#include "ui/SessionDialog.h"
#include "core/SessionForm.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace macxterm::ui {

SessionDialog::SessionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Session"));
    auto* form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_type = new QComboBox(this);
    for (auto t : {core::SessionType::Ssh, core::SessionType::Telnet, core::SessionType::Serial,
                   core::SessionType::Mosh, core::SessionType::Rdp, core::SessionType::Vnc,
                   core::SessionType::Shell}) {
        m_type->addItem(core::sessionTypeToString(t));
    }
    m_host = new QLineEdit(this);
    m_port = new QSpinBox(this);
    m_port->setRange(0, 65535);
    m_user = new QLineEdit(this);

    form->addRow(QStringLiteral("Name"), m_name);
    form->addRow(QStringLiteral("Type"), m_type);
    form->addRow(QStringLiteral("Host"), m_host);
    form->addRow(QStringLiteral("Port"), m_port);
    form->addRow(QStringLiteral("Username"), m_user);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SessionDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SessionDialog::setSession(const core::Session& s) {
    const QVariantMap f = core::SessionForm::fromSession(s);
    m_name->setText(f.value("name").toString());
    m_type->setCurrentText(f.value("type").toString());
    m_host->setText(f.value("host").toString());
    m_port->setValue(f.value("port").toInt());
    m_user->setText(f.value("username").toString());
}

core::Session SessionDialog::session() const {
    QVariantMap f;
    f.insert("name", m_name->text());
    f.insert("type", m_type->currentText());
    f.insert("host", m_host->text());
    if (m_port->value() > 0) f.insert("port", m_port->value());
    f.insert("username", m_user->text());
    return core::SessionForm::toSession(f);
}

void SessionDialog::onAccept() {
    QVariantMap f;
    f.insert("name", m_name->text());
    f.insert("type", m_type->currentText());
    f.insert("host", m_host->text());
    f.insert("port", m_port->value() > 0 ? QString::number(m_port->value()) : QString());
    const QString err = core::SessionForm::validate(f);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid session"), err);
        return;
    }
    accept();
}

} // namespace macxterm::ui
