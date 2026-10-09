#include "execution/ScheduledTaskExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeTaskBackend final : public platform::IScheduledTaskBackend
{
public:
    platform::ScheduledTaskReadResult current = platform::ScheduledTaskReadResult::present(true);
    bool failWrite{};
    QStringList writes;

    platform::ScheduledTaskReadResult read(const domain::ScheduledTaskLocation&) override
    {
        return current;
    }
    platform::ScheduledTaskWriteResult setEnabled(
        const domain::ScheduledTaskLocation&, bool enabled) override
    {
        if (failWrite) return {.error = u"write failed"_s};
        current = platform::ScheduledTaskReadResult::present(enabled);
        writes.append(enabled ? u"enabled"_s : u"disabled"_s);
        return {.success = true};
    }
};

const domain::ScheduledTaskLocation location{
    .folder = u"\\TweakopediaTests"_s,
    .name = u"Task"_s,
};

} // namespace

class ScheduledTaskExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void appliesVerifiesAndRestoresExactEnabledState()
    {
        FakeTaskBackend backend;
        execution::ScheduledTaskExecutor executor(backend);
        const auto captured = executor.capture(location);
        QVERIFY(captured.success);
        const auto fingerprint = execution::ScheduledTaskExecutor::fingerprint(captured.snapshot);
        QVERIFY(executor.compareBefore(captured.snapshot, fingerprint).success);
        QVERIFY(executor.apply({.location = location, .enabled = false}).success);
        QVERIFY(!backend.current.enabled);
        QVERIFY(executor.restore(captured.snapshot).success);
        QVERIFY(backend.current.enabled);
    }

    void rejectsStaleFingerprintAndMissingTask()
    {
        FakeTaskBackend backend;
        execution::ScheduledTaskExecutor executor(backend);
        const auto captured = executor.capture(location);
        QVERIFY(captured.success);
        QCOMPARE(executor.compareBefore(captured.snapshot, QByteArray(64, '0')).code,
                 u"scheduled_task.before_changed"_s);
        backend.current = platform::ScheduledTaskReadResult::missingTask();
        QCOMPARE(executor.capture(location).code, u"scheduled_task.missing"_s);
    }
};

QTEST_APPLESS_MAIN(ScheduledTaskExecutorTest)

#include "ScheduledTaskExecutorTest.moc"
