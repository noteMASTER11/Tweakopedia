#include "detection/BcdStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeBcdBackend final : public platform::IBcdBackend
{
public:
    platform::BcdReadResult result;
    platform::BcdReadResult read(const domain::BcdElementSpec&) const override { return result; }
    platform::BcdMutationResult set(
        const domain::BcdElementSpec&, const domain::BcdValue&) override { return {.success = true}; }
    platform::BcdMutationResult remove(const domain::BcdElementSpec&) override { return {.success = true}; }
};

domain::TweakDefinition definition()
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(u"boot.dynamic-tick"_s);
    tweak.title = u"Динамический системный таймер"_s;
    tweak.compatibility.architectures = {domain::CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {domain::WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {
        {.id = u"default"_s, .title = u"По умолчанию"_s},
        {.id = u"disabled"_s, .title = u"Выключено"_s},
    };
    tweak.bcdDetection = domain::BcdElementDetection{
        .spec = {.objectId = u"{current}"_s, .elementType = 0x260000A5,
                 .valueKind = domain::BcdValueKind::Boolean},
        .valuesByState = {{u"disabled"_s, domain::BcdValue{true}}},
        .missingState = u"default"_s,
    };
    return tweak;
}

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11, .build = 26200,
            .architecture = domain::CpuArchitecture::X64};
}

} // namespace

class BcdStateDetectorTest final : public QObject
{
    Q_OBJECT
private slots:
    void mapsMissingAndTypedValues()
    {
        auto backend = FakeBcdBackend{};
        auto state = detection::BcdStateDetector{}.detect(definition(), backend, profile());
        QCOMPARE(state.status, domain::DetectionStatus::Named);
        QCOMPARE(state.stateId, u"default"_s);

        backend.result = platform::BcdReadResult::present(domain::BcdValue{true});
        state = detection::BcdStateDetector{}.detect(definition(), backend, profile());
        QCOMPARE(state.stateId, u"disabled"_s);
        QCOMPARE(state.fingerprint.size(), 64);
    }

    void reportsBackendFailureAsUnknown()
    {
        FakeBcdBackend backend;
        backend.result = platform::BcdReadResult::failed(u"provider unavailable"_s);
        const auto state = detection::BcdStateDetector{}.detect(definition(), backend, profile());
        QCOMPARE(state.status, domain::DetectionStatus::Unknown);
    }
};

QTEST_APPLESS_MAIN(BcdStateDetectorTest)
#include "BcdStateDetectorTest.moc"
