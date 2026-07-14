#pragma once
#include "term/VtEngine.h"
#include "term/ColorScheme.h"
#include "connect/IConnection.h"
#include <QWidget>
#include <QFont>
#include <QPoint>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace macxterm::ui {

// A terminal pane: owns a VtEngine, renders its ScreenBuffer + scrollback,
// forwards keyboard input to an IConnection, and supports mouse selection /
// copy / paste and a configurable color scheme and font.
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

    // Appearance.
    void setColorScheme(const term::ColorScheme& scheme);
    void setTerminalFont(const QFont& font);

    // Clipboard.
    void copySelection();
    void paste();

    QSize sizeHint() const override { return {640, 400}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void recomputeGrid();
    void updateCellMetrics();
    int  totalLines() const;              // scrollback + visible rows
    int  topLine() const;                 // absolute index of the first visible row
    const term::Cell* cellAt(int absLine, int col) const;  // scrollback or screen
    QString selectedText() const;
    QPoint cellForPos(const QPoint& p) const;  // widget pixel -> (col, absLine)

    term::VtEngine m_vt;
    term::ColorScheme m_scheme;
    connect::IConnection* m_conn = nullptr;
    int m_cellW = 8;
    int m_cellH = 16;
    bool m_multiExec = true;

    int m_scrollOffset = 0;               // 0 = live bottom; >0 = scrolled up
    bool m_selecting = false;
    bool m_hasSelection = false;
    QPoint m_selAnchor;                   // (col, absLine)
    QPoint m_selHead;                     // (col, absLine)
};

} // namespace macxterm::ui
