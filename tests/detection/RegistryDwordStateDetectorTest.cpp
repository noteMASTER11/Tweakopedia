#include "detection/RegistryDwordStateDetector.h"
#include "fakes/FakeRegistryBackend.h"

#include <QtEndian>
#include <QtTest/QTest>

using namespace tweakopedia::detection;
using namespace tweakopedia::domain;
using namespace tweakopedia::platform;
using namespace tweakopedia::tests;
using namespace Qt::StringLiterals;

namespace {

RegistryLocation location()
{
    return {
        .hive = RegistryHive::LocalMachine,
        .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
        .valueName = u"LongPathsEnabled"_s,
        .view = RegistryView::Registry64,
    };
}

QByteArray dwordBytes(quint32 value)
{
    QByteArray bytes(sizeof(value), Qt::Uninitialized);
    qToLittleEndian(value, bytes.data());
    return bytes;
}

TweakDefinition definition()
{
    TweakDefinition tweak;
    tweak.id = *TweakId::parse(u"filesystem.win32-long-paths");
    tweak.compatibility.architectures = {CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {WindowsFamily::Windows10, WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 14393;
    tweak.states = {
        {.id = u"disabled"_s, .title = u"Выключено"_s,
         .operations = {SetRegistryDwordOperation{.location = location(), .value = 0}}},
        {.id = u"enabled"_s, .title = u"Включено"_s,
         .operations = {SetRegistryDwordOperation{.location = location(), .value = 1}}},
    };
    tweak.detection = RegistryDwordDetection{
        .location = location(),
        .statesByValue = {{0, u"disabled"_s}, {1, u"enabled"_s}},
        .missingState = u"disabled"_s,
    };
    return tweak;
}

SystemProfile profile()
{
    return {
        .family = WindowsFamily::Windows11,
        .build = 26200,
        .ubr = 1,
        .edition = u"Professional"_s,
        .architecture = CpuArchitecture::X64,
    };
}

} // namespace

class RegistryDwordStateDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void mapsDwordOneToEnabled()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(RegistryValueType::Dword, dwordBytes(1)));

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Named);
        QCOMPARE(result.stateId, u"enabled"_s);
        QVERIFY(!result.fingerprint.isEmpty());
    }

    void capturesFingerprintForEveryRegistryOperation()
    {
        auto tweak = definition();
        const RegistryLocation secondary{
            .hive = RegistryHive::LocalMachine,
            .key = u"SYSTEM\\CurrentControlSet\\Control\\CrashControl"_s,
            .valueName = u"DisableEmoticon"_s,
            .view = RegistryView::Registry64,
        };
        tweak.states[1].operations.append(SetRegistryDwordOperation{
            .location = secondary,
            .value = 1,
        });
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(
            RegistryValueType::Dword, dwordBytes(0)));
        backend.setReadResult(secondary, RegistryReadResult::present(
            RegistryValueType::Dword, dwordBytes(1)));

        const auto result = RegistryDwordStateDetector{}.detect(tweak, backend, profile());

        QCOMPARE(result.registryFingerprints.size(), 2);
        QVERIFY(std::any_of(
            result.registryFingerprints.cbegin(), result.registryFingerprints.cend(),
            [&](const DetectedRegistryFingerprint& item) {
                return item.location == secondary && !item.fingerprint.isEmpty();
            }));
    }

    void mapsDwordZeroToDisabled()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(RegistryValueType::Dword, dwordBytes(0)));

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Named);
        QCOMPARE(result.stateId, u"disabled"_s);
    }

    void mapsMissingValueToDeclaredState()
    {
        FakeRegistryBackend backend;

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Named);
        QCOMPARE(result.stateId, u"disabled"_s);
        QVERIFY(!result.fingerprint.isEmpty());
    }

    void treatsWrongRegistryTypeAsUnknown()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(RegistryValueType::String, QByteArray("1\0", 2)));

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Unknown);
        QCOMPARE(result.stateId, u"unknown"_s);
    }

    void treatsReadFailureAsUnknown()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::failed(std::make_error_code(std::errc::permission_denied)));

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Unknown);
        QCOMPARE(result.stateId, u"unknown"_s);
        QVERIFY(!result.details.isEmpty());
    }

    void mapsUnknownDwordToCustom()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(RegistryValueType::Dword, dwordBytes(7)));

        const auto result = RegistryDwordStateDetector{}.detect(definition(), backend, profile());

        QCOMPARE(result.status, DetectionStatus::Custom);
        QCOMPARE(result.stateId, u"custom"_s);
    }
};

QTEST_APPLESS_MAIN(RegistryDwordStateDetectorTest)

#include "RegistryDwordStateDetectorTest.moc"
