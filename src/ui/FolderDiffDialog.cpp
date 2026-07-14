#include "ui/FolderDiffDialog.h"
#include "tools/FolderDiff.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QLabel>

namespace macxterm::ui {

FolderDiffDialog::FolderDiffDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Compare Folders"));
    resize(640, 520);
    auto* layout = new QVBoxLayout(this);

    auto mkRow = [&](const QString& label, QLineEdit*& edit, auto slot) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(label, this));
        edit = new QLineEdit(this);
        row->addWidget(edit, 1);
        auto* btn = new QPushButton(QStringLiteral("Browse…"), this);
        connect(btn, &QPushButton::clicked, this, slot);
        row->addWidget(btn);
        layout->addLayout(row);
    };
    mkRow(QStringLiteral("Left:"), m_left, &FolderDiffDialog::chooseLeft);
    mkRow(QStringLiteral("Right:"), m_right, &FolderDiffDialog::chooseRight);

    auto* compare = new QPushButton(QStringLiteral("Compare"), this);
    connect(compare, &QPushButton::clicked, this, &FolderDiffDialog::runCompare);
    layout->addWidget(compare);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("File"), QStringLiteral("Status")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);
}

void FolderDiffDialog::chooseLeft() {
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Left folder"));
    if (!d.isEmpty()) m_left->setText(d);
}
void FolderDiffDialog::chooseRight() {
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Right folder"));
    if (!d.isEmpty()) m_right->setText(d);
}

void FolderDiffDialog::runCompare() {
    m_tree->clear();
    if (m_left->text().isEmpty() || m_right->text().isEmpty()) return;
    const auto entries = tools::FolderDiff::compare(m_left->text(), m_right->text());
    for (const tools::FolderDiffEntry& e : entries) {
        QString status; QColor color;
        switch (e.kind) {
            case tools::FolderDiffEntry::OnlyLeft:  status = QStringLiteral("only left");  color = QColor(0xff, 0xaa, 0x55); break;
            case tools::FolderDiffEntry::OnlyRight: status = QStringLiteral("only right"); color = QColor(0x55, 0xaa, 0xff); break;
            case tools::FolderDiffEntry::Differ:    status = QStringLiteral("differ");     color = QColor(0xff, 0x66, 0x66); break;
            case tools::FolderDiffEntry::Same:      status = QStringLiteral("same");       color = QColor(0x88, 0x88, 0x88); break;
        }
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, e.relPath);
        item->setText(1, status);
        item->setForeground(1, color);
    }
    setWindowTitle(QStringLiteral("Compare Folders — %1 entries").arg(entries.size()));
}

} // namespace macxterm::ui
