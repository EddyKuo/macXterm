#pragma once
#include <QDialog>

class QLineEdit;
class QTreeWidget;

namespace macxterm::ui {

// Recursive folder comparison UI (MobaXterm's MobaFoldersDiff). Pick two
// directories; the tree lists each file colored by status (only-left, only-right,
// differ, same). Backed by tools::FolderDiff.
class FolderDiffDialog : public QDialog {
    Q_OBJECT
public:
    explicit FolderDiffDialog(QWidget* parent = nullptr);

private slots:
    void chooseLeft();
    void chooseRight();
    void runCompare();

private:
    QLineEdit* m_left = nullptr;
    QLineEdit* m_right = nullptr;
    QTreeWidget* m_tree = nullptr;
};

} // namespace macxterm::ui
