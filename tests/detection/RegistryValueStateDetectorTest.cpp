#include "detection/RegistryValueStateDetector.h"
#include "fakes/FakeRegistryBackend.h"

#include <QtTest/QTest>

using namespace tweakopedia::detection;
using namespace tweakopedia::domain;
using namespace tweakopedia::platform;
using namespace tweakopedia::tests;
using namespace Qt::StringLiterals;

namespace {

RegistryLocation location(QString name = u"Value"_s)
{
    return {
        .hive = RegistryHive::LocalMachine,
        .key = u"SOFTWARE\\Tweakopedia\\RegistryValueDetector"_s,
        .valueName = std::move(name),
        .view = RegistryView::Registry64,
    };
}

TweakDefinition definition()
{
    TweakDefinition tweak;
    tweak.id = *TweakId::parse(u"devices.registry-value-detector"_s);
    tweak.compatibility.architectures = {CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {
        {.id = u"missing"_s, .title = u"Отсутствует"_s,
         .operations = {DeleteRegistryValueOperation{.location = location()}}},
        {.id = u"qword"_s, .title = u"QWORD"_s,
         .operations = {SetRegistryValueOperation{
             .location = location(), .value = RegistryValueSpec::qword(0x0102030405060708ULL)}}},
        {.id = u"binary"_s, .title = u"Binary"_s,
         .operations = {SetRegistryValueOperation{
             .location = location(), .value = RegistryValueSpec::binary(QByteArray::fromHex("deadbeef"))}}},
    };
    tweak.valueDetection = RegistryValueDetection{
        .location = location(),
        .valuesByState = {
            {u"qword"_s, RegistryValueSpec::qword(0x0102030405060708ULL)},
            {u"binary"_s, RegistryValueSpec::binary(QByteArray::fromHex("deadbeef"))},
        },
        .missingState = u"missing"_s,
    };
    return tweak;
}

SystemProfile profile()
{
    return {
        .family = WindowsFamily::Windows11,
        .build = 26200,
        .ubr = 9457,
        .edition = u"Professional"_s,
        .architecture = CpuArchitecture::X64,
    };
}

RegistryValueType valueType(quint32 nativeType)
{
    if (nativeType == 11) return RegistryValueType::Qword;
    if (nativeType == 3) return RegistryValueType::Binary;
    return RegistryValueType::Unknown;
}

} // namespace

class RegistryValueStateDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void matchesExactNativeTypeAndBytes()
    {
        FakeRegistryBackend backend;
        const auto expected = RegistryValueSpec::qword(0x0102030405060708ULL);
        backend.setReadResult(location(), RegistryReadResult::present(
            valueType(expected.nativeType), expected.rawValue, expected.nativeType));

        const auto result = RegistryValueStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Named);
        QCOMPARE(result.stateId, u"qword"_s);
        QVERIFY(!result.fingerprint.isEmpty());
    }

    void treatsSameBytesWithDifferentNativeTypeAsCustom()
    {
        FakeRegistryBackend backend;
        const auto expected = RegistryValueSpec::qword(0x0102030405060708ULL);
        backend.setReadResult(location(), RegistryReadResult::present(
            RegistryValueType::Binary, expected.rawValue, 3));

        const auto result = RegistryValueStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Custom);
        QCOMPARE(result.stateId, u"custom"_s);
    }

    void mapsMissingValueToDeclaredState()
    {
        FakeRegistryBackend backend;

        const auto result = RegistryValueStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Named);
        QCOMPARE(result.stateId, u"missing"_s);
    }

    void capturesEveryGenericRegistryOperation()
    {
        auto tweak = definition();
        const auto secondary = location(u"Secondary"_s);
        tweak.states[1].operations.append(SetRegistryValueOperation{
            .location = secondary,
            .value = RegistryValueSpec::string(u"value"),
        });
        FakeRegistryBackend backend;
        const auto expected = RegistryValueSpec::qword(0x0102030405060708ULL);
        backend.setReadResult(location(), RegistryReadResult::present(
            RegistryValueType::Qword, expected.rawValue, expected.nativeType));
        backend.setReadResult(secondary, RegistryReadResult::present(
            RegistryValueType::String, RegistryValueSpec::string(u"old").rawValue, 1));

        const auto result = RegistryValueStateDetector{}.detect(tweak, backend, profile());

        QCOMPARE(result.registryFingerprints.size(), 2);
        QVERIFY(std::any_of(result.registryFingerprints.cbegin(), result.registryFingerprints.cend(),
                            [&](const auto& item) { return item.location == secondary; }));
    }
};

QTEST_APPLESS_MAIN(RegistryValueStateDetectorTest)

#include "RegistryValueStateDetectorTest.moc"
