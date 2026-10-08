#include "execution/ExecutorLauncher.h"

#include <QtTest/QTest>

using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

namespace {

class FakeShellExecuteBackend final : public IShellExecuteBackend
{
public:
    ShellStartResult startResult{.status = ShellStartStatus::Started, .processId = 42, .handle = 7};
    ShellWaitStatus waitResult{ShellWaitStatus::Exited};
    ShellLaunchRequest request;
    bool closed{};
    bool terminated{};

    ShellStartResult start(const ShellLaunchRequest& value) override
    {
        request = value;
        return startResult;
    }

    ShellWaitStatus wait(quintptr, int) override { return waitResult; }
    bool terminate(quintptr) override { terminated = true; return true; }
    void close(quintptr) override { closed = true; }
};

} // namespace

class ExecutorLauncherTest final : public QObject
{
    Q_OBJECT

private slots:
    void usesRunasWithoutEmbeddingPlan()
    {
        FakeShellExecuteBackend backend;
        ExecutorLauncher launcher(backend);

        const auto started = launcher.start(
            u"D:/Portable/Tweakopedia.Executor.exe"_s,
            u"local-server"_s,
            u"one-time-nonce"_s);

        QVERIFY(started.started);
        QCOMPARE(started.processId, qint64{42});
        QCOMPARE(backend.request.verb, u"runas"_s);
        QVERIFY(backend.request.parameters.contains(u"local-server"_s));
        QVERIFY(backend.request.parameters.contains(u"one-time-nonce"_s));
        QVERIFY(!backend.request.parameters.contains(u"operations"_s));
        QVERIFY(!backend.request.parameters.contains(u"HKLM"_s));
    }

    void reportsUacCancellation()
    {
        FakeShellExecuteBackend backend;
        backend.startResult = {.status = ShellStartStatus::Cancelled};
        ExecutorLauncher launcher(backend);

        const auto result = launcher.start(u"executor.exe"_s, u"server"_s, u"nonce"_s);

        QVERIFY(!result.started);
        QCOMPARE(result.code, u"launch.cancelled"_s);
    }

    void reportsLaunchFailure()
    {
        FakeShellExecuteBackend backend;
        backend.startResult = {.status = ShellStartStatus::Failed, .nativeError = 2};
        ExecutorLauncher launcher(backend);

        const auto result = launcher.start(u"missing.exe"_s, u"server"_s, u"nonce"_s);

        QVERIFY(!result.started);
        QCOMPARE(result.code, u"launch.failed"_s);
    }

    void reportsTimeoutAndClosesHandle()
    {
        FakeShellExecuteBackend backend;
        backend.waitResult = ShellWaitStatus::Timeout;
        ExecutorLauncher launcher(backend);
        const auto session = launcher.start(u"executor.exe"_s, u"server"_s, u"nonce"_s);

        const auto result = launcher.monitor(session, 50, false);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"launch.timeout"_s);
        QVERIFY(backend.terminated);
        QVERIFY(backend.closed);
    }

    void processExitBeforeIpcResultIsFailure()
    {
        FakeShellExecuteBackend backend;
        ExecutorLauncher launcher(backend);
        const auto session = launcher.start(u"executor.exe"_s, u"server"_s, u"nonce"_s);

        const auto result = launcher.monitor(session, 5000, false);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"launch.exited_early"_s);
        QVERIFY(backend.closed);
    }
};

QTEST_APPLESS_MAIN(ExecutorLauncherTest)

#include "ExecutorLauncherTest.moc"
