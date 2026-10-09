#include "execution/AppxPackageExecutor.h"
#include "execution/FeatureStateExecutor.h"
#include "execution/FileOperationExecutor.h"
#include "execution/RegistrySnapshot.h"
#include "execution/RegistryTreeExecutor.h"
#include "execution/ScheduledTaskExecutor.h"
#include "execution/BcdElementExecutor.h"
#include "execution/PowerSettingExecutor.h"
#include "execution/WindowsComponentExecutor.h"
#include "execution/TransactionRunner.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRecord.h"
#include "persistence/TransactionRepository.h"

#include <QHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
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
    QHash<QString, domain::RegistryTreeSnapshot> trees;
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

    platform::RegistryWriteResult writeValue(
        const domain::RegistryLocation& location,
        const domain::RegistryValueSpec& value) override
    {
        ++writeCount;
        writes.append(u"value:"_s + location.valueName);
        if (writeCount == failWriteNumber) {
            return platform::RegistryWriteResult::failed(
                std::make_error_code(std::errc::permission_denied));
        }
        auto type = platform::RegistryValueType::Unknown;
        if (value.nativeType == 1) type = platform::RegistryValueType::String;
        else if (value.nativeType == 2) type = platform::RegistryValueType::ExpandString;
        else if (value.nativeType == 3) type = platform::RegistryValueType::Binary;
        else if (value.nativeType == 4) type = platform::RegistryValueType::Dword;
        else if (value.nativeType == 7) type = platform::RegistryValueType::MultiString;
        else if (value.nativeType == 11) type = platform::RegistryValueType::Qword;
        values.insert(keyFor(location), platform::RegistryReadResult::present(
            type, value.rawValue, value.nativeType));
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& bytes) override
    {
        writes.append(u"raw:"_s + location.valueName);
        auto type = platform::RegistryValueType::Unknown;
        if (nativeType == 1) type = platform::RegistryValueType::String;
        else if (nativeType == 2) type = platform::RegistryValueType::ExpandString;
        else if (nativeType == 3) type = platform::RegistryValueType::Binary;
        else if (nativeType == 4) type = platform::RegistryValueType::Dword;
        else if (nativeType == 7) type = platform::RegistryValueType::MultiString;
        else if (nativeType == 11) type = platform::RegistryValueType::Qword;
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

    platform::RegistryTreeReadResult readTree(
        const domain::RegistryKeyLocation& location,
        const domain::RegistryTreeLimits&) const override
    {
        return platform::RegistryTreeReadResult::succeeded(
            trees.value(location.key, domain::RegistryTreeSnapshot{
                .location = location, .existed = false}));
    }

    platform::RegistryWriteResult createKey(const domain::RegistryKeyLocation& location) override
    {
        writes.append(u"create-tree:"_s + location.key);
        trees.insert(location.key, {.location = location, .existed = true,
                                    .nodes = {{.relativePath = {}}}});
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult deleteTree(const domain::RegistryKeyLocation& location) override
    {
        writes.append(u"delete-tree:"_s + location.key);
        trees.insert(location.key, {.location = location, .existed = false});
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult restoreTree(
        const domain::RegistryTreeSnapshot& snapshot) override
    {
        writes.append(u"restore-tree:"_s + snapshot.location.key);
        trees.insert(snapshot.location.key, snapshot);
        return platform::RegistryWriteResult::succeeded();
    }
};

class ScriptedAppxBackend final : public platform::IAppxPackageBackend
{
public:
    platform::AppxPackageQueryResult installedForCurrentUser() const override
    {
        return {.packages = packages};
    }

    platform::AppxPackageMutationResult removeCurrentUser(const QString& fullName) override
    {
        removed.append(fullName);
        for (qsizetype index = packages.size() - 1; index >= 0; --index) {
            if (packages.at(index).fullName == fullName) packages.removeAt(index);
        }
        return {.success = true};
    }

    mutable QVector<platform::AppxPackageIdentity> packages;
    QStringList removed;
};

class ScriptedFeatureBackend final : public platform::IFeatureStoreBackend
{
public:
    platform::FeatureConfiguration current{.featureId = 42592269, .priority = 4,
        .state = domain::FeatureEnabledState::Disabled};
    platform::FeatureQueryResult query(quint32 featureId) const override
    {
        return featureId == current.featureId
            ? platform::FeatureQueryResult{.success = true, .configuration = current}
            : platform::FeatureQueryResult{.success = false};
    }
    platform::FeatureMutationResult setUserState(
        quint32 featureId, domain::FeatureEnabledState state) override
    {
        current.featureId = featureId;
        current.priority = 8;
        current.state = state;
        return {.success = true};
    }
    platform::FeatureMutationResult setUserConfiguration(
        const platform::FeatureConfiguration& configuration) override
    {
        current = configuration;
        return {.success = true};
    }
    platform::FeatureMutationResult resetUserConfiguration(quint32) override
    {
        current.priority = 4;
        current.state = domain::FeatureEnabledState::Disabled;
        return {.success = true};
    }
};

class ScriptedScheduledTaskBackend final : public platform::IScheduledTaskBackend
{
public:
    bool enabled{true};
    QStringList mutations;

    platform::ScheduledTaskReadResult read(
        const domain::ScheduledTaskLocation&) override
    {
        return platform::ScheduledTaskReadResult::present(enabled);
    }

    platform::ScheduledTaskWriteResult setEnabled(
        const domain::ScheduledTaskLocation&, bool value) override
    {
        enabled = value;
        mutations.append(value ? u"enabled"_s : u"disabled"_s);
        return {.success = true};
    }
};

class ScriptedBcdBackend final : public platform::IBcdBackend
{
public:
    platform::BcdReadResult current{platform::BcdReadResult::missingElement()};
    QStringList mutations;
    platform::BcdReadResult read(const domain::BcdElementSpec&) const override { return current; }
    platform::BcdMutationResult set(
        const domain::BcdElementSpec&, const domain::BcdValue& value) override
    {
        current = platform::BcdReadResult::present(value);
        mutations.append(u"set"_s);
        return {.success = true};
    }
    platform::BcdMutationResult remove(const domain::BcdElementSpec&) override
    {
        current = platform::BcdReadResult::missingElement();
        mutations.append(u"remove"_s);
        return {.success = true};
    }
};

class ScriptedPowerBackend final : public platform::IPowerSettingBackend
{
public:
    quint32 index{7};
    bool failNextWrite{};
    QStringList mutations;
    platform::PowerSettingReadResult read(const domain::PowerSettingLocation&) const override
    { return platform::PowerSettingReadResult::present(index, u"381b4222-f694-41f0-9685-ff5bb260df2e"_s); }
    platform::PowerSettingMutationResult write(
        const domain::PowerSettingLocation&, quint32 value) override
    {
        mutations.append(u"write:%1"_s.arg(value));
        if (failNextWrite) {
            failNextWrite = false;
            return {.error = u"planned failure"_s};
        }
        index = value;
        return {.success = true};
    }
    platform::PowerSettingMutationResult activateScheme(QStringView scheme) override
    { mutations.append(u"activate:"_s + scheme.toString()); return {.success = true}; }
};

class ScriptedWindowsComponentBackend final : public platform::IWindowsComponentBackend
{
public:
    domain::WindowsComponentState state{domain::WindowsComponentState::Disabled};
    QStringList mutations;
    platform::WindowsComponentQueryResult query(
        const domain::WindowsComponentTarget&) const override
    { return platform::WindowsComponentQueryResult::present(state); }
    platform::WindowsComponentMutationResult setState(
        const domain::WindowsComponentTarget&,
        domain::WindowsComponentState value) override
    {
        state = value;
        mutations.append(domain::windowsComponentStateName(value));
        return {.success = true};
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

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
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

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
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

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(result.code, u"state.changed"_s);
        QCOMPARE(backend.read(first).rawValue, zero.rawValue);
        QCOMPARE(backend.writes, QStringList({u"dword:First"_s, u"raw:First"_s}));
    }

    void rollsBackGenericRegistryValueWhenLaterFingerprintChanged()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto first = location(u"GenericFirst"_s);
        const auto second = location(u"GenericSecond"_s);
        const auto original = platform::RegistryReadResult::present(
            platform::RegistryValueType::String,
            domain::RegistryValueSpec::string(u"old").rawValue, 1);
        backend.values.insert(keyFor(first), original);
        backend.values.insert(keyFor(second), platform::RegistryReadResult::present(
            platform::RegistryValueType::String,
            domain::RegistryValueSpec::string(u"changed").rawValue, 1));
        fixture.plan.operations.append(planning::PlannedRegistryValueChange{
            .tweakId = *domain::TweakId::parse(u"devices.generic-first"_s),
            .targetState = u"enabled"_s,
            .change = domain::SetRegistryValueOperation{
                first, domain::RegistryValueSpec::qword(42)},
            .beforeFingerprint = execution::RegistrySnapshot::fingerprint(original),
        });
        fixture.plan.operations.append(planning::PlannedRegistryValueChange{
            .tweakId = *domain::TweakId::parse(u"devices.generic-second"_s),
            .targetState = u"enabled"_s,
            .change = domain::DeleteRegistryValueOperation{second},
            .beforeFingerprint = execution::RegistrySnapshot::fingerprint(original),
        });

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(result.code, u"state.changed"_s);
        QCOMPARE(backend.read(first).nativeType, quint32{1});
        QCOMPARE(backend.read(first).rawValue, original.rawValue);
        QCOMPARE(backend.writes, QStringList({u"value:GenericFirst"_s, u"raw:GenericFirst"_s}));
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

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"registry.verify_failed"_s);
        QCOMPARE(backend.read(target).rawValue, before.rawValue);
    }

    void restoresDeletedRegistryTreeWhenLaterOperationIsStale()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const domain::RegistryKeyLocation treeLocation{
            .hive = domain::RegistryHive::CurrentUser,
            .key = u"Software\\Tweakopedia\\RunnerTree"_s,
            .view = domain::RegistryView::Registry64,
        };
        const domain::RegistryTreeSnapshot tree{
            .location = treeLocation,
            .existed = true,
            .nodes = {
                {.relativePath = {}, .values = {{u"Text"_s, domain::RegistryValueSpec::string(u"root")}}},
                {.relativePath = u"Child"_s, .values = {{u"Data"_s, domain::RegistryValueSpec::binary("data")}}},
            },
        };
        backend.trees.insert(treeLocation.key, tree);
        const auto staleLocation = location(u"StaleAfterTree"_s);
        const auto expected = platform::RegistryReadResult::missing();
        backend.values.insert(keyFor(staleLocation), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(1), 4));
        fixture.plan.operations.append(planning::PlannedRegistryTreeChange{
            .tweakId = *domain::TweakId::parse(u"devices.tree-delete"_s),
            .targetState = u"absent"_s,
            .change = domain::DeleteRegistryTreeOperation{treeLocation},
            .beforeFingerprint = execution::RegistryTreeExecutor::fingerprint(tree),
        });
        fixture.plan.operations.append(change(staleLocation, 1, expected));

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(backend.trees.value(treeLocation.key), tree);
        QCOMPARE(backend.writes, QStringList({
            u"delete-tree:Software\\Tweakopedia\\RunnerTree"_s,
            u"restore-tree:Software\\Tweakopedia\\RunnerTree"_s,
        }));
    }

    void restoresReplacedFileWhenLaterOperationIsStale()
    {
        Fixture fixture;
        ScriptedBackend backend;
        const auto destination = fixture.root.filePath(u"target/logo.bmp"_s);
        const auto inputRelative = u"inputs/artifact-1/logo.bmp"_s;
        const auto input = QDir(fixture.files->directory(fixture.plan.transactionId))
                               .filePath(inputRelative);
        QVERIFY(QDir{}.mkpath(QFileInfo(destination).absolutePath()));
        QVERIFY(QDir{}.mkpath(QFileInfo(input).absolutePath()));
        QFile oldFile(destination);
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        QCOMPARE(oldFile.write("old"), 3);
        oldFile.close();
        QFile newFile(input);
        QVERIFY(newFile.open(QIODevice::WriteOnly));
        QCOMPARE(newFile.write("new"), 3);
        newFile.close();
        const auto oldHash = QCryptographicHash::hash("old", QCryptographicHash::Sha256).toHex();
        const auto newHash = QCryptographicHash::hash("new", QCryptographicHash::Sha256).toHex();
        const domain::FileSnapshot before{
            .destination = destination, .existed = true, .size = 3, .sha256 = oldHash};
        fixture.plan.operations.append(planning::PlannedFileChange{
            .tweakId = *domain::TweakId::parse(u"devices.oem-logo"_s),
            .targetState = u"configured"_s,
            .change = domain::FileOperation{
                .kind = domain::FileOperationKind::Replace,
                .artifact = {.id = u"logo"_s, .storageId = u"artifact-1"_s,
                             .managedPath = inputRelative, .size = 3, .sha256 = newHash},
                .destination = destination,
            },
            .beforeFingerprint = execution::FileOperationExecutor::fingerprint(before),
        });
        const auto staleLocation = location(u"StaleAfterFile"_s);
        const auto expected = platform::RegistryReadResult::missing();
        backend.values.insert(keyFor(staleLocation), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(1), 4));
        fixture.plan.operations.append(change(staleLocation, 1, expected));

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QFile restored(destination);
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), QByteArray("old"));
    }

    void removesAppxPackageAndPersistsExactBeforeSnapshot()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedAppxBackend appx;
        appx.packages = {{
            u"Clipchamp.Clipchamp"_s,
            u"Clipchamp.Clipchamp_3.0_x64__abc"_s,
        }};
        execution::AppxPackageExecutor appxExecutor(appx);
        const auto captured = appxExecutor.capture(u"Clipchamp.Clipchamp"_s);
        QVERIFY(captured.success);
        fixture.plan.operations.append(planning::PlannedAppxRemoval{
            .tweakId = domain::TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s).value(),
            .targetState = u"remove"_s,
            .change = domain::RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s},
            .beforeFingerprint = captured.snapshot.fingerprint(),
        });

        execution::TransactionRunner runner(
            {.registry = &registry, .appx = &appx},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(result.success);
        QCOMPARE(appx.removed, QStringList{u"Clipchamp.Clipchamp_3.0_x64__abc"_s});
        const auto before = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(before.has_value());
        const auto operations = before->value(u"operations"_s).toArray();
        QCOMPARE(operations.size(), 1);
        QCOMPARE(operations.first().toObject().value(u"type"_s).toString(), u"appx.packages"_s);
    }

    void appliesFeatureChangeAndPersistsRestorableSnapshot()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedFeatureBackend features;
        execution::FeatureStateExecutor executor(features);
        const auto captured = executor.capture(42592269);
        QVERIFY(captured.success);
        fixture.plan.operations.append(planning::PlannedFeatureStateChange{
            .tweakId = domain::TweakId::parse(u"experimental.end-task"_s).value(),
            .targetState = u"enabled"_s,
            .change = {42592269, domain::FeatureEnabledState::Enabled},
            .beforeFingerprint = captured.snapshot.fingerprint(),
        });

        execution::TransactionRunner runner(
            {.registry = &registry, .featureStore = &features},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(result.success);
        QCOMPARE(features.current.priority, 8U);
        QCOMPARE(features.current.state, domain::FeatureEnabledState::Enabled);
        const auto before = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(before.has_value());
        QCOMPARE(before->value(u"operations"_s).toArray().first().toObject()
                     .value(u"type"_s).toString(), u"feature.configuration"_s);
    }

    void rollsBackScheduledTaskWhenLaterOperationIsStale()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedScheduledTaskBackend tasks;
        const domain::ScheduledTaskLocation taskLocation{
            .folder = u"\\Microsoft\\Windows\\Application Experience"_s,
            .name = u"Microsoft Compatibility Appraiser"_s,
        };
        const domain::ScheduledTaskSnapshot taskBefore{
            .location = taskLocation, .enabled = true};
        fixture.plan.operations.append(planning::PlannedScheduledTaskChange{
            .tweakId = domain::TweakId::parse(u"privacy.disable-telemetry-task"_s).value(),
            .targetState = u"disabled"_s,
            .change = {.location = taskLocation, .enabled = false},
            .beforeFingerprint = execution::ScheduledTaskExecutor::fingerprint(taskBefore),
        });
        const auto staleLocation = location(u"StaleAfterTask"_s);
        const auto actual = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        registry.values.insert(keyFor(staleLocation), actual);
        auto stale = change(staleLocation, 1, actual);
        stale.beforeFingerprint = QByteArray(64, '0');
        fixture.plan.operations.append(stale);

        execution::TransactionRunner runner(
            {.registry = &registry, .scheduledTasks = &tasks},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QVERIFY(tasks.enabled);
        QCOMPARE(tasks.mutations, QStringList({u"disabled"_s, u"enabled"_s}));
        const auto before = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(before.has_value());
        QCOMPARE(before->value(u"operations"_s).toArray().first().toObject()
                     .value(u"type"_s).toString(), u"scheduled_task"_s);
    }

    void rollsBackBcdElementToMissingWhenLaterOperationIsStale()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedBcdBackend bcd;
        const domain::BcdElementSpec spec{
            u"{current}"_s, 0x260000A5, domain::BcdValueKind::Boolean};
        const domain::BcdElementSnapshot before{.spec = spec, .existed = false};
        fixture.plan.operations.append(planning::PlannedBcdElementChange{
            .tweakId = domain::TweakId::parse(u"boot.dynamic-tick"_s).value(),
            .targetState = u"disabled"_s,
            .change = {.spec = spec, .value = domain::BcdValue{true}},
            .beforeFingerprint = execution::BcdElementExecutor::fingerprint(before),
        });
        const auto staleLocation = location(u"StaleAfterBcd"_s);
        const auto actual = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        registry.values.insert(keyFor(staleLocation), actual);
        auto stale = change(staleLocation, 1, actual);
        stale.beforeFingerprint = QByteArray(64, '0');
        fixture.plan.operations.append(stale);

        execution::TransactionRunner runner(
            {.registry = &registry, .bcd = &bcd},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QVERIFY(bcd.current.missing);
        QCOMPARE(bcd.mutations, QStringList({u"set"_s, u"remove"_s}));
        const auto persisted = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(persisted.has_value());
        QCOMPARE(persisted->value(u"operations"_s).toArray().first().toObject()
                     .value(u"type"_s).toString(), u"bcd.element"_s);
    }

    void activatesPowerSchemeOnlyAfterSuccessfulPackage()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedPowerBackend power;
        const domain::PowerSettingLocation powerLocation{
            u"active"_s, u"54533251-82be-4824-96c1-47b60b740d00"_s,
            u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s, domain::PowerSource::Ac};
        const domain::PowerSettingSnapshot before{
            powerLocation, u"381b4222-f694-41f0-9685-ff5bb260df2e"_s, 7};
        fixture.plan.operations.append(planning::PlannedPowerSettingChange{
            .tweakId = domain::TweakId::parse(u"power.processor-minimum-ac"_s).value(),
            .targetState = u"maximum"_s, .change = {powerLocation, 100},
            .beforeFingerprint = execution::PowerSettingExecutor::fingerprint(before)});
        execution::TransactionRunner runner(
            {.registry = &registry, .powerSettings = &power},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);
        QVERIFY(result.success);
        QCOMPARE(power.mutations, QStringList({u"write:100"_s,
            u"activate:381b4222-f694-41f0-9685-ff5bb260df2e"_s}));
    }

    void rollsBackWindowsComponentWhenLaterOperationIsStale()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedWindowsComponentBackend components;
        const domain::WindowsComponentTarget target{
            domain::WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s};
        const domain::WindowsComponentSnapshot before{
            target, domain::WindowsComponentState::Disabled};
        fixture.plan.operations.append(planning::PlannedWindowsComponentChange{
            .tweakId = domain::TweakId::parse(
                u"components.virtual-machine-platform"_s).value(),
            .targetState = u"enabled"_s,
            .change = {target, domain::WindowsComponentState::Enabled},
            .beforeFingerprint = execution::WindowsComponentExecutor::fingerprint(before)});
        const auto staleLocation = location(u"StaleAfterComponent"_s);
        const auto actual = platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4);
        registry.values.insert(keyFor(staleLocation), actual);
        auto stale = change(staleLocation, 1, actual);
        stale.beforeFingerprint = QByteArray(64, '0');
        fixture.plan.operations.append(stale);

        execution::TransactionRunner runner(
            {.registry = &registry, .windowsComponents = &components},
            *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QCOMPARE(components.state, domain::WindowsComponentState::Disabled);
        QCOMPARE(components.mutations, QStringList({u"enabled"_s, u"disabled"_s}));
        const auto persisted = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(persisted.has_value());
        QCOMPARE(persisted->value(u"operations"_s).toArray().first().toObject()
                     .value(u"type"_s).toString(), u"windows_component"_s);
    }

    void rollsBackMixedPackageAndPersistsEverySnapshot()
    {
        Fixture fixture;
        ScriptedBackend registry;
        ScriptedScheduledTaskBackend tasks;
        ScriptedPowerBackend power;

        const auto registryLocation = location(u"MixedValue"_s);
        const auto missing = platform::RegistryReadResult::missing();
        fixture.plan.operations.append(planning::PlannedRegistryValueChange{
            .tweakId = domain::TweakId::parse(u"filesystem.mixed-value"_s).value(),
            .targetState = u"enabled"_s,
            .change = domain::SetRegistryValueOperation{
                registryLocation, domain::RegistryValueSpec::string(u"configured")},
            .beforeFingerprint = execution::RegistrySnapshot::fingerprint(missing)});

        const auto transactionDirectory = fixture.files->directory(fixture.plan.transactionId);
        const auto inputRelative = u"inputs/mixed/new.bin"_s;
        const auto input = QDir(transactionDirectory).filePath(inputRelative);
        QVERIFY(QDir().mkpath(QFileInfo(input).absolutePath()));
        const auto destination = fixture.root.filePath(u"mixed-destination.bin"_s);
        QFile oldFile(destination);
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        QCOMPARE(oldFile.write("old"), 3);
        oldFile.close();
        QFile newFile(input);
        QVERIFY(newFile.open(QIODevice::WriteOnly));
        QCOMPARE(newFile.write("new"), 3);
        newFile.close();
        const auto oldHash = QCryptographicHash::hash("old", QCryptographicHash::Sha256).toHex();
        const auto newHash = QCryptographicHash::hash("new", QCryptographicHash::Sha256).toHex();
        fixture.plan.operations.append(planning::PlannedFileChange{
            .tweakId = domain::TweakId::parse(u"devices.mixed-file"_s).value(),
            .targetState = u"configured"_s,
            .change = {.kind = domain::FileOperationKind::Replace,
                       .artifact = {.id = u"file"_s, .storageId = u"mixed"_s,
                                    .managedPath = inputRelative, .size = 3,
                                    .sha256 = newHash},
                       .destination = destination},
            .beforeFingerprint = execution::FileOperationExecutor::fingerprint(
                {.destination = destination, .existed = true, .size = 3, .sha256 = oldHash})});

        const domain::ScheduledTaskLocation taskLocation{u"\\Tweakopedia"_s, u"Mixed"_s};
        const domain::ScheduledTaskSnapshot taskBefore{taskLocation, true};
        fixture.plan.operations.append(planning::PlannedScheduledTaskChange{
            .tweakId = domain::TweakId::parse(u"tasks.mixed"_s).value(),
            .targetState = u"disabled"_s,
            .change = {taskLocation, false},
            .beforeFingerprint = execution::ScheduledTaskExecutor::fingerprint(taskBefore)});

        const domain::PowerSettingLocation powerLocation{
            u"active"_s, u"54533251-82be-4824-96c1-47b60b740d00"_s,
            u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s, domain::PowerSource::Ac};
        const domain::PowerSettingSnapshot powerBefore{
            powerLocation, u"381b4222-f694-41f0-9685-ff5bb260df2e"_s, 7};
        fixture.plan.operations.append(planning::PlannedPowerSettingChange{
            .tweakId = domain::TweakId::parse(u"power.mixed"_s).value(),
            .targetState = u"maximum"_s, .change = {powerLocation, 100},
            .beforeFingerprint = execution::PowerSettingExecutor::fingerprint(powerBefore)});
        power.failNextWrite = true;

        execution::ExecutionBackends backends{
            .registry = &registry, .scheduledTasks = &tasks, .powerSettings = &power};
        execution::TransactionRunner runner(
            backends, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QVERIFY(result.rolledBack);
        QVERIFY(registry.read(registryLocation).missing);
        QVERIFY(tasks.enabled);
        QFile restored(destination);
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), QByteArray("old"));
        const auto before = fixture.files->readBefore(fixture.plan.transactionId);
        QVERIFY(before.has_value());
        QCOMPARE(before->value(u"operations"_s).toArray().size(), 4);
    }

    void rejectsMissingBackendBeforeFirstMutation()
    {
        Fixture fixture;
        ScriptedBackend registry;
        const auto target = location(u"MustStayUntouched"_s);
        const auto missing = platform::RegistryReadResult::missing();
        fixture.plan.operations.append(change(target, 1, missing));
        const domain::ScheduledTaskLocation task{u"\\Tweakopedia"_s, u"MissingBackend"_s};
        fixture.plan.operations.append(planning::PlannedScheduledTaskChange{
            .tweakId = *domain::TweakId::parse(u"tasks.missing-backend"_s),
            .targetState = u"disabled"_s, .change = {task, false},
            .beforeFingerprint = execution::ScheduledTaskExecutor::fingerprint({task, true})});

        execution::TransactionRunner runner(
            {.registry = &registry}, *fixture.files, *fixture.repository);
        const auto result = runner.run(fixture.plan);

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"operation.unsupported"_s);
        QVERIFY(registry.writes.isEmpty());
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

        execution::TransactionRunner runner(
            {.registry = &backend}, *fixture.files, *fixture.repository);
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
