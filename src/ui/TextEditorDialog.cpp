#include "ui/TextEditorDialog.h"
#include "tools/TextDiff.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QFontDatabase>
#include <QTextStream>

namespace macxterm::ui {

TextEditorDialog::TextEditorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Text Editor"));
    resize(720, 560);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* tb = new QToolBar(this);
    tb->addAction(QStringLiteral("Open"), this, &TextEditorDialog::openDialog);
    tb->addAction(QStringLiteral("Save"), this, &TextEditorDialog::save);
    tb->addAction(QStringLiteral("Save As"), this, &TextEditorDialog::saveAs);
    layout->addWidget(tb);

    m_edit = new QPlainTextEdit(this);
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_edit, 1);
}

void TextEditorDialog::openFile(const QString& path) {
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        m_edit->setPlainText(QString::fromUtf8(f.readAll()));
        m_path = path;
        setWindowTitle(QStringLiteral("Text Editor — ") + path);
    }
}

void TextEditorDialog::openDialog() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open file"));
    openFile(path);
}

void TextEditorDialog::save() {
    if (m_diffMode) return;
    if (m_path.isEmpty()) { saveAs(); return; }
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_edit->toPlainText().toUtf8());
        f.close();
        emit fileSaved(m_path);
    }
}

void TextEditorDialog::saveAs() {
    if (m_diffMode) return;
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save as"), m_path);
    if (path.isEmpty()) return;
    m_path = path;
    save();
    setWindowTitle(QStringLiteral("Text Editor — ") + path);
}

void TextEditorDialog::showDiff(const QString& fileA, const QString& fileB) {
    m_diffMode = true;
    setWindowTitle(QStringLiteral("Compare: %1 ⇄ %2").arg(fileA, fileB));
    m_edit->setReadOnly(true);

    auto read = [](const QString& p) {
        QFile f(p); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    };
    const auto diff = tools::TextDiff::diff(read(fileA), read(fileB));

    // Render with +/- markers; use HTML for color.
    QString html = QStringLiteral("<pre style='font-family:monospace'>");
    for (const tools::DiffLine& l : diff) {
        QString esc = l.text.toHtmlEscaped();
        if (l.kind == tools::DiffLine::Added)
            html += QStringLiteral("<span style='background:#1e4620;color:#b7f7c2'>+ %1</span>\n").arg(esc);
        else if (l.kind == tools::DiffLine::Removed)
            html += QStringLiteral("<span style='background:#4a1f1f;color:#f7b7b7'>- %1</span>\n").arg(esc);
        else
            html += QStringLiteral("<span>  %1</span>\n").arg(esc);
    }
    html += QStringLiteral("</pre>");
    m_edit->appendHtml(html);
}

} // namespace macxterm::ui
