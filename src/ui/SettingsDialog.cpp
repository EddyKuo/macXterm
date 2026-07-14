#include "ui/SettingsDialog.h"
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>

namespace macxterm::ui {

SettingsDialog::SettingsDialog(const core::Settings& s, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Settings"));
    auto* tabs = new QTabWidget(this);

    // Terminal tab.
    auto* term = new QWidget;
    auto* tf = new QFormLayout(term);
    m_font = new QLineEdit(s.fontFamily(), term);
    m_fontSize = new QSpinBox(term); m_fontSize->setRange(6, 72); m_fontSize->setValue(s.fontSize());
    m_scheme = new QComboBox(term); m_scheme->addItems({"Dark", "Light", "Solarized Dark"});
    m_scheme->setCurrentText(s.colorScheme());
    m_scrollback = new QSpinBox(term); m_scrollback->setRange(0, 1000000); m_scrollback->setValue(s.scrollbackLines());
    tf->addRow(QStringLiteral("Font"), m_font);
    tf->addRow(QStringLiteral("Font size"), m_fontSize);
    tf->addRow(QStringLiteral("Color scheme"), m_scheme);
    tf->addRow(QStringLiteral("Scrollback"), m_scrollback);
    tabs->addTab(term, QStringLiteral("Terminal"));

    // X11 tab.
    auto* x11 = new QWidget;
    auto* xf = new QFormLayout(x11);
    m_x11Auto = new QCheckBox(QStringLiteral("Auto-start X server"), x11);
    m_x11Auto->setChecked(s.x11AutoStart());
    m_x11Fwd = new QCheckBox(QStringLiteral("Enable SSH X11 forwarding"), x11);
    m_x11Fwd->setChecked(s.value("ssh.x11Forwarding", true).toBool());
    xf->addRow(m_x11Auto);
    xf->addRow(m_x11Fwd);
    tabs->addTab(x11, QStringLiteral("X11"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

core::Settings SettingsDialog::settings() const {
    core::Settings s;
    s.setValue("terminal.font", m_font->text());
    s.setValue("terminal.fontSize", m_fontSize->value());
    s.setValue("terminal.scheme", m_scheme->currentText());
    s.setValue("terminal.scrollback", m_scrollback->value());
    s.setValue("x11.autoStart", m_x11Auto->isChecked());
    s.setValue("ssh.x11Forwarding", m_x11Fwd->isChecked());
    return s;
}

} // namespace macxterm::ui
