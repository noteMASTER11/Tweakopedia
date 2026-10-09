#include "execution/PowerSettingExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {
class FakePowerBackend final : public platform::IPowerSettingBackend
{
public:
    quint32 value{7};
    platform::PowerSettingReadResult read(const domain::PowerSettingLocation&) const override
    { return platform::PowerSettingReadResult::present(value, u"scheme-guid"_s); }
    platform::PowerSettingMutationResult write(
        const domain::PowerSettingLocation&, quint32 index) override
    { value = index; return {.success = true}; }
};
const domain::PowerSettingLocation location{
    u"active"_s, u"54533251-82be-4824-96c1-47b60b740d00"_s,
    u"3b04d4fd-1cc7-4f23-ab1c-d1337819c4bb"_s, domain::PowerSource::Ac};
}
class PowerSettingExecutorTest final : public QObject
{
    Q_OBJECT
private slots:
    void writesVerifiesAndRestoresExactIndex()
    {
        FakePowerBackend backend;
        execution::PowerSettingExecutor executor(backend);
        const auto before = executor.capture(location);
        QVERIFY(before.success);
        QVERIFY(executor.apply({location, 2}).success);
        QCOMPARE(backend.value, 2U);
        QVERIFY(executor.restore(before.snapshot).success);
        QCOMPARE(backend.value, 7U);
    }
    void rejectsStaleValue()
    {
        FakePowerBackend backend;
        execution::PowerSettingExecutor executor(backend);
        const auto captured = executor.capture(location);
        QVERIFY(!executor.compareBefore(captured.snapshot, QByteArray(64, '0')).success);
    }
};
QTEST_APPLESS_MAIN(PowerSettingExecutorTest)
#include "PowerSettingExecutorTest.moc"
