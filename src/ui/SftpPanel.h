#pragma once
#include "sftp/SftpConnection.h"
#include "core/Session.h"
#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QLabel;

namespace macxterm::ui {

// Graphical SFTP browser panel (MobaXterm's SFTP side pane). Lists the remote
// directory, navigates on double-click, and uploads/downloads files. Backed by
// sftp::SftpConnection over its own dedicated SSH session for the given session.
class SftpPanel : public QWidget {
    Q_OBJECT
public:
    explicit SftpPanel(QWidget* parent = nullptr);

    // Connect the panel to a session (SSH/SFTP). Returns false on failure.
    bool openFor(const core::Session& session);
    bool isConnected() const { return m_sftp.isReady(); }

private slots:
    void refresh();
    void goUp();
    void onItemActivated();
    void download();
    void upload();

private:
    void navigateTo(const QString& path);
    void setStatus(const QString& msg);

    sftp::SftpConnection m_sftp;
    QLineEdit* m_pathBar = nullptr;
    QTreeWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QString m_cwd = QStringLiteral(".");
};

} // namespace macxterm::ui
