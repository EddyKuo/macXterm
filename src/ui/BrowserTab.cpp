#include "ui/BrowserTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QWebEngineView>
#include <QUrl>

namespace macxterm::ui {

BrowserTab::BrowserTab(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* bar = new QHBoxLayout;
    m_view = new QWebEngineView(this);
    auto mkBtn = [&](const QString& text, auto slot) {
        auto* b = new QToolButton(this);
        b->setText(text);
        connect(b, &QToolButton::clicked, m_view, slot);
        bar->addWidget(b);
    };
    mkBtn(QStringLiteral("◀"), &QWebEngineView::back);
    mkBtn(QStringLiteral("▶"), &QWebEngineView::forward);
    mkBtn(QStringLiteral("⟳"), &QWebEngineView::reload);

    m_address = new QLineEdit(this);
    connect(m_address, &QLineEdit::returnPressed, this, &BrowserTab::navigateToBar);
    bar->addWidget(m_address, 1);
    layout->addLayout(bar);
    layout->addWidget(m_view, 1);

    connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl& u) {
        m_address->setText(u.toString());
    });
}

void BrowserTab::load(const QString& url) {
    QString u = url;
    if (!u.contains(QStringLiteral("://"))) u.prepend(QStringLiteral("https://"));
    m_address->setText(u);
    m_view->load(QUrl(u));
}

void BrowserTab::navigateToBar() { load(m_address->text()); }

} // namespace macxterm::ui
