#include "execution/RegistryTreeExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::RegistryKeyLocation location()
{
    return {
        .hive = domain::RegistryHive::CurrentUser,
        .key = u"Software\\Tweakopedia\\TreeExecutor"_s,
        .view = domain::RegistryView::Registry64,
    };
}

domain::RegistryTreeSnapshot populatedSnapshot()
{
    return {
        .location = location(),
        .existed = true,
        .nodes = {
            {.relativePath = {},
             .values = {{u"Text"_s, domain::RegistryValueSpec::string(u"root")}}},
            {.relativePath = u"Child"_s,
             .values = {{u"Data"_s, domain::RegistryValueSpec::binary(QByteArray::fromHex("deadbeef"))}}},
        },
    };
}

class TreeBackend final : public platform::IRegistryBackend
{
public:
    platform::RegistryReadResult read(const domain::RegistryLocation&) const override
    {
        return platform::RegistryReadResult::missing();
    }
    platform::RegistryWriteResult writeDword(const domain::RegistryLocation&, quint32) override
    {
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation&, quint32, const QByteArray&) override
    {
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult deleteValue(const domain::RegistryLocation&) override
    {
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryTreeReadResult readTree(
        const domain::RegistryKeyLocation&, const domain::RegistryTreeLimits&) const override
    {
        if (!readSuccess) return platform::RegistryTreeReadResult::failed(u"registry.tree_limit"_s);
        return platform::RegistryTreeReadResult::succeeded(tree);
    }
    platform::RegistryWriteResult createKey(const domain::RegistryKeyLocation& target) override
    {
        tree = {.location = target, .existed = true, .nodes = {{.relativePath = {}}}};
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult deleteTree(const domain::RegistryKeyLocation& target) override
    {
        tree = {.location = target, .existed = false};
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult restoreTree(
        const domain::RegistryTreeSnapshot& snapshot) override
    {
        tree = snapshot;
        return platform::RegistryWriteResult::succeeded();
    }

    mutable domain::RegistryTreeSnapshot tree{.location = location(), .existed = false};
    bool readSuccess{true};
};

} // namespace

class RegistryTreeExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void serializesCompleteSnapshot()
    {
        const auto snapshot = populatedSnapshot();

        const auto decoded = domain::RegistryTreeSnapshot::fromJson(snapshot.toJson());

        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, snapshot);
    }

    void createsDeletesAndRestoresTree()
    {
        TreeBackend backend;
        backend.tree = populatedSnapshot();
        execution::RegistryTreeExecutor executor(backend);
        const auto captured = executor.capture(location());
        QVERIFY(captured.success);

        QVERIFY(executor.apply(domain::DeleteRegistryTreeOperation{location()}).success);
        QVERIFY(!backend.tree.existed);
        QVERIFY(executor.restore(captured.snapshot).success);
        QCOMPARE(backend.tree, populatedSnapshot());

        QVERIFY(executor.apply(domain::CreateRegistryKeyOperation{location()}).success);
        QVERIFY(backend.tree.existed);
    }

    void rejectsCaptureWhenTraversalLimitIsExceeded()
    {
        TreeBackend backend;
        backend.readSuccess = false;
        execution::RegistryTreeExecutor executor(backend);

        const auto captured = executor.capture(location());

        QVERIFY(!captured.success);
        QCOMPARE(captured.code, u"registry.tree_limit"_s);
    }
};

QTEST_APPLESS_MAIN(RegistryTreeExecutorTest)

#include "RegistryTreeExecutorTest.moc"
