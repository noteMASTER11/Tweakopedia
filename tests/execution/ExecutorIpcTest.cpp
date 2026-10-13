#include "execution/ExecutionProtocol.h"
#include "execution/ExecutorClient.h"
#include "execution/ExecutorServer.h"
#include "execution/RegistrySnapshot.h"
#include "execution/FileOperationExecutor.h"
#include "execution/ScheduledTaskExecutor.h"
#include "execution/PowerSettingExecutor.h"
#include "execution/SystemToolProtocol.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <algorithm>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

planning::ExecutionPlan samplePlan()
{
    return {
        .transactionId = QUuid::createUuid(),
        .profile = {
            .family = domain::WindowsFamily::Windows11,
            .build = 26200,
            .ubr = 1000,
            .edition = u"Professional"_s,
            .architecture = domain::CpuArchitecture::X64,
        },
        .createdAtUtc = QDateTime::currentDateTimeUtc(),
        .operations = {planning::PlannedRegistryValueChange{
            .tweakId = domain::TweakId::parse(u"filesystem.win32-long-paths"_s).value(),
            .targetState = u"enabled"_s,
            .change = domain::SetRegistryValueOperation{
                .location = {
                    .hive = domain::RegistryHive::LocalMachine,
                    .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
                    .valueName = u"LongPathsEnabled"_s,
                    .view = domain::RegistryView::Registry64,
                },
                .value = domain::RegistryValueSpec::string(u"enabled"),
            },
            .beforeFingerprint = execution::RegistrySnapshot::fingerprint(
                platform::RegistryReadResult::missing()),
        }},
        .summary = u"Смешанный пакет"_s,
    };
}

} // namespace

class ExecutorIpcTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<execution::SystemToolRequest>();
        qRegisterMetaType<execution::SystemToolStageEvent>();
        qRegisterMetaType<execution::SystemToolOutputEvent>();
        qRegisterMetaType<execution::SystemToolProgressEvent>();
        qRegisterMetaType<execution::SystemToolResult>();
    }

    void acknowledgesDeliveredResult()
    {
        const auto serverName = u"tweakopedia-ack-test-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY2(server.listen(serverName, nonce), qPrintable(server.errorString()));
        server.setExpectedProcessId(QCoreApplication::applicationPid());

        execution::ExecutorClient client;
        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy completed(&server, &execution::ExecutorServer::completed);
        QSignalSpy acknowledged(&client, &execution::ExecutorClient::resultAcknowledged);
        client.connectToServer(serverName, nonce);

        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 3000);
        QVERIFY(client.sendResult({{u"status"_s, u"succeeded"_s}}));
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(acknowledged.size(), 1, 3000);
    }

    void runsOneShotAuthenticatedExecutorSession()
    {
        const auto serverName = u"tweakopedia-test-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY2(server.listen(serverName, nonce), qPrintable(server.errorString()));

        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy progress(&server, &execution::ExecutorServer::progressReceived);
        QSignalSpy completed(&server, &execution::ExecutorServer::completed);
        QSignalSpy rejected(&server, &execution::ExecutorServer::clientRejected);

        QProcess process;
        process.setProgram(QStringLiteral(TWEAKOPEDIA_TEST_EXECUTOR));
        process.setArguments({u"--server"_s, serverName, u"--nonce"_s, nonce, u"--test-mode"_s});
        process.start();
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        server.setExpectedProcessId(process.processId());

        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 5000);
        QCOMPARE(authenticated.first().first().toLongLong(), process.processId());

        QTemporaryDir dataRoot;
        QVERIFY(dataRoot.isValid());
        auto plan = samplePlan();
        const auto transactionDirectory = QDir(dataRoot.path()).filePath(
            u"transactions/"_s + plan.transactionId.toString(QUuid::WithoutBraces));
        const auto inputRelative = u"inputs/ipc/new.bin"_s;
        const auto input = QDir(transactionDirectory).filePath(inputRelative);
        QVERIFY(QDir().mkpath(QFileInfo(input).absolutePath()));
        QFile inputFile(input);
        QVERIFY(inputFile.open(QIODevice::WriteOnly));
        QCOMPARE(inputFile.write("new"), 3);
        inputFile.close();
        const auto destination = QDir(dataRoot.path()).filePath(u"destination.bin"_s);
        QFile destinationFile(destination);
        QVERIFY(destinationFile.open(QIODevice::WriteOnly));
        QCOMPARE(destinationFile.write("old"), 3);
        destinationFile.close();
        plan.operations.append(planning::PlannedFileChange{
            .tweakId = *domain::TweakId::parse(u"devices.ipc-file"_s),
            .targetState = u"configured"_s,
            .change = {.kind = domain::FileOperationKind::Replace,
                       .artifact = {.id = u"file"_s, .storageId = u"ipc"_s,
                                    .managedPath = inputRelative, .size = 3,
                                    .sha256 = QCryptographicHash::hash(
                                        "new", QCryptographicHash::Sha256).toHex()},
                       .destination = destination},
            .beforeFingerprint = execution::FileOperationExecutor::fingerprint(
                {.destination = destination, .existed = true, .size = 3,
                 .sha256 = QCryptographicHash::hash(
                     "old", QCryptographicHash::Sha256).toHex()})});
        const domain::ScheduledTaskLocation task{u"\\Tweakopedia"_s, u"Ipc"_s};
        plan.operations.append(planning::PlannedScheduledTaskChange{
            .tweakId = *domain::TweakId::parse(u"tasks.ipc"_s),
            .targetState = u"disabled"_s,
            .change = {task, false},
            .beforeFingerprint = execution::ScheduledTaskExecutor::fingerprint({task, true})});
        const domain::PowerSettingLocation power{
            u"active"_s, u"54533251-82be-4824-96c1-47b60b740d00"_s,
            u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s, domain::PowerSource::Ac};
        plan.operations.append(planning::PlannedPowerSettingChange{
            .tweakId = *domain::TweakId::parse(u"power.ipc"_s),
            .targetState = u"maximum"_s, .change = {power, 100},
            .beforeFingerprint = execution::PowerSettingExecutor::fingerprint(
                {power, u"381b4222-f694-41f0-9685-ff5bb260df2e"_s, 7})});
        QVERIFY(server.sendPlan(
            execution::ExecutionProtocol::encode(plan),
            dataRoot.path(),
            transactionDirectory));

        QTRY_VERIFY_WITH_TIMEOUT(!progress.isEmpty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 5000);
        QCOMPARE(completed.first().first().toJsonObject().value(u"status"_s).toString(),
                 u"succeeded"_s);

        persistence::TransactionFiles files(QDir(dataRoot.path()).filePath(u"transactions"_s));
        const auto before = files.readBefore(plan.transactionId);
        QVERIFY(before.has_value());
        const auto snapshots = before->value(u"operations"_s).toArray();
        QCOMPARE(snapshots.size(), 4);
        QCOMPARE(snapshots.at(0).toObject().value(u"snapshotType"_s).toString(),
                 u"registry.value"_s);
        QCOMPARE(snapshots.at(1).toObject().value(u"type"_s).toString(), u"file"_s);
        QCOMPARE(snapshots.at(2).toObject().value(u"type"_s).toString(), u"scheduled_task"_s);
        QCOMPARE(snapshots.at(3).toObject().value(u"type"_s).toString(), u"power.setting"_s);
        QVERIFY(std::any_of(progress.cbegin(), progress.cend(), [](const auto& entry) {
            return entry.at(1).toString().contains(u"registry.value"_s)
                && entry.at(1).toString().contains(u"1/4"_s);
        }));
        persistence::Database database;
        QVERIFY(database.open(QDir(dataRoot.path()).filePath(u"tweakopedia.db"_s)));
        persistence::TransactionRepository repository(database);
        const auto record = repository.find(plan.transactionId);
        QVERIFY(record.has_value());
        QCOMPARE(record->status, persistence::TransactionStatus::Succeeded);

        execution::ExecutorClient second;
        QSignalSpy secondRejected(&second, &execution::ExecutorClient::failed);
        second.connectToServer(serverName, nonce);
        QTRY_VERIFY_WITH_TIMEOUT(!secondRejected.isEmpty(), 3000);
        QVERIFY(!rejected.isEmpty());

        if (process.state() != QProcess::NotRunning) {
            QVERIFY(process.waitForFinished(5000));
        }
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    void streamsAuthenticatedSystemToolMessages()
    {
        const auto serverName = u"tweakopedia-tool-test-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY2(server.listen(serverName, nonce), qPrintable(server.errorString()));
        server.setExpectedProcessId(QCoreApplication::applicationPid());

        execution::ExecutorClient client;
        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy requestReceived(&client, &execution::ExecutorClient::toolRequestReceived);
        QSignalSpy stageReceived(&server, &execution::ExecutorServer::toolStageReceived);
        QSignalSpy outputReceived(&server, &execution::ExecutorServer::toolOutputReceived);
        QSignalSpy progressReceived(&server, &execution::ExecutorServer::toolProgressReceived);
        QSignalSpy resultReceived(&server, &execution::ExecutorServer::toolResultReceived);
        QSignalSpy acknowledged(&client, &execution::ExecutorClient::toolResultAcknowledged);
        client.connectToServer(serverName, nonce);
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 3000);

        const execution::SystemToolRequest request{
            QUuid::createUuid(), execution::SystemToolKind::ComponentStore,
            execution::SystemToolOperation::Scan, {}};
        QVERIFY(server.sendToolRequest(request));
        QTRY_COMPARE_WITH_TIMEOUT(requestReceived.size(), 1, 3000);
        QCOMPARE(qvariant_cast<execution::SystemToolRequest>(
                     requestReceived.first().first()).requestId, request.requestId);

        QVERIFY(client.sendToolStage({request.requestId, execution::SystemToolStage::ScanHealth}));
        QVERIFY(client.sendToolOutput(request.requestId, u"chunk one"_s));
        QVERIFY(client.sendToolOutput(request.requestId, u"chunk two"_s));
        QVERIFY(client.sendToolProgress(
            {request.requestId, execution::SystemToolStage::ScanHealth, 43}));
        QVERIFY(client.sendToolResult({
            .requestId = request.requestId,
            .status = execution::SystemToolResultStatus::Completed,
            .exitCode = 0,
            .outcome = execution::SystemToolOutcome::Healthy,
            .logPath = u"C:/logs/tool.log"_s,
        }));

        QTRY_COMPARE_WITH_TIMEOUT(stageReceived.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(outputReceived.size(), 2, 3000);
        QCOMPARE(qvariant_cast<execution::SystemToolOutputEvent>(
                     outputReceived.at(0).first()).sequence, 0);
        QCOMPARE(qvariant_cast<execution::SystemToolOutputEvent>(
                     outputReceived.at(1).first()).sequence, 1);
        QTRY_COMPARE_WITH_TIMEOUT(progressReceived.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(resultReceived.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(acknowledged.size(), 1, 3000);
    }

    void rejectsToolEventForAnotherRequest()
    {
        const auto serverName = u"tweakopedia-tool-reject-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY(server.listen(serverName, nonce));
        server.setExpectedProcessId(QCoreApplication::applicationPid());
        execution::ExecutorClient client;
        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy rejected(&server, &execution::ExecutorServer::clientRejected);
        client.connectToServer(serverName, nonce);
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 3000);

        const execution::SystemToolRequest request{
            QUuid::createUuid(), execution::SystemToolKind::ComponentStore,
            execution::SystemToolOperation::Scan, {}};
        QVERIFY(server.sendToolRequest(request));
        QVERIFY(client.sendToolStage(
            {QUuid::createUuid(), execution::SystemToolStage::ScanHealth}));
        QTRY_COMPARE_WITH_TIMEOUT(rejected.size(), 1, 3000);
        QCOMPARE(rejected.first().first().toString(), u"tool.request_mismatch"_s);
    }

    void runsOneShotSystemIntegrityExecutorSession()
    {
        const auto serverName = u"tweakopedia-tool-e2e-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY(server.listen(serverName, nonce));
        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy stages(&server, &execution::ExecutorServer::toolStageReceived);
        QSignalSpy output(&server, &execution::ExecutorServer::toolOutputReceived);
        QSignalSpy result(&server, &execution::ExecutorServer::toolResultReceived);
        QTemporaryDir logs;
        QVERIFY(logs.isValid());

        QProcess process;
        process.setProgram(QStringLiteral(TWEAKOPEDIA_TEST_EXECUTOR));
        process.setArguments({u"--server"_s, serverName, u"--nonce"_s, nonce,
                              u"--test-tool-mode"_s,
                              u"--test-tool-root"_s, logs.path()});
        process.start();
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        server.setExpectedProcessId(process.processId());
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 5000);

        const execution::SystemToolRequest request{
            QUuid::createUuid(), execution::SystemToolKind::ComponentStore,
            execution::SystemToolOperation::Scan, {}};
        QVERIFY(server.sendToolRequest(request));
        QTRY_COMPARE_WITH_TIMEOUT(stages.size(), 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(output.size() >= 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 5000);
        const auto typed = qvariant_cast<execution::SystemToolResult>(result.first().first());
        QCOMPARE(typed.status, execution::SystemToolResultStatus::Completed);
        QCOMPARE(typed.outcome, execution::SystemToolOutcome::Healthy);
        QVERIFY(QFileInfo::exists(typed.logPath));
        if (process.state() != QProcess::NotRunning) {
            QVERIFY(process.waitForFinished(5000));
        }
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    void runsOneShotFileSystemCheckExecutorSession()
    {
        const auto serverName = u"tweakopedia-chkdsk-e2e-"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        QVERIFY(server.listen(serverName, nonce));
        QSignalSpy authenticated(&server, &execution::ExecutorServer::authenticated);
        QSignalSpy stages(&server, &execution::ExecutorServer::toolStageReceived);
        QSignalSpy output(&server, &execution::ExecutorServer::toolOutputReceived);
        QSignalSpy result(&server, &execution::ExecutorServer::toolResultReceived);
        QTemporaryDir logs;
        QVERIFY(logs.isValid());

        QProcess process;
        process.setProgram(QStringLiteral(TWEAKOPEDIA_TEST_EXECUTOR));
        process.setArguments({u"--server"_s, serverName, u"--nonce"_s, nonce,
                              u"--test-tool-mode"_s,
                              u"--test-tool-root"_s, logs.path()});
        process.start();
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        server.setExpectedProcessId(process.processId());
        QTRY_COMPARE_WITH_TIMEOUT(authenticated.size(), 1, 5000);

        const execution::SystemToolRequest request{
            QUuid::createUuid(), execution::SystemToolKind::FileSystem,
            execution::SystemToolOperation::Check, u"C:"_s};
        QVERIFY(server.sendToolRequest(request));
        QTRY_COMPARE_WITH_TIMEOUT(stages.size(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!output.isEmpty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 5000);
        const auto typed = qvariant_cast<execution::SystemToolResult>(result.first().first());
        QCOMPARE(typed.status, execution::SystemToolResultStatus::Completed);
        QCOMPARE(typed.outcome, execution::SystemToolOutcome::Healthy);
        QCOMPARE(typed.exitCode, 0);
        QVERIFY(QFileInfo::exists(typed.logPath));
        QVERIFY(typed.logPath.contains(u"Tools/FileSystemCheck"_s));
        if (process.state() != QProcess::NotRunning) {
            QVERIFY(process.waitForFinished(5000));
        }
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }
};

QTEST_GUILESS_MAIN(ExecutorIpcTest)

#include "ExecutorIpcTest.moc"
