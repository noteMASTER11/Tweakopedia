#include "detection/WindowsComponentStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {
class FakeComponentBackend final : public platform::IWindowsComponentBackend
{
public:
    platform::WindowsComponentQueryResult result;

    platform::WindowsComponentQueryResult query(
        const domain::WindowsComponentTarget&) const override
    {
        return result;
    }

    platform::WindowsComponentMutationResult setState(
        const domain::WindowsComponentTarget&, domain::WindowsComponentState) override
    {
        return {.success = true};
    }
};

domain::TweakDefinition definition()
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(u"windows-component.test"_s);
    tweak.title = u"Тестовый компонент"_s;
    tweak.compatibility.architectures = {domain::CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {domain::WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {{.id = u"on"_s, .title = u"Включено"_s},
                    {.id = u"off"_s, .title = u"Выключено"_s}};
    tweak.windowsComponentDetection = domain::WindowsComponentDetection{
        .target = {domain::WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s},
        .states = {{domain::WindowsComponentState::Enabled, u"on"_s},
                   {domain::WindowsComponentState::Disabled, u"off"_s}},
    };
    return tweak;
}

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11,
            .build = 26200,
            .architecture = domain::CpuArchitecture::X64};
}
}

class WindowsComponentStateDetectorTest final : public QObject
{
    Q_OBJECT
private slots:
    void mapsNamedState()
    {
        FakeComponentBackend backend;
        backend.result = platform::WindowsComponentQueryResult::present(
            domain::WindowsComponentState::Enabled);
        const auto detected = detection::WindowsComponentStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(detected.status, domain::DetectionStatus::Named);
        QCOMPARE(detected.stateId, u"on"_s);
        QVERIFY(!detected.fingerprint.isEmpty());
    }

    void mapsUnsupportedTarget()
    {
        FakeComponentBackend backend;
        backend.result = platform::WindowsComponentQueryResult::unsupported(u"Не найден."_s);
        const auto detected = detection::WindowsComponentStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(detected.status, domain::DetectionStatus::Unsupported);
    }
};

QTEST_APPLESS_MAIN(WindowsComponentStateDetectorTest)
#include "WindowsComponentStateDetectorTest.moc"
