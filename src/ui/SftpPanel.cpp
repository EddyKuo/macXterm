#include "ui/SftpPanel.h"
#include "sftp/RemotePath.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>

namespace macxterm::ui {

SftpPanel::SftpPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    // Toolbar: Up / Refresh / Download / Upload.
    auto* bar = new QHBoxLayout;
    auto mkBtn = [&](const QString& text, auto slot) {
        auto* b = new QToolButton(this);
        b->setText(text);
        connect(b, &QToolButton::clicked, this, slot);
        bar->addWidget(b);
        return b;
    };
    mkBtn(QStringLiteral("↑"), &SftpPanel::goUp);
    mkBtn(QStringLiteral("⟳"), &SftpPanel::refresh);
    mkBtn(QStringLiteral("↓ Download"), &SftpPanel::download);
    mkBtn(QStringLiteral("↑ Upload"), &SftpPanel::upload);
    bar->addStretch();
    layout->addLayout(bar);

    m_pathBar = new QLineEdit(this);
    connect(m_pathBar, &QLineEdit::returnPressed, this, [this] { navigateTo(m_pathBar->text()); });
    layout->addWidget(m_pathBar);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size"), QStringLiteral("Perms")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(m_list, &QTreeWidget::itemActivated, this, &SftpPanel::onItemActivated);
    layout->addWidget(m_list, 1);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    connect(&m_sftp, &sftp::SftpConnection::error, this, [this](const QString& m) { setStatus(m); });
}

void SftpPanel::setStatus(const QString& msg) { if (m_status) m_status->setText(msg); }

bool SftpPanel::openFor(const core::Session& session) {
    setStatus(QStringLiteral("Connecting…"));
    QApplication::processEvents();
    if (!m_sftp.connectSession(session)) {
        setStatus(QStringLiteral("SFTP connect failed"));
        return false;
    }
    m_cwd = QStringLiteral(".");
    navigateTo(m_cwd);
    return true;
}

void SftpPanel::navigateTo(const QString& path) {
    if (!m_sftp.isReady()) return;
    QList<sftp::SftpEntry> entries;
    if (!m_sftp.list(path, entries)) return;   // error() already emitted
    m_cwd = path;
    m_pathBar->setText(path);
    m_list->clear();
    for (const sftp::SftpEntry& e : entries) {
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, (e.isDir ? QStringLiteral("📁 ") : QString()) + e.name);
        item->setText(1, e.isDir ? QString() : e.sizeString());
        item->setText(2, e.permString());
        item->setData(0, Qt::UserRole, e.name);
        item->setData(0, Qt::UserRole + 1, e.isDir);
    }
    setStatus(QStringLiteral("%1 items").arg(entries.size()));
}

void SftpPanel::refresh() { navigateTo(m_cwd); }

void SftpPanel::goUp() { navigateTo(sftp::RemotePath::parent(m_cwd)); }

void SftpPanel::onItemActivated() {
    QTreeWidgetItem* item = m_list->currentItem();
    if (!item) return;
    const QString name = item->data(0, Qt::UserRole).toString();
    const bool isDir = item->data(0, Qt::UserRole + 1).toBool();
    if (name == QStringLiteral(".")) return;
    if (isDir) navigateTo(sftp::RemotePath::join(m_cwd, name));
}

void SftpPanel::download() {
    QTreeWidgetItem* item = m_list->currentItem();
    if (!item || !m_sftp.isReady()) return;
    const QString name = item->data(0, Qt::UserRole).toString();
    if (item->data(0, Qt::UserRole + 1).toBool()) return;  // dirs not supported yet
    const QString remote = sftp::RemotePath::join(m_cwd, name);
    const QString local = QFileDialog::getSaveFileName(this, QStringLiteral("Download to"), name);
    if (local.isEmpty()) return;
    const qint64 n = m_sftp.download(remote, local);
    setStatus(n >= 0 ? QStringLiteral("Downloaded %1 bytes").arg(n)
                     : QStringLiteral("Download failed"));
}

void SftpPanel::upload() {
    if (!m_sftp.isReady()) return;
    const QString local = QFileDialog::getOpenFileName(this, QStringLiteral("Upload file"));
    if (local.isEmpty()) return;
    const QString remote = sftp::RemotePath::join(m_cwd, QFileInfo(local).fileName());
    const qint64 n = m_sftp.upload(local, remote);
    setStatus(n >= 0 ? QStringLiteral("Uploaded %1 bytes").arg(n)
                     : QStringLiteral("Upload failed"));
    if (n >= 0) refresh();
}

} // namespace macxterm::ui
