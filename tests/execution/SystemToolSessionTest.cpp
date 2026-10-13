#include "execution/ExecutorClient.h"
#include "execution/SystemToolSession.h"

#include <QCoreApplication>
#include <QMutex>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest/QTest>

using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

namespace {

class SessionShellBackend final : public IShellExecuteBackend
{
public:
    ShellStartResult start(const ShellLaunchRequest& value) override
    {
        QMutexLocker lock(&mutex);
        request = value;
        started = true;
        return startResult;
    }
    ShellWaitStatus wait(quintptr, int) override
    {
        QMutexLocker lock(&mutex);
        return waitResult;
    }
    bool terminate(quintptr) override
    {
        QMutexLocker lock(&mutex);
        terminated = true;
        return true;
    }
    void close(quintptr) override
    {
        QMutexLocker lock(&mutex);
        closed = true;
    }

    QString parameters()
    {
        QMutexLocker lock(&mutex);
        return request.parameters;
    }

    QMutex mutex;
    ShellStartResult startResult{
        .status = ShellStartStatus::Started,
        .processId = QCoreApplication::applicationPid(),
        .handle = 7};
    ShellWaitStatus waitResult{ShellWaitStatus::Timeout};
    ShellLaunchRequest request;
    bool started{};
    bool terminated{};
    bool closed{};
};

std::pair<QString, QString> connectionArguments(const QString& parameters)
{
    const QRegularExpression expression(
        u"--server \\\"([^\\\"]+)\\\" --nonce \\\"([^\\\"]+)\\\""_s);
    const auto match = expression.match(parameters);
    return {match.captured(1), match.captured(2)};
}

} // namespace

class SystemToolSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SystemToolRequest>();
        qRegisterMetaType<SystemToolStageEvent>();
        qRegisterMetaType<SystemToolOutputEvent>();
        qRegisterMetaType<SystemToolResult>();
    }

    void streamsOneAuthenticatedRequestWithoutBlocking()
    {
        SessionShellBackend backend;
        SystemToolSession session(backend, u"executor.exe"_s, 10, 5000);
        QSignalSpy stages(&session, &SystemToolSession::stageChanged);
        QSignalSpy output(&session, &SystemToolSession::outputReceived);
        QSignalSpy finished(&session, &SystemToolSession::finished);

        QVERIFY(session.start(SystemToolOperation::Scan));
        QVERIFY(!session.start(SystemToolOperation::Restore));
        QTRY_VERIFY_WITH_TIMEOUT(backend.started, 3000);
        const auto [server, nonce] = connectionArguments(backend.parameters());
        QVERIFY(!server.isEmpty());
        ExecutorClient client;
        QSignalSpy request(&client, &ExecutorClient::toolRequestReceived);
        client.connectToServer(server, nonce);
        QTRY_COMPARE_WITH_TIMEOUT(request.size(), 1, 3000);
        const auto typedRequest = qvariant_cast<SystemToolRequest>(request.first().first());

        QVERIFY(client.sendToolStage({typedRequest.requestId, SystemToolStage::ScanHealth}));
        QVERIFY(client.sendToolOutput(typedRequest.requestId, u"live output\n"_s));
        QVERIFY(client.sendToolResult({
            .requestId = typedRequest.requestId,
            .status = SystemToolResultStatus::Completed,
            .exitCode = 0,
            .health = ComponentStoreHealth::Healthy,
            .logPath = u"C:/logs/scan.log"_s,
        }));
        QTRY_COMPARE_WITH_TIMEOUT(stages.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(output.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    }

    void reportsUacCancellationAndAllowsRetry()
    {
        SessionShellBackend backend;
        backend.startResult = {.status = ShellStartStatus::Cancelled};
        SystemToolSession session(backend, u"executor.exe"_s, 10, 5000);
        QSignalSpy cancelled(&session, &SystemToolSession::launchCancelled);
        QVERIFY(session.start(SystemToolOperation::Scan));
        QTRY_COMPARE_WITH_TIMEOUT(cancelled.size(), 1, 3000);
        QVERIFY(session.start(SystemToolOperation::Scan));
    }

    void reportsEarlyExitAndWatchdogTimeout()
    {
        SessionShellBackend exitedBackend;
        exitedBackend.waitResult = ShellWaitStatus::Exited;
        SystemToolSession exited(exitedBackend, u"executor.exe"_s, 10, 5000);
        QSignalSpy earlyFailure(&exited, &SystemToolSession::failed);
        QVERIFY(exited.start(SystemToolOperation::Scan));
        QTRY_COMPARE_WITH_TIMEOUT(earlyFailure.size(), 1, 3000);
        QCOMPARE(earlyFailure.first().first().toString(), u"launch.exited_early"_s);

        SessionShellBackend stuckBackend;
        SystemToolSession stuck(stuckBackend, u"executor.exe"_s, 5, 30);
        QSignalSpy timeout(&stuck, &SystemToolSession::failed);
        QVERIFY(stuck.start(SystemToolOperation::Scan));
        QTRY_COMPARE_WITH_TIMEOUT(timeout.size(), 1, 3000);
        QCOMPARE(timeout.first().first().toString(), u"tool.timeout"_s);
        QVERIFY(stuckBackend.terminated);
    }
};

QTEST_GUILESS_MAIN(SystemToolSessionTest)
#include "SystemToolSessionTest.moc"
