#include "ui/SftpPanel.h"
#include "ui/TextEditorDialog.h"
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
#include <QDir>
#include <QApplication>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QDrag>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QUuid>

namespace macxterm::ui {

SftpPanel::SftpPanel(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);   // accept OS file drops for upload
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    // Toolbar: Up / Refresh / Download / Upload / Follow.
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
    m_follow = new QToolButton(this);
    m_follow->setText(QStringLiteral("⇄ Follow"));
    m_follow->setCheckable(true);
    m_follow->setToolTip(QStringLiteral("Follow the terminal's current folder"));
    bar->addWidget(m_follow);
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
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->viewport()->installEventFilter(this);   // for drag-out
    connect(m_list, &QTreeWidget::itemActivated, this, &SftpPanel::onItemActivated);
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, &SftpPanel::showContextMenu);
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
    // Resolve "." to an absolute path so follow-terminal-folder can compare.
    const QString abs = m_sftp.realpath(QStringLiteral("."));
    m_cwd = abs.isEmpty() ? QStringLiteral(".") : abs;
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
        item->setData(0, Qt::UserRole + 2, static_cast<uint>(e.permissions));
    }
    setStatus(QStringLiteral("%1 items").arg(entries.size()));
}

void SftpPanel::refresh() { navigateTo(m_cwd); }

void SftpPanel::goUp() { navigateTo(sftp::RemotePath::parent(m_cwd)); }

void SftpPanel::setRemoteCwd(const QString& absPath) {
    if (!m_follow || !m_follow->isChecked()) return;
    if (absPath.isEmpty() || absPath == m_cwd) return;
    if (!m_sftp.isReady()) return;
    navigateTo(absPath);
}

QString SftpPanel::selectedName(bool* isDir) const {
    QTreeWidgetItem* item = m_list->currentItem();
    if (!item) return {};
    if (isDir) *isDir = item->data(0, Qt::UserRole + 1).toBool();
    return item->data(0, Qt::UserRole).toString();
}

void SftpPanel::onItemActivated() {
    bool isDir = false;
    const QString name = selectedName(&isDir);
    if (name.isEmpty() || name == QStringLiteral(".")) return;
    const QString target = sftp::RemotePath::join(m_cwd, name);
    if (isDir) navigateTo(target);
    else editRemote(target);   // double-click a file → edit
}

qint64 SftpPanel::downloadTo(const QString& remotePath, const QString& localPath) {
    return m_sftp.download(remotePath, localPath);
}

void SftpPanel::editRemote(const QString& remotePath) {
    if (!m_sftp.isReady()) return;
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString local = QDir(tmpDir).filePath(
        QStringLiteral("macxterm-%1-%2")
            .arg(QUuid::createUuid().toString(QUuid::Id128), QFileInfo(remotePath).fileName()));
    if (downloadTo(remotePath, local) < 0) { setStatus(QStringLiteral("Open for edit failed")); return; }

    m_editing.insert(local, remotePath);
    auto* ed = new TextEditorDialog(this);
    ed->setAttribute(Qt::WA_DeleteOnClose);
    ed->openFile(local);
    // On every save, push the temp file back to the server.
    connect(ed, &TextEditorDialog::fileSaved, this, [this](const QString& savedLocal) {
        const QString remote = m_editing.value(savedLocal);
        if (remote.isEmpty()) return;
        const qint64 n = m_sftp.upload(savedLocal, remote);
        setStatus(n >= 0 ? QStringLiteral("Saved back %1 bytes → %2").arg(n).arg(remote)
                         : QStringLiteral("Save-back failed"));
    });
    connect(ed, &QDialog::finished, this, [this, local] {
        m_editing.remove(local);
        QFile::remove(local);
    });
    ed->show();
    setStatus(QStringLiteral("Editing %1 (saves upload back)").arg(remotePath));
}

