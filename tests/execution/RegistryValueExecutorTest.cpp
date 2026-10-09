#include "execution/RegistryValueExecutor.h"
#include "fakes/FakeRegistryBackend.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::RegistryLocation location(QString name = u"Value"_s)
{
    return {
        .hive = domain::RegistryHive::CurrentUser,
        .key = u"Software\\Tweakopedia\\RegistryValueExecutor"_s,
        .valueName = std::move(name),
        .view = domain::RegistryView::Registry64,
    };
}

class CorruptingBackend final : public platform::IRegistryBackend
{
public:
    platform::RegistryReadResult read(const domain::RegistryLocation&) const override
    {
        return current;
    }
    platform::RegistryWriteResult writeDword(const domain::RegistryLocation&, quint32) override
    {
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult writeValue(
        const domain::RegistryLocation&, const domain::RegistryValueSpec& value) override
    {
        current = platform::RegistryReadResult::present(
            platform::RegistryValueType::Binary, value.rawValue + 'x', value.nativeType);
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation&, quint32, const QByteArray&) override
    {
        return platform::RegistryWriteResult::succeeded();
    }
    platform::RegistryWriteResult deleteValue(const domain::RegistryLocation&) override
    {
        current = platform::RegistryReadResult::present(
            platform::RegistryValueType::Binary, QByteArray("not-deleted"), 3);
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryReadResult current;
};

} // namespace

class RegistryValueExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesNativeValuesWithoutChangingBytes_data()
    {
        QTest::addColumn<quint32>("nativeType");
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("qword") << quint32{11} << domain::RegistryValueSpec::qword(42).rawValue;
        QTest::newRow("string") << quint32{1} << domain::RegistryValueSpec::string(u"OEM").rawValue;
        QTest::newRow("binary") << quint32{3} << QByteArray::fromHex("deadbeef");
    }

    void writesNativeValuesWithoutChangingBytes()
    {
        QFETCH(quint32, nativeType);
        QFETCH(QByteArray, bytes);
        tests::FakeRegistryBackend backend;
        execution::RegistryValueExecutor executor(backend);

        const auto result = executor.apply(domain::SetRegistryValueOperation{
            .location = location(),
            .value = {.nativeType = nativeType, .rawValue = bytes},
        });

        QVERIFY(result.success);
        const auto actual = backend.read(location());
        QCOMPARE(actual.nativeType, nativeType);
        QCOMPARE(actual.rawValue, bytes);
    }

    void deletesAndVerifiesMissingValue()
    {
        tests::FakeRegistryBackend backend;
        backend.setReadResult(location(), platform::RegistryReadResult::present(
            platform::RegistryValueType::String,
            domain::RegistryValueSpec::string(u"old").rawValue, 1));
        execution::RegistryValueExecutor executor(backend);

        const auto result = executor.apply(domain::DeleteRegistryValueOperation{location()});

        QVERIFY(result.success);
        QCOMPARE(backend.read(location()).presence, platform::RegistryPresence::Missing);
    }

    void restoresPresentAndMissingSnapshots()
    {
        tests::FakeRegistryBackend backend;
        execution::RegistryValueExecutor executor(backend);
        const auto original = platform::RegistryReadResult::present(
            platform::RegistryValueType::ExpandString,
            domain::RegistryValueSpec::expandString(u"%TEMP%").rawValue, 2);
        backend.setReadResult(location(), original);
        const auto present = executor.capture(location()).snapshot;
        QVERIFY(executor.apply(domain::DeleteRegistryValueOperation{location()}).success);
        QVERIFY(executor.restore(present).success);
        QCOMPARE(backend.read(location()).rawValue, original.rawValue);
        QCOMPARE(backend.read(location()).nativeType, quint32{2});

        const auto missingLocation = location(u"Missing"_s);
        const auto missing = executor.capture(missingLocation).snapshot;
        QVERIFY(executor.apply(domain::SetRegistryValueOperation{
            .location = missingLocation, .value = domain::RegistryValueSpec::dword(1)}).success);
        QVERIFY(executor.restore(missing).success);
        QCOMPARE(backend.read(missingLocation).presence, platform::RegistryPresence::Missing);
    }

    void detectsFailedWriteVerification()
    {
        CorruptingBackend backend;
        execution::RegistryValueExecutor executor(backend);

        const auto result = executor.apply(domain::SetRegistryValueOperation{
            .location = location(), .value = domain::RegistryValueSpec::binary("value")});

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"registry.verify_failed"_s);
    }
};

QTEST_APPLESS_MAIN(RegistryValueExecutorTest)

#include "RegistryValueExecutorTest.moc"
