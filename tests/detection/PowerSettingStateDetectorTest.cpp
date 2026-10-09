#include "detection/PowerSettingStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {
class FakePowerBackend final : public platform::IPowerSettingBackend
{
public:
    QHash<int, platform::PowerSettingReadResult> values;
    platform::PowerSettingReadResult read(const domain::PowerSettingLocation& location) const override
    { return values.value(static_cast<int>(location.source)); }
    platform::PowerSettingMutationResult write(
        const domain::PowerSettingLocation&, quint32) override { return {.success = true}; }
};

domain::TweakDefinition definition(domain::PowerSource source)
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(source == domain::PowerSource::Ac
        ? u"power.test-ac"_s : u"power.test-dc"_s);
    tweak.title = u"Тестовый параметр питания"_s;
    tweak.compatibility.architectures = {domain::CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {domain::WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {{.id = u"zero"_s, .title = u"Ноль"_s},
                    {.id = u"one"_s, .title = u"Один"_s}};
    tweak.powerDetection = domain::PowerSettingDetection{
        .location = {.scheme = u"active"_s,
                     .subgroup = u"54533251-82be-4824-96c1-47b60b740d00"_s,
                     .setting = u"3b04d4fd-1cc7-4f23-ab1c-d1337819c4bb"_s,
                     .source = source},
        .statesByIndex = {{0, u"zero"_s}, {1, u"one"_s}},
    };
    return tweak;
}
domain::SystemProfile profile() { return {.family = domain::WindowsFamily::Windows11,
    .build = 26200, .architecture = domain::CpuArchitecture::X64}; }
}

class PowerSettingStateDetectorTest final : public QObject
{
    Q_OBJECT
private slots:
    void keepsAcAndDcIndependent()
    {
        FakePowerBackend backend;
        backend.values.insert(static_cast<int>(domain::PowerSource::Ac),
                              platform::PowerSettingReadResult::present(0, u"guid"_s));
        backend.values.insert(static_cast<int>(domain::PowerSource::Dc),
                              platform::PowerSettingReadResult::present(1, u"guid"_s));
        QCOMPARE(detection::PowerSettingStateDetector{}.detect(
                     definition(domain::PowerSource::Ac), backend, profile()).stateId, u"zero"_s);
        QCOMPARE(detection::PowerSettingStateDetector{}.detect(
                     definition(domain::PowerSource::Dc), backend, profile()).stateId, u"one"_s);
    }
    void mapsMissingSettingToUnsupported()
    {
        FakePowerBackend backend;
        const auto state = detection::PowerSettingStateDetector{}.detect(
            definition(domain::PowerSource::Ac), backend, profile());
        QCOMPARE(state.status, domain::DetectionStatus::Unsupported);
    }
};
QTEST_APPLESS_MAIN(PowerSettingStateDetectorTest)
#include "PowerSettingStateDetectorTest.moc"
