#include "ui/SftpPanel.h"
#include "ui/TextEditorDialog.h"
#include "sftp/RemotePath.h"
#include "sftp/SftpConnection.h"
#include "sftp/FtpClient.h"
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
#include <QDateTime>
#include <QProgressDialog>
#include <QDirIterator>
#include <QUuid>

namespace macxterm::ui {

namespace {
// Tree item that sorts the Size column numerically (not lexically) and the
// Modified column chronologically, while other columns fall back to text.
class SftpItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override {
        const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (col == 1)   // Size
            return data(1, Qt::UserRole).toULongLong() < other.data(1, Qt::UserRole).toULongLong();
        if (col == 2)   // Modified
            return data(2, Qt::UserRole).toLongLong() < other.data(2, Qt::UserRole).toLongLong();
        return text(col).compare(other.text(col), Qt::CaseInsensitive) < 0;
    }
};
} // namespace

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
    mkBtn(QStringLiteral("⌂"), &SftpPanel::goHome)->setToolTip(QStringLiteral("Home folder"));
    mkBtn(QStringLiteral("↑"), &SftpPanel::goUp);
    mkBtn(QStringLiteral("⟳"), &SftpPanel::refresh);
    mkBtn(QStringLiteral("↓ Download"), &SftpPanel::download);
    mkBtn(QStringLiteral("↑ Upload"), &SftpPanel::upload);
    m_follow = new QToolButton(this);
    m_follow->setText(QStringLiteral("⇄ Follow"));
    m_follow->setCheckable(true);
    m_follow->setChecked(true);   // follow the terminal's folder by default (MobaXterm-like)
    m_follow->setToolTip(QStringLiteral("Follow the terminal's current folder"));
    bar->addWidget(m_follow);
    bar->addStretch();
    layout->addLayout(bar);

    m_pathBar = new QLineEdit(this);
    connect(m_pathBar, &QLineEdit::returnPressed, this, [this] { navigateTo(m_pathBar->text()); });
    layout->addWidget(m_pathBar);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(4);
    m_list->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size"),
                             QStringLiteral("Modified"), QStringLiteral("Perms")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->setSortingEnabled(true);
    m_list->sortByColumn(0, Qt::AscendingOrder);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->viewport()->installEventFilter(this);   // for drag-out
    connect(m_list, &QTreeWidget::itemActivated, this, &SftpPanel::onItemActivated);
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, &SftpPanel::showContextMenu);
    layout->addWidget(m_list, 1);

    m_status = new QLabel(this);
    layout->addWidget(m_status);

    initBackend(Backend::Sftp);
}

SftpPanel::SftpPanel(Backend backend, QWidget* parent) : SftpPanel(parent) {
    initBackend(backend);   // swap the default SFTP backend for the requested one
}

void SftpPanel::initBackend(Backend backend) {
    if (m_fsObj) { m_fsObj->deleteLater(); m_fsObj = nullptr; m_fs = nullptr; }
    if (backend == Backend::Ftp) {
        auto* c = new sftp::FtpClient(this);
        connect(c, &sftp::FtpClient::error, this, [this](const QString& m) { setStatus(m); });
        m_fs = c; m_fsObj = c;
    } else {
        auto* c = new sftp::SftpConnection(this);
        connect(c, &sftp::SftpConnection::error, this, [this](const QString& m) { setStatus(m); });
        m_fs = c; m_fsObj = c;
    }
}

void SftpPanel::setStatus(const QString& msg) { if (m_status) m_status->setText(msg); }

bool SftpPanel::openFor(const core::Session& session) {
    setStatus(QStringLiteral("Connecting…"));
    QApplication::processEvents();
    if (!m_fs->connectSession(session)) {
        setStatus(QStringLiteral("Connect failed"));
        return false;
    }
    // Resolve "." to an absolute path so follow-terminal-folder can compare.
    const QString abs = m_fs->realpath(QStringLiteral("."));
    m_cwd = abs.isEmpty() ? QStringLiteral(".") : abs;
    m_home = m_cwd;   // remember the login directory for the Home button
    navigateTo(m_cwd);
    return true;
}

