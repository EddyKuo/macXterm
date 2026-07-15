#pragma once
#include "term/VtEngine.h"
#include "term/ColorScheme.h"
#include "term/SyntaxHighlighter.h"
#include "connect/IConnection.h"
#include <QWidget>
#include <QFont>
#include <QPoint>
#include <functional>

class QFile;
class QLineEdit;
class QLabel;

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;

namespace macxterm::ui {

// A terminal pane: owns a VtEngine, renders its ScreenBuffer + scrollback,
// forwards keyboard input to an IConnection, and supports mouse selection /
// copy / paste and a configurable color scheme and font.
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() override;

    // Attach a connection (takes ownership). Wires data both directions.
    void attach(connect::IConnection* conn);

    // MultiExec: when disabled, this pane is excluded from broadcast input.
    bool multiExecEnabled() const { return m_multiExec; }
    void setMultiExecEnabled(bool on) { m_multiExec = on; }

    // Inject input straight to this pane's connection (used by MultiExec
    // broadcast to deliver the same keystrokes to every pane).
    void feedInput(const QByteArray& bytes);

    // When set, user keystrokes/paste are routed here instead of straight to
    // this pane's own connection (MultiExec). Clear to restore normal routing.
    void setInputHandler(std::function<void(const QByteArray&)> handler) {
        m_inputHandler = std::move(handler);
    }

    // Appearance.
    void setColorScheme(const term::ColorScheme& scheme);
    void setTerminalFont(const QFont& font);

    // Terminal keyword/regex syntax highlighting (MobaXterm feature).
    void setSyntaxHighlighting(bool on);
    bool syntaxHighlighting() const { return m_highlighter.enabled(); }

    // Session logging: mirror all received output to a file. Returns false if
    // the file can't be opened. stopLogging() closes it.
    bool startLogging(const QString& path);
    void stopLogging();
    bool isLogging() const { return m_logFile != nullptr; }

    // Clipboard / selection.
    void copySelection();
    void paste();
    void selectAll();        // select the whole screen + scrollback
    void clearScrollback();  // drop history and return to the live bottom
    void showFindBar();      // open the scrollback search bar (Cmd/Ctrl+Shift+F)

    // Paste delay: when > 0, a multi-line paste is delivered line-by-line with
    // this many milliseconds between lines (avoids overrunning slow remotes).
    void setPasteDelay(int ms) { m_pasteDelayMs = ms; }
    void setScrollbackLines(int n) { m_vt.setScrollbackMax(n); }
    int pasteDelay() const { return m_pasteDelayMs; }
    // Bytes sent when the Backspace key is pressed (per-session override; DEL
    // 0x7f by default, ^H 0x08 for hosts that expect the legacy code).
    void setBackspaceCode(const QByteArray& code) { if (!code.isEmpty()) m_backspaceCode = code; }

    QSize sizeHint() const override { return {640, 400}; }

signals:
    void cwdChanged(const QString& absPath);   // OSC 7 from the shell
    void titleChanged(const QString& title);    // OSC 0/2

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void recomputeGrid();
    void updateCellMetrics();
    // Scrollback search.
    QString lineText(int absLine) const;
    QString urlAt(int absLine, int col) const;   // URL spanning a cell, or empty
    void findUpdate(const QString& query);     // recompute matches for a query
    void findStep(bool forward);               // move to next/prev match
    void findReveal(int index);                // select + scroll match into view
    void positionFindBar();                    // place the overlay bar
    // Mouse reporting: encode one event for the far-end app per the active mode.
    // cb is the button/motion code (before the +32 offset); col1/row1 are 1-based.
    bool reportMouseIfEnabled(QMouseEvent* e, int cbBase, bool release, bool motion);
    void sendMouseReport(int cb, int col1, int row1, bool release);
    int  totalLines() const;              // scrollback + visible rows
    int  topLine() const;                 // absolute index of the first visible row
    const term::Cell* cellAt(int absLine, int col) const;  // scrollback or screen
    QString selectedText() const;
    QPoint cellForPos(const QPoint& p) const;  // widget pixel -> (col, absLine)

    term::VtEngine m_vt;
    term::ColorScheme m_scheme;
    term::SyntaxHighlighter m_highlighter;
    QFile* m_logFile = nullptr;
    int m_pasteDelayMs = 0;
    QByteArray m_backspaceCode = "\x7f";
    connect::IConnection* m_conn = nullptr;
    int m_cellW = 8;
    int m_cellH = 16;
    bool m_multiExec = true;

    void sendInput(const QByteArray& bytes);   // routes via handler or own conn

    std::function<void(const QByteArray&)> m_inputHandler;
    int m_scrollOffset = 0;               // 0 = live bottom; >0 = scrolled up
    bool m_selecting = false;
    bool m_hasSelection = false;
    int m_mouseBtn = -1;                  // button currently held for motion reports
    QPoint m_lastMouseCell{-1, -1};       // last reported (col,row) to dedupe motion

    // Scrollback find bar (lazily created).
    QWidget*   m_findBar = nullptr;
    QLineEdit* m_findEdit = nullptr;
    QLabel*    m_findCount = nullptr;
    QList<QPoint> m_findMatches;          // (col, absLine) of each match start
    int m_findLen = 0;                    // length of the current query
    int m_findIndex = -1;                 // current match, or -1
    QPoint m_selAnchor;                   // (col, absLine)
    QPoint m_selHead;                     // (col, absLine)
};

} // namespace macxterm::ui
