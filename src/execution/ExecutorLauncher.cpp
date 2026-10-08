#include "execution/ExecutorLauncher.h"

#include <windows.h>
#include <shellapi.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

QString quoted(QString value)
{
    value.replace(u'"', u"\\\""_s);
    return u'"' + value + u'"';
}

} // namespace

ShellStartResult WindowsShellExecuteBackend::start(const ShellLaunchRequest& request)
{
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = reinterpret_cast<LPCWSTR>(request.verb.utf16());
    info.lpFile = reinterpret_cast<LPCWSTR>(request.executable.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(request.parameters.utf16());
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        const auto error = GetLastError();
        return {
            .status = error == ERROR_CANCELLED ? ShellStartStatus::Cancelled : ShellStartStatus::Failed,
            .nativeError = error,
        };
    }
    return {
        .status = ShellStartStatus::Started,
        .processId = static_cast<qint64>(GetProcessId(info.hProcess)),
        .handle = reinterpret_cast<quintptr>(info.hProcess),
    };
}

ShellWaitStatus WindowsShellExecuteBackend::wait(quintptr handle, int timeoutMs)
{
    const auto status = WaitForSingleObject(
        reinterpret_cast<HANDLE>(handle),
        timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
    if (status == WAIT_OBJECT_0) return ShellWaitStatus::Exited;
    if (status == WAIT_TIMEOUT) return ShellWaitStatus::Timeout;
    return ShellWaitStatus::Failed;
}

void WindowsShellExecuteBackend::close(quintptr handle)
{
    if (handle) CloseHandle(reinterpret_cast<HANDLE>(handle));
}

ExecutorLauncher::ExecutorLauncher(IShellExecuteBackend& backend)
    : backend_(&backend)
{
}

ExecutorLaunchSession ExecutorLauncher::start(
    const QString& executable,
    const QString& serverName,
    const QString& nonce)
{
    const auto native = backend_->start({
        .executable = executable,
        .parameters = u"--server "_s + quoted(serverName) + u" --nonce "_s + quoted(nonce),
        .verb = u"runas"_s,
    });
    if (native.status == ShellStartStatus::Cancelled) {
        return {.code = u"launch.cancelled"_s, .message = u"Запрос UAC отменён."_s};
    }
    if (native.status != ShellStartStatus::Started || !native.handle || native.processId <= 0) {
        return {.code = u"launch.failed"_s, .message = u"Executor не удалось запустить."_s};
    }
    return {
        .started = true,
        .processId = native.processId,
        .handle = native.handle,
    };
}

ExecutorMonitorResult ExecutorLauncher::monitor(
    const ExecutorLaunchSession& session,
    int timeoutMs,
    bool transactionCompleted)
{
    if (!session.started) return {.code = session.code, .message = session.message};
    const auto status = backend_->wait(session.handle, timeoutMs);
    backend_->close(session.handle);
    if (status == ShellWaitStatus::Timeout) {
        return {.code = u"launch.timeout"_s, .message = u"Executor не завершился вовремя."_s};
    }
    if (status != ShellWaitStatus::Exited) {
        return {.code = u"launch.wait_failed"_s, .message = u"Не удалось дождаться Executor."_s};
    }
    if (!transactionCompleted) {
        return {.code = u"launch.exited_early"_s, .message = u"Executor завершился без результата IPC."_s};
    }
    return {.success = true};
}

} // namespace tweakopedia::execution