void SftpPanel::navigateTo(const QString& path) {
    if (!m_fs->isReady()) return;
    QList<sftp::SftpEntry> entries;
    if (!m_fs->list(path, entries)) return;   // error() already emitted
    m_cwd = path;
    m_pathBar->setText(path);
    // Repopulating with sorting live is O(n log n) per insert; disable while filling.
    const bool wasSorting = m_list->isSortingEnabled();
    m_list->setSortingEnabled(false);
    m_list->clear();
    for (const sftp::SftpEntry& e : entries) {
        auto* item = new SftpItem(m_list);
        item->setText(0, (e.isDir ? QStringLiteral("📁 ") : QString()) + e.name);
        item->setText(1, e.isDir ? QString() : e.sizeString());
        const QString when = e.mtime > 0
            ? QDateTime::fromSecsSinceEpoch(e.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();
        item->setText(2, when);
        item->setText(3, e.permString());
        item->setData(0, Qt::UserRole, e.name);
        item->setData(0, Qt::UserRole + 1, e.isDir);
        item->setData(0, Qt::UserRole + 2, static_cast<uint>(e.permissions));
        item->setData(1, Qt::UserRole, static_cast<qulonglong>(e.size));  // numeric sort key
        item->setData(2, Qt::UserRole, static_cast<qlonglong>(e.mtime));  // chronological sort key
    }
    m_list->setSortingEnabled(wasSorting);
    setStatus(QStringLiteral("%1 items").arg(entries.size()));
}

void SftpPanel::refresh() { navigateTo(m_cwd); }

void SftpPanel::goUp() { navigateTo(sftp::RemotePath::parent(m_cwd)); }

void SftpPanel::goHome() { if (!m_home.isEmpty()) navigateTo(m_home); }

void SftpPanel::setRemoteCwd(const QString& absPath) {
    if (!m_follow || !m_follow->isChecked()) return;
    if (absPath.isEmpty() || absPath == m_cwd) return;
    if (!m_fs->isReady()) return;
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
    return m_fs->download(remotePath, localPath);
}

void SftpPanel::editRemote(const QString& remotePath) {
    if (!m_fs->isReady()) return;
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
        const qint64 n = m_fs->upload(savedLocal, remote);
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

qint64 SftpPanel::downloadTree(const QString& remote, const QString& local, bool isDir,
                               QProgressDialog& prog) {
    if (prog.wasCanceled()) return -1;
    if (!isDir) {
        prog.setLabelText(QStringLiteral("Downloading %1").arg(remote));
        QApplication::processEvents();
        return downloadTo(remote, local);
    }
    // Mirror the remote directory locally, then recurse into its entries.
    if (!QDir().mkpath(local)) return -1;
    QList<sftp::SftpEntry> entries;
    if (!m_fs->list(remote, entries)) return -1;
    qint64 total = 0;
    for (const sftp::SftpEntry& e : entries) {
        if (e.name == QStringLiteral(".") || e.name == QStringLiteral("..")) continue;
        if (prog.wasCanceled()) return -1;
        const qint64 n = downloadTree(sftp::RemotePath::join(remote, e.name),
                                      QDir(local).filePath(e.name), e.isDir, prog);
        if (n < 0) return -1;
        total += n;
    }
    return total;
}

qint64 SftpPanel::uploadTree(const QString& local, const QString& remote, QProgressDialog& prog) {
    if (prog.wasCanceled()) return -1;
    QFileInfo fi(local);
    if (!fi.isDir()) {
        prog.setLabelText(QStringLiteral("Uploading %1").arg(fi.fileName()));
        QApplication::processEvents();
        return m_fs->upload(local, remote);
    }
    m_fs->makeDir(remote);   // best-effort; may already exist
    qint64 total = 0;
    QDirIterator it(local, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        if (prog.wasCanceled()) return -1;
        const QString name = it.fileName();
        const qint64 n = uploadTree(it.filePath(), sftp::RemotePath::join(remote, name), prog);
        if (n < 0) return -1;
        total += n;
    }
    return total;
}

void SftpPanel::download() {
    bool isDir = false;
    const QString name = selectedName(&isDir);
    if (name.isEmpty() || !m_fs->isReady()) return;
    const QString remote = sftp::RemotePath::join(m_cwd, name);
    QString local;
    if (isDir) {
        const QString into = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Download folder into…"));
        if (into.isEmpty()) return;
        local = QDir(into).filePath(name);
    } else {
        local = QFileDialog::getSaveFileName(this, QStringLiteral("Download to"), name);
    }
    if (local.isEmpty()) return;
    QProgressDialog prog(QStringLiteral("Downloading…"), QStringLiteral("Cancel"), 0, 0, this);
    prog.setWindowModality(Qt::WindowModal);
    const qint64 n = downloadTree(remote, local, isDir, prog);
    prog.close();
    setStatus(n >= 0 ? QStringLiteral("Downloaded %1 bytes").arg(n)
             : prog.wasCanceled() ? QStringLiteral("Download canceled")
                                   : QStringLiteral("Download failed"));
}

void SftpPanel::upload() {
    if (!m_fs->isReady()) return;
    const QString local = QFileDialog::getOpenFileName(this, QStringLiteral("Upload file"));
    if (local.isEmpty()) return;
    const QString remote = sftp::RemotePath::join(m_cwd, QFileInfo(local).fileName());
    const qint64 n = m_fs->upload(local, remote);
    setStatus(n >= 0 ? QStringLiteral("Uploaded %1 bytes").arg(n)
                     : QStringLiteral("Upload failed"));
    if (n >= 0) refresh();
}

void SftpPanel::uploadFolder() {
    if (!m_fs->isReady()) return;
    const QString local = QFileDialog::getExistingDirectory(this, QStringLiteral("Upload folder"));
    if (local.isEmpty()) return;
    const QString remote = sftp::RemotePath::join(m_cwd, QFileInfo(local).fileName());
    QProgressDialog prog(QStringLiteral("Uploading…"), QStringLiteral("Cancel"), 0, 0, this);
    prog.setWindowModality(Qt::WindowModal);
    const qint64 n = uploadTree(local, remote, prog);
    prog.close();
    setStatus(n >= 0 ? QStringLiteral("Uploaded %1 bytes").arg(n)
             : prog.wasCanceled() ? QStringLiteral("Upload canceled")
                                   : QStringLiteral("Upload failed"));
    if (n >= 0) refresh();
}

void SftpPanel::showContextMenu(const QPoint& pos) {
    if (!m_fs->isReady()) return;
    bool isDir = false;
    const QString name = selectedName(&isDir);
    QMenu menu(this);
    if (!name.isEmpty() && name != QStringLiteral(".")) {
        const QString target = sftp::RemotePath::join(m_cwd, name);
        if (!isDir) {
            menu.addAction(QStringLiteral("Edit…"), this, [this, target] { editRemote(target); });
            menu.addAction(QStringLiteral("Download…"), this, &SftpPanel::download);
            menu.addAction(QStringLiteral("Download via SCP…"), this, [this, target, name] {
                const QString local = QFileDialog::getSaveFileName(
                    this, QStringLiteral("Download (SCP) to"), name);
                if (local.isEmpty()) return;
                const qint64 n = m_fs->scpDownload(target, local);
                setStatus(n >= 0 ? QStringLiteral("SCP downloaded %1 bytes").arg(n)
                                 : QStringLiteral("SCP download failed"));
            });
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
            if (valid && m_fs->chmod(target, m)) refresh();
        });
        menu.addAction(QStringLiteral("Rename…"), this, [this, target, name] {
            bool ok = false;
            const QString nn = QInputDialog::getText(this, QStringLiteral("Rename"),
                QStringLiteral("New name:"), QLineEdit::Normal, name, &ok);
            if (ok && !nn.isEmpty() && m_fs->rename(target, sftp::RemotePath::join(m_cwd, nn)))
                refresh();
        });
        menu.addAction(QStringLiteral("Delete"), this, [this, target, isDir, name] {
            if (QMessageBox::question(this, QStringLiteral("Delete"),
                    QStringLiteral("Delete %1?").arg(name)) != QMessageBox::Yes) return;
            const bool ok = isDir ? m_fs->removeDir(target) : m_fs->removeFile(target);
            if (ok) refresh();
        });
        menu.addSeparator();
    }
    menu.addAction(QStringLiteral("New folder…"), this, [this] {
        bool ok = false;
        const QString nn = QInputDialog::getText(this, QStringLiteral("New folder"),
            QStringLiteral("Folder name:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !nn.isEmpty() && m_fs->makeDir(sftp::RemotePath::join(m_cwd, nn))) refresh();
    });
    menu.addAction(QStringLiteral("Upload file…"), this, &SftpPanel::upload);
    menu.addAction(QStringLiteral("Upload folder…"), this, &SftpPanel::uploadFolder);
    menu.addAction(QStringLiteral("Refresh"), this, &SftpPanel::refresh);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

// ── Drag-and-drop ───────────────────────────────────────────────────────────

void SftpPanel::dragEnterEvent(QDragEnterEvent* e) {
    if (m_fs->isReady() && e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void SftpPanel::dropEvent(QDropEvent* e) {
    if (!m_fs->isReady() || !e->mimeData()->hasUrls()) return;
    QProgressDialog prog(QStringLiteral("Uploading…"), QStringLiteral("Cancel"), 0, 0, this);
    prog.setWindowModality(Qt::WindowModal);
    int ok = 0;
    for (const QUrl& url : e->mimeData()->urls()) {
        if (!url.isLocalFile() || prog.wasCanceled()) continue;
        const QString local = url.toLocalFile();
        const QString remote = sftp::RemotePath::join(m_cwd, QFileInfo(local).fileName());
        if (uploadTree(local, remote, prog) >= 0) ++ok;   // handles files and folders
    }
    prog.close();
    setStatus(QStringLiteral("Uploaded %1 item(s)").arg(ok));
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
                if (!name.isEmpty() && !isDir && m_fs->isReady()) {
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
