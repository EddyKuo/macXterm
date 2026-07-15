#pragma once
#include "sftp/IRemoteFs.h"
#include "core/Session.h"
#include <QWidget>
#include <QHash>

class QLineEdit;
class QTreeWidget;
class QProgressDialog;
class QLabel;
class QToolButton;
class QPoint;

namespace macxterm::ui {

// Graphical SFTP browser panel (MobaXterm's SFTP side pane). Lists the remote
// directory and navigates on double-click. Supports:
//   - upload/download via buttons AND drag-and-drop (drop OS files to upload;
//     drag a remote file out to download it to the drop target),
//   - double-click a remote file to edit it in the built-in editor, auto
//     re-uploading on save,
//   - a right-click context menu (download / edit / chmod / rename / delete /
//     new folder),
//   - "follow terminal folder": when enabled, setRemoteCwd() re-homes the panel
//     to the directory the terminal is currently in.
// Backed by sftp::SftpConnection over a dedicated SSH session.
class SftpPanel : public QWidget {
    Q_OBJECT
public:
    // Which remote-filesystem backend the panel drives.
    enum class Backend { Sftp, Ftp };
    explicit SftpPanel(QWidget* parent = nullptr);
    SftpPanel(Backend backend, QWidget* parent = nullptr);

    // Connect the panel to a session (SSH/SFTP or FTP). Returns false on failure.
    bool openFor(const core::Session& session);
    bool isConnected() const { return m_fs && m_fs->isReady(); }

    // Tear down the current connection and clear the view (called when the owning
    // terminal session closes). The panel stays reusable — a later openFor()
    // reconnects on a fresh backend.
    void closeSession();

public slots:
    // Called by MainWindow when the associated terminal's working directory
    // changes (via OSC 7). No-op unless "follow terminal folder" is checked.
    void setRemoteCwd(const QString& absPath);

private slots:
    void refresh();
    void goUp();
    void goHome();
    void onItemActivated();
    void download();
    void upload();
    void uploadFolder();
    void showContextMenu(const QPoint& pos);

private:
    void navigateTo(const QString& path);
    void setStatus(const QString& msg);
    QString selectedName(bool* isDir = nullptr) const;
    void editRemote(const QString& remotePath);
    qint64 downloadTo(const QString& remotePath, const QString& localPath);
    // Recursive transfer of a file or whole directory tree. Report progress and
    // honour cancellation via the shared QProgressDialog. Return bytes or -1.
    qint64 downloadTree(const QString& remote, const QString& local, bool isDir,
                        QProgressDialog& prog);
    qint64 uploadTree(const QString& local, const QString& remote, QProgressDialog& prog);

    // Drag-and-drop: accept OS file drops (upload) and start drags (download).
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

    void initBackend(Backend backend);
    Backend m_backend = Backend::Sftp;    // which backend this panel drives (for reconnect)
    sftp::IRemoteFs* m_fs = nullptr;      // SFTP or FTP backend (owned via QObject parent)
    QObject* m_fsObj = nullptr;           // same object as QObject, for signal wiring
    QLineEdit* m_pathBar = nullptr;
    QTreeWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QToolButton* m_follow = nullptr;
    QString m_cwd = QStringLiteral(".");
    QString m_home;   // initial remote directory (for the Home button)
    QPoint m_dragStart;
    // Maps a temp local path (being edited) -> its remote path, for re-upload.
    QHash<QString, QString> m_editing;
};

} // namespace macxterm::ui
