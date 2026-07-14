#pragma once
#include "term/VtEngine.h"
#include "connect/IConnection.h"
#include <QWidget>
#include <memory>

class QKeyEvent;

namespace macxterm::ui {

// A terminal pane: owns a VtEngine, renders its ScreenBuffer, forwards keyboard
// input to an IConnection. One TerminalWidget per tab/split (Architecture §6.2).
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);

    // Attach a connection (takes ownership). Wires data both directions.
    void attach(connect::IConnection* conn);

    // MultiExec: when disabled, this pane is excluded from broadcast input.
    bool multiExecEnabled() const { return m_multiExec; }
    void setMultiExecEnabled(bool on) { m_multiExec = on; }

    // Inject input as if typed (used by MultiExec broadcast).
    void feedInput(const QByteArray& bytes);

    QSize sizeHint() const override { return {640, 400}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void recomputeGrid();

    term::VtEngine m_vt;
    connect::IConnection* m_conn = nullptr;
    int m_cellW = 8;
    int m_cellH = 16;
    bool m_multiExec = true;
};

} // namespace macxterm::ui
