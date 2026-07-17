#include "ui/SettingsDialog.h"
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QLabel>
#include <QFont>
#include <QFontMetrics>
#include <QRawFont>
#include <QFrame>
#include <QStringList>

namespace macxterm::ui {

SettingsDialog::SettingsDialog(const core::Settings& s, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Settings"));
    auto* tabs = new QTabWidget(this);

    // Terminal tab.
    auto* term = new QWidget;
    auto* tf = new QFormLayout(term);
    // A monospaced-only picker: each entry is drawn in its own face, so the list
    // itself previews the fonts. A terminal needs fixed-width, so filter to those.
    m_font = new QFontComboBox(term);
    m_font->setFontFilters(QFontComboBox::MonospacedFonts);
    m_font->setCurrentFont(QFont(s.fontFamily()));
    m_fontSize = new QSpinBox(term); m_fontSize->setRange(6, 72); m_fontSize->setValue(s.fontSize());
    m_scheme = new QComboBox(term); m_scheme->addItems({"Dark", "Light", "Solarized Dark"});
    m_scheme->setCurrentText(s.colorScheme());
    m_scrollback = new QSpinBox(term); m_scrollback->setRange(0, 1000000); m_scrollback->setValue(s.scrollbackLines());
    tf->addRow(QStringLiteral("Font"), m_font);
    tf->addRow(QStringLiteral("Font size"), m_fontSize);

    // Live preview + Unicode-coverage readout, so the family/size can be judged
    // before applying. See updateFontPreview().
    m_preview = new QLabel(term);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setMinimumHeight(96);
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_coverage = new QLabel(term);
    m_coverage->setWordWrap(true);
    tf->addRow(QStringLiteral("Preview"), m_preview);
    tf->addRow(QStringLiteral("Unicode"), m_coverage);
    connect(m_font, &QFontComboBox::currentFontChanged, this, [this] { updateFontPreview(); });
    connect(m_fontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { updateFontPreview(); });
    updateFontPreview();

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
    s.setValue("terminal.font", m_font->currentFont().family());
    s.setValue("terminal.fontSize", m_fontSize->value());
    s.setValue("terminal.scheme", m_scheme->currentText());
    s.setValue("terminal.scrollback", m_scrollback->value());
    s.setValue("x11.autoStart", m_x11Auto->isChecked());
    s.setValue("ssh.x11Forwarding", m_x11Fwd->isChecked());
    return s;
}

void SettingsDialog::updateFontPreview() {
    QFont f = m_font->currentFont();
    f.setPointSize(m_fontSize->value());
    m_preview->setFont(f);
    // A sample spanning the ranges a terminal actually renders: ASCII, accents,
    // CJK, box-drawing / block elements, arrows, Greek/Cyrillic, powerline and
    // emoji. Qt substitutes a fallback font for glyphs the family itself lacks,
    // so this roughly mirrors what the terminal will show.
    m_preview->setText(QStringLiteral(
        "The quick brown fox — 0123456789 {}[]()<>|/\\\n"
        "中文字型 日本語 한국어 · αβγδ ЯдЖ · éüñ ©®€£¥\n"
        "┌──┬──┐ │ ▓▒░ █ ▁▂▃▄ →←↑↓ ✓✗  0xE0B0"));

    // Report which representative Unicode ranges the *selected* family covers on
    // its own. QRawFont::supportsCharacter tests the font's actual cmap — unlike
    // QFontMetrics::inFont, which on macOS reports true after CoreText's silent
    // fallback and would claim e.g. Menlo "has" CJK when it does not.
    const QRawFont rf = QRawFont::fromFont(f);
    struct Probe { const char* label; uint cp; };
    static const Probe probes[] = {
        {"Latin", 'A'}, {"Accents", 0x00E9 /*é*/}, {"Greek", 0x03B1 /*α*/},
        {"Cyrillic", 0x042F /*Я*/}, {"CJK", 0x4E2D /*中*/}, {"Box-drawing", 0x2500 /*─*/},
        {"Blocks", 0x2588 /*█*/}, {"Powerline", 0xE0B0}, {"Emoji", 0x1F600 /*😀*/},
    };
    QStringList have, missing;
    for (const Probe& p : probes) {
        if (rf.isValid() && rf.supportsCharacter(p.cp)) have << QString::fromLatin1(p.label);
        else missing << QString::fromLatin1(p.label);
    }
    QString msg = QStringLiteral("This font covers: ") +
                  (have.isEmpty() ? QStringLiteral("(none)") : have.join(QStringLiteral(", ")));
    if (!missing.isEmpty())
        msg += QStringLiteral(".  Not in this font (falls back in the terminal): ") +
               missing.join(QStringLiteral(", "));
    m_coverage->setText(msg);
}

} // namespace macxterm::ui
