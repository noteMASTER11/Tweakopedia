#include "execution/RegistryDwordExecutor.h"
#include "execution/RegistrySnapshot.h"
#include "fakes/FakeRegistryBackend.h"

#include <QtEndian>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::RegistryLocation location()
{
    return {
        .hive = domain::RegistryHive::CurrentUser,
        .key = u"Software\\Tweakopedia\\ExecutorTest"_s,
        .valueName = u"Value"_s,
        .view = domain::RegistryView::Registry64,
    };
}

QByteArray dwordBytes(quint32 value)
{
    QByteArray bytes(sizeof(value), Qt::Uninitialized);
    qToLittleEndian(value, bytes.data());
    return bytes;
}

} // namespace

class RegistryDwordExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void restoresExactOriginalValue_data()
    {
        QTest::addColumn<int>("presence");
        QTest::addColumn<int>("type");
        QTest::addColumn<quint32>("nativeType");
        QTest::addColumn<QByteArray>("bytes");

        QTest::newRow("missing")
            << int(platform::RegistryPresence::Missing)
            << int(platform::RegistryValueType::None)
            << quint32{0}
            << QByteArray{};
        QTest::newRow("dword-zero")
            << int(platform::RegistryPresence::Present)
            << int(platform::RegistryValueType::Dword)
            << quint32{4}
            << dwordBytes(0);
        QTest::newRow("dword-one")
            << int(platform::RegistryPresence::Present)
            << int(platform::RegistryValueType::Dword)
            << quint32{4}
            << dwordBytes(1);
        QTest::newRow("wrong-type-expand-string")
            << int(platform::RegistryPresence::Present)
            << int(platform::RegistryValueType::String)
            << quint32{2}
            << QByteArray::fromHex("6f006e0065000000");
    }

    void restoresExactOriginalValue()
    {
        QFETCH(int, presence);
        QFETCH(int, type);
        QFETCH(quint32, nativeType);
        QFETCH(QByteArray, bytes);

        tests::FakeRegistryBackend backend;
        const auto original = platform::RegistryReadResult{
            .presence = static_cast<platform::RegistryPresence>(presence),
            .type = static_cast<platform::RegistryValueType>(type),
            .nativeType = nativeType,
            .rawValue = bytes,
        };
        backend.setReadResult(location(), original);
        execution::RegistryDwordExecutor executor(backend);

        const auto captured = executor.capture(location());
        QVERIFY(captured.success);
        QVERIFY(executor.apply({.location = location(), .value = 9}).success);
        QVERIFY(executor.restore(captured.snapshot).success);

        const auto restored = backend.read(location());
        QCOMPARE(restored.presence, original.presence);
        QCOMPARE(restored.type, original.type);
        QCOMPARE(restored.nativeType, original.nativeType);
        QCOMPARE(restored.rawValue, original.rawValue);
    }

    void rejectsChangedBeforeFingerprint()
    {
        tests::FakeRegistryBackend backend;
        backend.setReadResult(location(), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, dwordBytes(0), 4));
        execution::RegistryDwordExecutor executor(backend);

        const auto captured = executor.capture(location());
        QVERIFY(captured.success);
        const auto result = executor.compareBefore(captured.snapshot, QByteArray(64, 'f'));

        QVERIFY(!result.success);
        QCOMPARE(result.code, u"state.changed"_s);
    }
};

QTEST_APPLESS_MAIN(RegistryDwordExecutorTest)

#include "RegistryDwordExecutorTest.moc"
