#pragma once

#include <QString>

namespace tweakopedia::execution {

enum class ShellStartStatus { Started, Cancelled, Failed };
enum class ShellWaitStatus { Exited, Timeout, Failed };

struct ShellLaunchRequest {
    QString executable;
    QString parameters;
    QString verb;
};

struct ShellStartResult {
    ShellStartStatus status{ShellStartStatus::Failed};
    qint64 processId{};
    quintptr handle{};
    quint32 nativeError{};
};

class IShellExecuteBackend
{
public:
    virtual ~IShellExecuteBackend() = default;
    [[nodiscard]] virtual ShellStartResult start(const ShellLaunchRequest& request) = 0;
    [[nodiscard]] virtual ShellWaitStatus wait(quintptr handle, int timeoutMs) = 0;
    [[nodiscard]] virtual bool terminate(quintptr handle) = 0;
    virtual void close(quintptr handle) = 0;
};

class WindowsShellExecuteBackend final : public IShellExecuteBackend
{
public:
    [[nodiscard]] ShellStartResult start(const ShellLaunchRequest& request) override;
    [[nodiscard]] ShellWaitStatus wait(quintptr handle, int timeoutMs) override;
    [[nodiscard]] bool terminate(quintptr handle) override;
    void close(quintptr handle) override;
};

struct ExecutorLaunchSession {
    bool started{};
    qint64 processId{};
    quintptr handle{};
    QString code;
    QString message;
};

struct ExecutorMonitorResult {
    bool success{};
    QString code;
    QString message;
};

class ExecutorLauncher final
{
public:
    explicit ExecutorLauncher(IShellExecuteBackend& backend);

    [[nodiscard]] ExecutorLaunchSession start(
        const QString& executable,
        const QString& serverName,
        const QString& nonce);
    [[nodiscard]] ExecutorMonitorResult monitor(
        const ExecutorLaunchSession& session,
        int timeoutMs,
        bool transactionCompleted);

private:
    IShellExecuteBackend* backend_{};
};

} // namespace tweakopedia::execution
