#include "content/TweakCatalogLoader.h"
#include "detection/RegistryDwordStateDetector.h"
#include "detection/RegistryValueStateDetector.h"
#include "execution/FileOperationExecutor.h"
#include "execution/RegistryDwordExecutor.h"
#include "execution/RegistryValueExecutor.h"
#include "execution/RegistrySnapshot.h"
#include "execution/ExecutorLauncher.h"
#include "execution/TransactionRunner.h"
#include "fakes/FakeRegistryBackend.h"
#include "persistence/Database.h"
#include "persistence/PendingInputStore.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "planning/PlanBuilder.h"
#include "planning/TweakQueue.h"

#include <QJsonArray>
#include <QFile>
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
        const auto longPathsId = *domain::TweakId::parse(u"filesystem.win32-long-paths");
        const auto* loadedTweak = loaded.catalog->find(longPathsId);
        QVERIFY(loadedTweak != nullptr);
        const auto& tweak = *loadedTweak;
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
        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
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
        QVERIFY(loaded.catalog.has_value());
        const auto longPathsId = *domain::TweakId::parse(u"filesystem.win32-long-paths");
        const auto* loadedTweak = loaded.catalog->find(longPathsId);
        QVERIFY(loadedTweak != nullptr);
        const auto& tweak = *loadedTweak;
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
            {.registry = &backend}, *fixture.files, *fixture.repository).run(plan);

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

    void appliesAndRestoresOemTextAndLogoFromMissingState()
    {
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        QVERIFY(loaded.catalog.has_value());
        auto manufacturer = *loaded.catalog->find(
            *domain::TweakId::parse(u"devices.oem-manufacturer"_s));
        auto logo = *loaded.catalog->find(*domain::TweakId::parse(u"devices.oem-logo"_s));

        Fixture fixture;
        const auto logoDestination = QDir::cleanPath(
            fixture.root.filePath(u"system32/oemlogo.bmp"_s));
        for (auto& state : logo.states) {
            for (auto& operation : state.operations) {
                if (auto* file = std::get_if<domain::FileOperationDefinition>(&operation)) {
                    file->destination = logoDestination;
                }
            }
        }
        const content::TweakCatalog catalog({manufacturer, logo});
        tests::FakeRegistryBackend backend;
        const auto manufacturerState = detection::RegistryValueStateDetector{}.detect(
            manufacturer, backend, profile());
        const auto logoState = detection::RegistryValueStateDetector{}.detect(
            logo, backend, profile());
        QCOMPARE(manufacturerState.stateId, u"cleared"_s);
        QCOMPARE(logoState.stateId, u"cleared"_s);

        const auto source = fixture.root.filePath(u"source/logo.bmp"_s);
        QVERIFY(QDir{}.mkpath(QFileInfo(source).absolutePath()));
        QFile sourceFile(source);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        const QByteArray logoBytes{"BM-Tweakopedia-OEM"};
        QCOMPARE(sourceFile.write(logoBytes), logoBytes.size());
        sourceFile.close();

        planning::TweakQueue queue;
        QVERIFY(queue.setTarget(
            manufacturer, u"configured"_s,
            {{u"manufacturer"_s, u"Tweakopedia Labs"_s}}).accepted);
        QVERIFY(queue.setTarget(
            logo, u"configured"_s, {{u"logo"_s, source}}).accepted);
        auto plan = planning::PlanBuilder{}.build(
            catalog, queue,
            {{manufacturer.id, manufacturerState}, {logo.id, logoState}}, profile());
        QVERIFY2(plan.plan.has_value(), qPrintable(
            plan.issues.isEmpty() ? QString{} : plan.issues.first().code));
        QCOMPARE(plan.plan->operations.size(), 3);

        QVERIFY(fixture.files->create(plan.plan->transactionId));
        persistence::PendingInputStore pending(fixture.root.filePath(u"pending"_s));
        for (auto& operation : plan.plan->operations) {
            auto* file = std::get_if<planning::PlannedFileChange>(&operation);
            if (!file) continue;
            const auto imported = pending.importFile(
                file->change.artifact.id, file->change.artifact.managedPath,
                file->allowedExtensions, file->maximumInputSize);
            QVERIFY(imported.artifact.has_value());
            QCOMPARE(imported.artifact->size, static_cast<quint64>(logoBytes.size()));
            QVERIFY(!imported.artifact->sha256.isEmpty());
            const auto materialized = fixture.files->materializeInput(
                plan.plan->transactionId, *imported.artifact);
            QVERIFY(materialized.artifact.has_value());
            QCOMPARE(materialized.artifact->sha256, imported.artifact->sha256);
            file->change.artifact = *materialized.artifact;
        }
        QVERIFY(fixture.repository->insert(persistence::TransactionRecord::pending(
            plan.plan->transactionId, u"OEM-сведения"_s,
            fixture.files->directory(plan.plan->transactionId))));

        const auto applied = execution::TransactionRunner(
            {.registry = &backend}, *fixture.files, *fixture.repository).run(*plan.plan);
        QVERIFY(applied.success);
        QVERIFY(QFileInfo::exists(logoDestination));
        QCOMPARE(backend.read(manufacturer.valueDetection->location).nativeType, quint32{1});
        QCOMPARE(backend.read(logo.valueDetection->location).nativeType, quint32{1});

        const auto before = fixture.files->readBefore(plan.plan->transactionId);
        QVERIFY(before.has_value());
        const auto snapshots = before->value(u"operations"_s).toArray();
        QCOMPARE(snapshots.size(), 3);
        execution::RegistryValueExecutor registryExecutor(backend);
        execution::FileOperationExecutor fileExecutor(
            fixture.files->directory(plan.plan->transactionId));
        for (qsizetype index = snapshots.size() - 1; index >= 0; --index) {
            const auto object = snapshots[index].toObject();
            if (object.value(u"snapshotType"_s) == u"registry.value"_s) {
                const auto snapshot = execution::RegistrySnapshot::fromJson(object);
                QVERIFY(snapshot.has_value());
                QVERIFY(registryExecutor.restore(*snapshot).success);
            } else {
                const auto snapshot = domain::FileSnapshot::fromJson(object);
                QVERIFY(snapshot.has_value());
                QVERIFY(fileExecutor.restore(*snapshot).success);
            }
        }
        QCOMPARE(backend.read(manufacturer.valueDetection->location).presence,
                 platform::RegistryPresence::Missing);
        QCOMPARE(backend.read(logo.valueDetection->location).presence,
                 platform::RegistryPresence::Missing);
        QVERIFY(!QFileInfo::exists(logoDestination));
    }
};

QTEST_GUILESS_MAIN(VerticalSliceWorkflowTest)

#include "VerticalSliceWorkflowTest.moc"
