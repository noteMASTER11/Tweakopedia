#include "content/TweakCatalogLoader.h"
#include "detection/RegistryDwordStateDetector.h"
#include "execution/RegistryDwordExecutor.h"
#include "execution/RegistrySnapshot.h"
#include "execution/ExecutorLauncher.h"
#include "execution/TransactionRunner.h"
#include "fakes/FakeRegistryBackend.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "planning/PlanBuilder.h"
#include "planning/TweakQueue.h"

#include <QJsonArray>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11, .build = 26200, .ubr = 1000,
            .edition = u"Professional"_s, .architecture = domain::CpuArchitecture::X64};
}

struct Fixture {
    QTemporaryDir root;
    persistence::Database database;
    std::unique_ptr<persistence::TransactionRepository> repository;
    std::unique_ptr<persistence::TransactionFiles> files;

    Fixture()
    {
        Q_ASSERT(database.open(root.filePath(u"tweakopedia.db"_s)));
        repository = std::make_unique<persistence::TransactionRepository>(database);
        files = std::make_unique<persistence::TransactionFiles>(root.filePath(u"transactions"_s));
    }
};

class CancelledShellBackend final : public execution::IShellExecuteBackend
{
public:
    execution::ShellStartResult start(const execution::ShellLaunchRequest&) override
    {
        return {.status = execution::ShellStartStatus::Cancelled};
    }
    execution::ShellWaitStatus wait(quintptr, int) override
    {
        return execution::ShellWaitStatus::Failed;
    }
    bool terminate(quintptr) override { return true; }
    void close(quintptr) override {}
};

} // namespace

class VerticalSliceWorkflowTest final : public QObject
{
    Q_OBJECT

private slots:
    void appliesAndRollsBackAbsentDword()
    {
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        QVERIFY(loaded.catalog.has_value());
        const auto& tweak = loaded.catalog->tweaks().first();
        tests::FakeRegistryBackend backend;
        const auto detected = detection::RegistryDwordStateDetector{}.detect(tweak, backend, profile());
        QCOMPARE(detected.stateId, u"disabled"_s);

        planning::TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled"_s).accepted);
        const auto built = planning::PlanBuilder{}.build(
            *loaded.catalog, queue, {{tweak.id, detected}}, profile());
        QVERIFY(built.plan.has_value());
        QCOMPARE(built.plan->operations.size(), 1);

        Fixture fixture;
        QVERIFY(fixture.files->create(built.plan->transactionId));
        QVERIFY(fixture.repository->insert(persistence::TransactionRecord::pending(
            built.plan->transactionId, u"Сквозной тест"_s,
            fixture.files->directory(built.plan->transactionId))));
        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto applied = runner.run(*built.plan);
        QVERIFY(applied.success);

        const auto location = tweak.detection->location;
        const auto current = backend.read(location);
        QCOMPARE(current.type, platform::RegistryValueType::Dword);
        QCOMPARE(qFromLittleEndian<quint32>(current.rawValue.constData()), quint32{1});
        QCOMPARE(fixture.repository->find(built.plan->transactionId)->status,
                 persistence::TransactionStatus::Succeeded);

        const auto before = fixture.files->readBefore(built.plan->transactionId);
        QVERIFY(before.has_value());
        const auto snapshots = before->value(u"operations"_s).toArray();
        QCOMPARE(snapshots.size(), 1);
        const auto snapshot = execution::RegistrySnapshot::fromJson(snapshots.first().toObject());
        QVERIFY(snapshot.has_value());
        execution::RegistryDwordExecutor executor(backend);
        QVERIFY(executor.restore(*snapshot).success);
        QVERIFY(fixture.repository->updateStatus(
            built.plan->transactionId, persistence::TransactionStatus::RolledBack));
        QCOMPARE(backend.read(location).presence, platform::RegistryPresence::Missing);
    }

    void stalePreviewStopsBeforeWrite()
    {
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        const auto& tweak = loaded.catalog->tweaks().first();
        tests::FakeRegistryBackend backend;
        const auto detected = detection::RegistryDwordStateDetector{}.detect(tweak, backend, profile());
        planning::TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled"_s).accepted);
        const auto plan = planning::PlanBuilder{}.build(
            *loaded.catalog, queue, {{tweak.id, detected}}, profile()).plan.value();
        backend.setReadResult(tweak.detection->location, platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, QByteArray::fromHex("07000000"), 4));
        Fixture fixture;
        QVERIFY(fixture.files->create(plan.transactionId));
        QVERIFY(fixture.repository->insert(persistence::TransactionRecord::pending(
            plan.transactionId, u"Гонка"_s, fixture.files->directory(plan.transactionId))));

        const auto result = execution::TransactionRunner(
            backend, *fixture.files, *fixture.repository).run(plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"state.changed"_s);
        QCOMPARE(qFromLittleEndian<quint32>(
                     backend.read(tweak.detection->location).rawValue.constData()), quint32{7});
    }

    void marksRunningTransactionInterruptedOnNextStartup()
    {
        Fixture fixture;
        const auto id = QUuid::createUuid();
        auto record = persistence::TransactionRecord::pending(
            id, u"Прерванный пакет"_s, fixture.root.filePath(u"transaction"_s));
        record.status = persistence::TransactionStatus::Running;
        QVERIFY(fixture.repository->insert(record));
        QVERIFY(fixture.files->create(id));
        QVERIFY(fixture.files->writeBefore(id, QJsonObject{{u"operations"_s, QJsonArray{}}}));
        QVERIFY(!fixture.files->readResult(id).has_value());

        QCOMPARE(fixture.repository->markRunningAsInterrupted(), 1);

        QCOMPARE(fixture.repository->find(id)->status, persistence::TransactionStatus::Interrupted);
    }

    void uacCancellationIsNotSuccess()
    {
        CancelledShellBackend backend;
        execution::ExecutorLauncher launcher(backend);

        const auto result = launcher.start(u"executor.exe"_s, u"server"_s, u"nonce"_s);

        QVERIFY(!result.started);
        QCOMPARE(result.code, u"launch.cancelled"_s);
    }

    void failedResultRemainsFailedInHistory()
    {
        Fixture fixture;
        const auto id = QUuid::createUuid();
        auto record = persistence::TransactionRecord::pending(
            id, u"Аварийный результат"_s, fixture.files->directory(id));
        QVERIFY(fixture.repository->insert(record));
        QVERIFY(fixture.files->create(id));
        QVERIFY(fixture.repository->updateStatus(
            id, persistence::TransactionStatus::Failed, u"executor.crashed"_s));
        QVERIFY(fixture.files->writeResult(id, {
            {u"status"_s, u"failed"_s},
            {u"code"_s, u"executor.crashed"_s},
        }));

        const auto records = fixture.repository->list();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().status, persistence::TransactionStatus::Failed);
        QVERIFY(records.first().status != persistence::TransactionStatus::Succeeded);
    }
};

QTEST_GUILESS_MAIN(VerticalSliceWorkflowTest)

#include "VerticalSliceWorkflowTest.moc"
