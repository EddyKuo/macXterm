#pragma once
#include <QWidget>

class QLineEdit;
class QWebEngineView;

namespace macxterm::ui {

// An embedded web browser tab (MobaXterm's Browser session type). Wraps a
// QWebEngineView with a minimal address bar + back/forward/reload, so browser
// sessions open inside macXterm instead of the system browser.
class BrowserTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowserTab(QWidget* parent = nullptr);
    void load(const QString& url);

private:
    void navigateToBar();

    QLineEdit* m_address = nullptr;
    QWebEngineView* m_view = nullptr;
};

} // namespace macxterm::ui
