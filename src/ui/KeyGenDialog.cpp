#include "ui/KeyGenDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QFileDialog>
#include <QFile>
#include <QDir>

namespace macxterm::ui {

KeyGenDialog::KeyGenDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("SSH Key Generator"));
    resize(520, 360);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_type = new QComboBox(this);
    m_type->addItems({QStringLiteral("ed25519"), QStringLiteral("ecdsa"), QStringLiteral("rsa (4096)")});
    form->addRow(tr("Type"), m_type);

    m_path = new QLineEdit(QDir::homePath() + QStringLiteral("/.ssh/id_macxterm"), this);
    auto* browse = new QPushButton(tr("Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &KeyGenDialog::browse);
    auto* pathRow = new QWidget(this);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(m_path);
    pathLayout->addWidget(browse);
    form->addRow(tr("Output file"), pathRow);

    m_passphrase = new QLineEdit(this);
    m_passphrase->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Passphrase"), m_passphrase);
    layout->addLayout(form);

    auto* gen = new QPushButton(tr("Generate"), this);
    connect(gen, &QPushButton::clicked, this, &KeyGenDialog::generate);
    layout->addWidget(gen);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    layout->addWidget(m_output, 1);
}

void KeyGenDialog::browse() {
    const QString p = QFileDialog::getSaveFileName(this, tr("Key file"), m_path->text());
    if (!p.isEmpty()) m_path->setText(p);
}

void KeyGenDialog::generate() {
    QString type = m_type->currentText();
    if (type.startsWith(QStringLiteral("rsa"))) type = QStringLiteral("rsa");
    const QString path = m_path->text();

    QStringList args{QStringLiteral("-t"), type, QStringLiteral("-f"), path,
                     QStringLiteral("-N"), m_passphrase->text()};
    if (type == QStringLiteral("rsa")) args << QStringLiteral("-b") << QStringLiteral("4096");

    QProcess proc;
    proc.start(QStringLiteral("ssh-keygen"), args);
    if (!proc.waitForFinished(15000)) {
        m_output->setPlainText(tr("ssh-keygen timed out or is not installed."));
        return;
    }
    QString out = QString::fromUtf8(proc.readAllStandardOutput() + proc.readAllStandardError());
    QFile pub(path + QStringLiteral(".pub"));
    if (pub.open(QIODevice::ReadOnly)) {
        out += tr("\n\nPublic key (%1.pub):\n%2")
                   .arg(path, QString::fromUtf8(pub.readAll()));
    }
    m_output->setPlainText(out);
}

} // namespace macxterm::ui
