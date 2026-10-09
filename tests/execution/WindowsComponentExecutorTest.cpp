#include "execution/WindowsComponentExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {
class FakeComponentBackend final : public platform::IWindowsComponentBackend
{
public:
    domain::WindowsComponentState state{domain::WindowsComponentState::Disabled};
    bool restartRequired{};

    platform::WindowsComponentQueryResult query(
        const domain::WindowsComponentTarget&) const override
    {
        return platform::WindowsComponentQueryResult::present(state);
    }

    platform::WindowsComponentMutationResult setState(
        const domain::WindowsComponentTarget&, domain::WindowsComponentState requested) override
    {
        state = requested;
        return {.success = true, .restartRequired = restartRequired};
    }
};

const domain::WindowsComponentTarget target{
    domain::WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s};
}

class WindowsComponentExecutorTest final : public QObject
{
    Q_OBJECT
private slots:
    void writesVerifiesAndRestoresExactState()
    {
        FakeComponentBackend backend;
        execution::WindowsComponentExecutor executor(backend);
        const auto before = executor.capture(target);
        QVERIFY(before.success);
        const auto applied = executor.apply({target, domain::WindowsComponentState::Enabled});
        QVERIFY(applied.success);
        QCOMPARE(backend.state, domain::WindowsComponentState::Enabled);
        QVERIFY(executor.restore(before.snapshot).success);
        QCOMPARE(backend.state, domain::WindowsComponentState::Disabled);
    }

    void propagatesRestartRequirementAndRejectsStaleState()
    {
        FakeComponentBackend backend;
        backend.restartRequired = true;
        execution::WindowsComponentExecutor executor(backend);
        const auto before = executor.capture(target);
        QVERIFY(!executor.compareBefore(before.snapshot, QByteArray(64, '0')).success);
        const auto applied = executor.apply({target, domain::WindowsComponentState::Enabled});
        QVERIFY(applied.success);
        QVERIFY(applied.restartRequired);
    }
};

QTEST_APPLESS_MAIN(WindowsComponentExecutorTest)
#include "WindowsComponentExecutorTest.moc"
