#pragma once
#include "sftp/SftpConnection.h"
#include "core/Session.h"
#include <QWidget>
#include <QHash>

class QLineEdit;
class QTreeWidget;
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
    explicit SftpPanel(QWidget* parent = nullptr);

    // Connect the panel to a session (SSH/SFTP). Returns false on failure.
    bool openFor(const core::Session& session);
    bool isConnected() const { return m_sftp.isReady(); }

public slots:
    // Called by MainWindow when the associated terminal's working directory
    // changes (via OSC 7). No-op unless "follow terminal folder" is checked.
    void setRemoteCwd(const QString& absPath);

private slots:
    void refresh();
    void goUp();
    void onItemActivated();
    void download();
    void upload();
    void showContextMenu(const QPoint& pos);

private:
    void navigateTo(const QString& path);
    void setStatus(const QString& msg);
    QString selectedName(bool* isDir = nullptr) const;
    void editRemote(const QString& remotePath);
    qint64 downloadTo(const QString& remotePath, const QString& localPath);

    // Drag-and-drop: accept OS file drops (upload) and start drags (download).
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

    sftp::SftpConnection m_sftp;
    QLineEdit* m_pathBar = nullptr;
    QTreeWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QToolButton* m_follow = nullptr;
    QString m_cwd = QStringLiteral(".");
    QPoint m_dragStart;
    // Maps a temp local path (being edited) -> its remote path, for re-upload.
    QHash<QString, QString> m_editing;
};

} // namespace macxterm::ui
