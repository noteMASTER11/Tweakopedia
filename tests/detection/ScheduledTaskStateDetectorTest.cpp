#include "detection/ScheduledTaskStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeTaskBackend final : public platform::IScheduledTaskBackend
{
public:
    platform::ScheduledTaskReadResult next;
    platform::ScheduledTaskReadResult read(const domain::ScheduledTaskLocation&) override
    {
        return next;
    }
    platform::ScheduledTaskWriteResult setEnabled(
        const domain::ScheduledTaskLocation&, bool) override { return {.success = true}; }
};

domain::TweakDefinition definition()
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(u"privacy.test-scheduled-task"_s);
    tweak.compatibility.architectures = {domain::CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {domain::WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {
        {.id = u"disabled"_s, .title = u"Выключено"_s},
        {.id = u"enabled"_s, .title = u"Включено"_s},
    };
    tweak.scheduledTaskDetection = domain::ScheduledTaskDetection{
        .location = {.folder = u"\\Microsoft\\Windows\\Test"_s, .name = u"Task"_s},
        .enabledState = u"enabled"_s,
        .disabledState = u"disabled"_s,
    };
    return tweak;
}

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11, .build = 26200,
            .architecture = domain::CpuArchitecture::X64};
}

} // namespace

class ScheduledTaskStateDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void detectsEnabledAndDisabled_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<QString>("state");
        QTest::newRow("enabled") << true << u"enabled"_s;
        QTest::newRow("disabled") << false << u"disabled"_s;
    }

    void detectsEnabledAndDisabled()
    {
        QFETCH(bool, enabled);
        QFETCH(QString, state);
        FakeTaskBackend backend;
        backend.next = platform::ScheduledTaskReadResult::present(enabled);

        const auto detected = detection::ScheduledTaskStateDetector{}.detect(
            definition(), backend, profile());

        QCOMPARE(detected.status, domain::DetectionStatus::Named);
        QCOMPARE(detected.stateId, state);
        QCOMPARE(detected.fingerprint.size(), 64);
    }

    void mapsMissingTaskToUnsupportedAndBackendErrorToUnknown()
    {
        FakeTaskBackend backend;
        backend.next = platform::ScheduledTaskReadResult::missingTask();
        QCOMPARE(detection::ScheduledTaskStateDetector{}.detect(
                     definition(), backend, profile()).status,
                 domain::DetectionStatus::Unsupported);
        backend.next = platform::ScheduledTaskReadResult::failed(u"COM error"_s);
        const auto failed = detection::ScheduledTaskStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(failed.status, domain::DetectionStatus::Unknown);
        QCOMPARE(failed.details, u"COM error"_s);
    }
};

QTEST_APPLESS_MAIN(ScheduledTaskStateDetectorTest)

#include "ScheduledTaskStateDetectorTest.moc"
