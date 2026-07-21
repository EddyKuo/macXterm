#include "ui/TunnelDialog.h"
#include "tunnel/TunnelForm.h"
#include <QVariantMap>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace macxterm::ui {

TunnelDialog::TunnelDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("SSH Tunnel"));
    auto* form = new QFormLayout;
    m_kind = new QComboBox(this);
    m_kind->addItems({"local", "remote", "dynamic"});
    m_bindAddr = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_bindPort = new QSpinBox(this); m_bindPort->setRange(0, 65535);
    m_targetHost = new QLineEdit(this);
    m_targetPort = new QSpinBox(this); m_targetPort->setRange(0, 65535);

    form->addRow(tr("Type"), m_kind);
    form->addRow(tr("Bind address"), m_bindAddr);
    form->addRow(tr("Bind port"), m_bindPort);
    form->addRow(tr("Target host"), m_targetHost);
    form->addRow(tr("Target port"), m_targetPort);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &TunnelDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

static QVariantMap fields(QComboBox* kind, QLineEdit* ba, QSpinBox* bp,
                          QLineEdit* th, QSpinBox* tp) {
    QVariantMap f;
    f.insert("kind", kind->currentText());
    f.insert("bindAddr", ba->text());
    f.insert("bindPort", bp->value());
    f.insert("targetHost", th->text());
    f.insert("targetPort", tp->value());
    return f;
}

tunnel::Tunnel TunnelDialog::tunnel() const {
    return tunnel::TunnelForm::toTunnel(
        fields(m_kind, m_bindAddr, m_bindPort, m_targetHost, m_targetPort));
}

void TunnelDialog::onAccept() {
    const QString err = tunnel::TunnelForm::validate(
        fields(m_kind, m_bindAddr, m_bindPort, m_targetHost, m_targetPort));
    if (!err.isEmpty()) { QMessageBox::warning(this, windowTitle(), err); return; }
    accept();
}

} // namespace macxterm::ui