void SftpPanel::download() {
    bool isDir = false;
    const QString name = selectedName(&isDir);
    if (name.isEmpty() || isDir || !m_sftp.isReady()) return;   // dirs not supported yet
    const QString remote = sftp::RemotePath::join(m_cwd, name);
    const QString local = QFileDialog::getSaveFileName(this, QStringLiteral("Download to"), name);
    if (local.isEmpty()) return;
    const qint64 n = downloadTo(remote, local);
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

void SftpPanel::showContextMenu(const QPoint& pos) {
    if (!m_sftp.isReady()) return;
    bool isDir = false;
    const QString name = selectedName(&isDir);
    QMenu menu(this);
    if (!name.isEmpty() && name != QStringLiteral(".")) {
        const QString target = sftp::RemotePath::join(m_cwd, name);
        if (!isDir) {
            menu.addAction(QStringLiteral("Edit…"), this, [this, target] { editRemote(target); });
            menu.addAction(QStringLiteral("Download…"), this, &SftpPanel::download);
        }
        menu.addAction(QStringLiteral("Permissions (chmod)…"), this, [this, target] {
            QTreeWidgetItem* it = m_list->currentItem();
            const uint cur = it ? it->data(0, Qt::UserRole + 2).toUInt() : 0644;
            bool ok = false;
            const QString mode = QInputDialog::getText(
                this, QStringLiteral("chmod"), QStringLiteral("Octal permissions:"),
                QLineEdit::Normal, QStringLiteral("%1").arg(cur, 3, 8, QChar('0')), &ok);
            if (!ok) return;
            bool valid = false;
            const uint m = mode.toUInt(&valid, 8);
            if (valid && m_sftp.chmod(target, m)) refresh();
        });
        menu.addAction(QStringLiteral("Rename…"), this, [this, target, name] {
            bool ok = false;
            const QString nn = QInputDialog::getText(this, QStringLiteral("Rename"),
                QStringLiteral("New name:"), QLineEdit::Normal, name, &ok);
            if (ok && !nn.isEmpty() && m_sftp.rename(target, sftp::RemotePath::join(m_cwd, nn)))
                refresh();
        });
        menu.addAction(QStringLiteral("Delete"), this, [this, target, isDir, name] {
            if (QMessageBox::question(this, QStringLiteral("Delete"),
                    QStringLiteral("Delete %1?").arg(name)) != QMessageBox::Yes) return;
            const bool ok = isDir ? m_sftp.removeDir(target) : m_sftp.removeFile(target);
            if (ok) refresh();
        });
        menu.addSeparator();
    }
    menu.addAction(QStringLiteral("New folder…"), this, [this] {
        bool ok = false;
        const QString nn = QInputDialog::getText(this, QStringLiteral("New folder"),
            QStringLiteral("Folder name:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !nn.isEmpty() && m_sftp.makeDir(sftp::RemotePath::join(m_cwd, nn))) refresh();
    });
    menu.addAction(QStringLiteral("Upload file…"), this, &SftpPanel::upload);
    menu.addAction(QStringLiteral("Refresh"), this, &SftpPanel::refresh);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

// ── Drag-and-drop ───────────────────────────────────────────────────────────

void SftpPanel::dragEnterEvent(QDragEnterEvent* e) {
    if (m_sftp.isReady() && e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void SftpPanel::dropEvent(QDropEvent* e) {
    if (!m_sftp.isReady() || !e->mimeData()->hasUrls()) return;
    int ok = 0;
    for (const QUrl& url : e->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString local = url.toLocalFile();
        const QString remote = sftp::RemotePath::join(m_cwd, QFileInfo(local).fileName());
        if (m_sftp.upload(local, remote) >= 0) ++ok;
    }
    setStatus(QStringLiteral("Uploaded %1 file(s)").arg(ok));
    if (ok) { refresh(); e->acceptProposedAction(); }
}

// Start an OS drag when the user drags a remote file out of the list; the file
// is downloaded to a temp path and offered as a file URL to the drop target.
bool SftpPanel::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_list->viewport()) {
        if (ev->type() == QEvent::MouseButtonPress) {
            m_dragStart = static_cast<QMouseEvent*>(ev)->pos();
        } else if (ev->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if ((me->buttons() & Qt::LeftButton) &&
                (me->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
                bool isDir = false;
                const QString name = selectedName(&isDir);
                if (!name.isEmpty() && !isDir && m_sftp.isReady()) {
                    const QString remote = sftp::RemotePath::join(m_cwd, name);
                    const QString tmp = QDir(QStandardPaths::writableLocation(
                        QStandardPaths::TempLocation)).filePath(name);
                    if (downloadTo(remote, tmp) >= 0) {
                        auto* drag = new QDrag(this);
                        auto* mime = new QMimeData;
                        mime->setUrls({QUrl::fromLocalFile(tmp)});
                        drag->setMimeData(mime);
                        drag->exec(Qt::CopyAction);
                    }
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

} // namespace macxterm::ui
