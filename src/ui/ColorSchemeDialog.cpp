#include "ui/ColorSchemeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QDialogButtonBox>

namespace macxterm::ui {

static void tintButton(QPushButton* b, const QColor& c) {
    b->setStyleSheet(QStringLiteral("background:%1; min-width:24px; min-height:20px").arg(c.name()));
}

ColorSchemeDialog::ColorSchemeDialog(const term::ColorScheme& initial, QWidget* parent)
    : QDialog(parent), m_scheme(initial) {
    setWindowTitle(QStringLiteral("Color Scheme Editor"));
    auto* layout = new QVBoxLayout(this);

    auto* base = new QHBoxLayout;
    base->addWidget(new QLabel(QStringLiteral("Foreground"), this));
    m_fgBtn = new QPushButton(this);
    connect(m_fgBtn, &QPushButton::clicked, this, [this] { pickColor(-1); });
    base->addWidget(m_fgBtn);
    base->addWidget(new QLabel(QStringLiteral("Background"), this));
    m_bgBtn = new QPushButton(this);
    connect(m_bgBtn, &QPushButton::clicked, this, [this] { pickColor(-2); });
    base->addWidget(m_bgBtn);
    base->addStretch();
    layout->addLayout(base);

    auto* box = new QGroupBox(QStringLiteral("ANSI palette"), this);
    auto* grid = new QGridLayout(box);
    for (int i = 0; i < 16; ++i) {
        m_ansiBtn[i] = new QPushButton(box);
        connect(m_ansiBtn[i], &QPushButton::clicked, this, [this, i] { pickColor(i); });
        grid->addWidget(m_ansiBtn[i], i / 8, i % 8);
    }
    layout->addWidget(box);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshSwatches();
}

void ColorSchemeDialog::refreshSwatches() {
    tintButton(m_fgBtn, m_scheme.foreground());
    tintButton(m_bgBtn, m_scheme.background());
    for (int i = 0; i < 16; ++i) tintButton(m_ansiBtn[i], m_scheme.ansi(i));
}

void ColorSchemeDialog::pickColor(int which) {
    QColor cur = (which == -1) ? m_scheme.foreground()
               : (which == -2) ? m_scheme.background()
                               : m_scheme.ansi(which);
    const QColor c = QColorDialog::getColor(cur, this, QStringLiteral("Pick color"));
    if (!c.isValid()) return;
    if (which == -1) m_scheme.setForeground(c);
    else if (which == -2) m_scheme.setBackground(c);
    else m_scheme.setAnsi(which, c);
    refreshSwatches();
    emit schemeChosen(m_scheme);   // live preview
}

} // namespace macxterm::ui
