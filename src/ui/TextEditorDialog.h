#pragma once
#include <QDialog>

class QPlainTextEdit;

namespace macxterm::ui {

// Built-in text editor + file compare (MobaXterm's MobaTextEditor / MobaTextDiff).
// A plain editor with open/save, plus a compare mode that renders an LCS diff of
// two files (backed by tools::TextDiff) with +/- markers and color.
class TextEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextEditorDialog(QWidget* parent = nullptr);

    // Open the editor on a file (empty = blank document).
    void openFile(const QString& path);
    // Show a read-only diff of two files.
    void showDiff(const QString& fileA, const QString& fileB);

signals:
    // Emitted after the buffer is written to disk (used by the SFTP panel to
    // re-upload a remotely-edited file).
    void fileSaved(const QString& path);

private slots:
    void openDialog();
    void save();
    void saveAs();

private:
    QPlainTextEdit* m_edit = nullptr;
    QString m_path;
    bool m_diffMode = false;
};

} // namespace macxterm::ui
