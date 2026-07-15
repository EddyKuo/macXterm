#pragma once
#include "sftp/IRemoteFs.h"
#include <QString>
#include <functional>

namespace macxterm::sftp {

// Recursive file/directory transfer over any IRemoteFs backend (SFTP or FTP).
// Pure of UI so it is unit-testable end-to-end against the built-in FtpServer.
//   canceled : polled between items; return true to abort (→ returns -1).
//   onItem   : called with each file's path as it starts transferring (for UI).
// Both callbacks are optional. Return total bytes transferred, or -1 on error.

qint64 downloadTree(IRemoteFs& fs, const QString& remote, const QString& local, bool isDir,
                    const std::function<bool()>& canceled = {},
                    const std::function<void(const QString&)>& onItem = {});

qint64 uploadTree(IRemoteFs& fs, const QString& local, const QString& remote,
                  const std::function<bool()>& canceled = {},
                  const std::function<void(const QString&)>& onItem = {});

} // namespace macxterm::sftp
