#include "execution/ExecutionProtocol.h"
#include "execution/ExecutorClient.h"
#include "execution/ExecutorServer.h"
#include "execution/RegistrySnapshot.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

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
        .operations = {planning::PlannedRegistryDwordChange{
            .tweakId = domain::TweakId::parse(u"filesystem.win32-long-paths"_s).value(),
            .targetState = u"enabled"_s,
            .change = {
                .location = {
                    .hive = domain::RegistryHive::LocalMachine,
                    .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
                    .valueName = u"LongPathsEnabled"_s,
                    .view = domain::RegistryView::Registry64,
                },
                .value = 1,
            },
            .beforeFingerprint = execution::RegistrySnapshot::fingerprint(
                platform::RegistryReadResult::missing()),
        }},
        .summary = u"Одна операция"_s,
    };
}

} // namespace

class ExecutorIpcTest final : public QObject
{
    Q_OBJECT

private slots:
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
        const auto plan = samplePlan();
        const auto transactionDirectory = QDir(dataRoot.path()).filePath(
            u"transactions/"_s + plan.transactionId.toString(QUuid::WithoutBraces));
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
        QCOMPARE(before->value(u"operations"_s).toArray().size(), 1);
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

        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }
};

QTEST_GUILESS_MAIN(ExecutorIpcTest)

#include "ExecutorIpcTest.moc"
