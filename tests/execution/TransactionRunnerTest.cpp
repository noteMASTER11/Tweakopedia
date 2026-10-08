#include "execution/RegistrySnapshot.h"
#include "execution/TransactionRunner.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRecord.h"
#include "persistence/TransactionRepository.h"

#include <QHash>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

QString keyFor(const domain::RegistryLocation& location)
{
    return location.key + u'|' + location.valueName;
}

QByteArray dwordBytes(quint32 value)
{
    QByteArray bytes(sizeof(value), Qt::Uninitialized);
    qToLittleEndian(value, bytes.data());
    return bytes;
}

class ScriptedBackend final : public platform::IRegistryBackend
{
public:
    QHash<QString, platform::RegistryReadResult> values;
    QStringList writes;
    int failWriteNumber{};
    int writeCount{};
    bool corruptDwordWrite{};

    platform::RegistryReadResult read(const domain::RegistryLocation& location) const override
    {
        return values.value(keyFor(location), platform::RegistryReadResult::missing());
    }

    platform::RegistryWriteResult writeDword(
        const domain::RegistryLocation& location,
        quint32 value) override
    {
        ++writeCount;
        writes.append(u"dword:"_s + location.valueName);
        if (writeCount == failWriteNumber) {
            return platform::RegistryWriteResult::failed(
                std::make_error_code(std::errc::permission_denied));
        }
        const auto stored = corruptDwordWrite ? value + 1 : value;
        values.insert(keyFor(location), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(stored), 4));
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& bytes) override
    {
        writes.append(u"raw:"_s + location.valueName);
        const auto type = nativeType == 4
            ? platform::RegistryValueType::Dword
            : platform::RegistryValueType::Unknown;
        values.insert(keyFor(location), platform::RegistryReadResult::present(
            type, bytes, nativeType));
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult deleteValue(const domain::RegistryLocation& location) override
    {
        writes.append(u"delete:"_s + location.valueName);
        values.insert(keyFor(location), platform::RegistryReadResult::missing());
        return platform::RegistryWriteResult::succeeded();
    }
};

domain::RegistryLocation location(QString name)
{
    return {
        .hive = domain::RegistryHive::CurrentUser,
        .key = u"Software\\Tweakopedia\\RunnerTest"_s,
        .valueName = std::move(name),
        .view = domain::RegistryView::Registry64,
    };
}

planning::PlannedRegistryDwordChange change(
    const domain::RegistryLocation& target,
    quint32 value,
    const platform::RegistryReadResult& before)
{
    return {
        .tweakId = domain::TweakId::parse(u"filesystem.win32-long-paths"_s).value(),
        .targetState = value == 0 ? u"disabled"_s : u"enabled"_s,
        .change = {.location = target, .value = value},
        .beforeFingerprint = execution::RegistrySnapshot::fingerprint(before),
    };
}

struct Fixture {
    QTemporaryDir root;
    persistence::Database database;
    std::unique_ptr<persistence::TransactionRepository> repository;
    std::unique_ptr<persistence::TransactionFiles> files;
    planning::ExecutionPlan plan;

    Fixture()
    {
        Q_ASSERT(root.isValid());
        Q_ASSERT(database.open(root.filePath(u"history.db"_s)));
        repository = std::make_unique<persistence::TransactionRepository>(database);
        files = std::make_unique<persistence::TransactionFiles>(root.filePath(u"transactions"_s));
        plan.transactionId = QUuid::createUuid();
        plan.createdAtUtc = QDateTime::currentDateTimeUtc();
        Q_ASSERT(files->create(plan.transactionId));
        Q_ASSERT(repository->insert(persistence::TransactionRecord::pending(
            plan.transactionId, u"Test"_s, files->directory(plan.transactionId))));
    }
};

} // namespace

class TransactionRunnerTest final : public QObject
{
    Q_OBJECT

private slots:
    void stopsWhenBeforeFingerprintChanged()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto target = location(u"First"_s);
        const auto preview = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        fixture.plan.operations.append(change(target, 1, preview));
        backend.values.insert(keyFor(target), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(7), 4));

        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"state.changed"_s);
        QVERIFY(backend.writes.isEmpty());
        QCOMPARE(fixture.repository->find(fixture.plan.transactionId)->status,
                 persistence::TransactionStatus::Failed);
    }

    void reportsWriteFailure()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto target = location(u"First"_s);
        const auto before = platform::RegistryReadResult::missing();
        backend.values.insert(keyFor(target), before);
        backend.failWriteNumber = 1;
        fixture.plan.operations.append(change(target, 1, before));

        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"registry.write_failed"_s);
        QCOMPARE(backend.read(target).presence, platform::RegistryPresence::Missing);
    }

    void rollsBackFirstWhenSecondBeforeFingerprintChanged()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto first = location(u"First"_s);
        const auto second = location(u"Second"_s);
        const auto zero = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        backend.values.insert(keyFor(first), zero);
        backend.values.insert(keyFor(second), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(7), 4));
        fixture.plan.operations.append(change(first, 1, zero));
        fixture.plan.operations.append(change(second, 1, zero));

        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(result.code, u"state.changed"_s);
        QCOMPARE(backend.read(first).rawValue, zero.rawValue);
        QCOMPARE(backend.writes, QStringList({u"dword:First"_s, u"raw:First"_s}));
    }

    void reportsVerificationMismatchAndRestoresSnapshot()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto target = location(u"First"_s);
        const auto before = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        backend.values.insert(keyFor(target), before);
        backend.corruptDwordWrite = true;
        fixture.plan.operations.append(change(target, 1, before));

        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"registry.verify_failed"_s);
        QCOMPARE(backend.read(target).rawValue, before.rawValue);
    }

    void rollsBackConfirmedOperationsInReverseOrder()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto first = location(u"First"_s);
        const auto second = location(u"Second"_s);
        const auto zero = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        backend.values.insert(keyFor(first), zero);
        backend.values.insert(keyFor(second), zero);
        backend.failWriteNumber = 2;
        fixture.plan.operations.append(change(first, 1, zero));
        fixture.plan.operations.append(change(second, 1, zero));

        execution::TransactionRunner runner(backend, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(backend.read(first).rawValue, zero.rawValue);
        QCOMPARE(backend.read(second).rawValue, zero.rawValue);
        QCOMPARE(backend.writes,
                 QStringList({u"dword:First"_s, u"dword:Second"_s,
                              u"raw:Second"_s, u"raw:First"_s}));
        QCOMPARE(fixture.repository->find(fixture.plan.transactionId)->status,
                 persistence::TransactionStatus::RolledBack);

        const auto before = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(before.has_value());
        QCOMPARE(before->value(u"operations"_s).toArray().size(), 2);
        const auto persisted = fixture.files->readResult(fixture.plan.transactionId);
        QVERIFY(persisted.has_value());
        QCOMPARE(persisted->value(u"status"_s).toString(), u"rolled_back"_s);
    }
};

QTEST_GUILESS_MAIN(TransactionRunnerTest)

#include "TransactionRunnerTest.moc"
