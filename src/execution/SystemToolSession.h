#pragma once

#include "execution/ExecutorLauncher.h"
#include "execution/ExecutorServer.h"
#include "execution/ISystemToolRunner.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>

#include <memory>

namespace tweakopedia::execution {

class SystemToolSession final : public ISystemToolRunner
{
    Q_OBJECT

public:
    SystemToolSession(
        IShellExecuteBackend& shellBackend,
        QString executorPath,
        int pollIntervalMs = 250,
        int inactivityTimeoutMs = 90 * 60 * 1000,
        QObject* parent = nullptr);
    ~SystemToolSession() override;

    bool start(SystemToolOperation operation) override;

private:
    void launchFinished();
    void pollProcess();
    void checkWatchdog();
    void touchActivity();
    void fail(QString code, QString message, bool terminateProcess = false);
    void resetSession(bool closeHandle);

    IShellExecuteBackend* shellBackend_{};
    ExecutorLauncher launcher_;
    QString executorPath_;
    int inactivityTimeoutMs_{};
    QFutureWatcher<ExecutorLaunchSession> launchWatcher_;
    QTimer processTimer_;
    QTimer watchdogTimer_;
    QElapsedTimer activity_;
    std::unique_ptr<ExecutorServer> server_;
    ExecutorLaunchSession launchSession_;
    SystemToolRequest request_;
    bool active_{};
    bool resultReceived_{};
};

} // namespace tweakopedia::execution
