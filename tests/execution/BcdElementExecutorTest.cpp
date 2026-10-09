#include "execution/BcdElementExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeBcdBackend final : public platform::IBcdBackend
{
public:
    platform::BcdReadResult current;
    platform::BcdReadResult read(const domain::BcdElementSpec&) const override { return current; }
    platform::BcdMutationResult set(
        const domain::BcdElementSpec&, const domain::BcdValue& value) override
    {
        current = platform::BcdReadResult::present(value);
        return {.success = true};
    }
    platform::BcdMutationResult remove(const domain::BcdElementSpec&) override
    {
        current = platform::BcdReadResult::missingElement();
        return {.success = true};
    }
};

const domain::BcdElementSpec spec{
    .objectId = u"{current}"_s,
    .elementType = 0x260000A5,
    .valueKind = domain::BcdValueKind::Boolean,
};

} // namespace

class BcdElementExecutorTest final : public QObject
{
    Q_OBJECT
private slots:
    void verifiesWriteAndRestoresMissing()
    {
        FakeBcdBackend backend;
        backend.current = platform::BcdReadResult::missingElement();
        execution::BcdElementExecutor executor(backend);
        const auto before = executor.capture(spec);
        QVERIFY(before.success);
        QVERIFY(!before.snapshot.existed);
        QVERIFY(executor.apply({.spec = spec, .value = domain::BcdValue{true}}).success);
        QVERIFY(executor.restore(before.snapshot).success);
        QVERIFY(backend.current.missing);
    }

    void rejectsStaleFingerprint()
    {
        FakeBcdBackend backend;
        backend.current = platform::BcdReadResult::present(domain::BcdValue{false});
        execution::BcdElementExecutor executor(backend);
        const auto captured = executor.capture(spec);
        QVERIFY(!executor.compareBefore(captured.snapshot, QByteArray(64, '0')).success);
    }
};

QTEST_APPLESS_MAIN(BcdElementExecutorTest)
#include "BcdElementExecutorTest.moc"
