#include "detection/RegistryTreeStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class TreeBackend final : public platform::IRegistryBackend
{
public:
    domain::RegistryTreeSnapshot snapshot;
    bool success{true};

    platform::RegistryReadResult read(const domain::RegistryLocation&) const override
    { return platform::RegistryReadResult::missing(); }
    platform::RegistryWriteResult writeDword(const domain::RegistryLocation&, quint32) override
    { return platform::RegistryWriteResult::succeeded(); }
    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation&, quint32, const QByteArray&) override
    { return platform::RegistryWriteResult::succeeded(); }
    platform::RegistryWriteResult deleteValue(const domain::RegistryLocation&) override
    { return platform::RegistryWriteResult::succeeded(); }
    platform::RegistryTreeReadResult readTree(
        const domain::RegistryKeyLocation& location,
        const domain::RegistryTreeLimits&) const override
    {
        if (!success) return platform::RegistryTreeReadResult::failed(u"registry.tree_read_failed"_s);
        auto result = snapshot;
        result.location = location;
        return platform::RegistryTreeReadResult::succeeded(std::move(result));
    }
};

domain::TweakDefinition definition()
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(u"behavior.registry-tree-test");
    tweak.title = u"Тест ветви"_s;
    tweak.compatibility.architectures = {domain::CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {domain::WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {
        {.id = u"present"_s, .title = u"Есть"_s},
        {.id = u"absent"_s, .title = u"Нет"_s},
    };
    tweak.treeDetection = domain::RegistryTreeDetection{
        .location = {.hive = domain::RegistryHive::CurrentUser,
                     .key = u"Software\\Tweakopedia\\Tree"_s,
                     .view = domain::RegistryView::Registry64},
        .presentState = u"present"_s,
        .missingState = u"absent"_s,
    };
    return tweak;
}

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11, .build = 26200,
            .architecture = domain::CpuArchitecture::X64};
}

} // namespace

class RegistryTreeStateDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void detectsPresentAndMissingTreeWithFingerprint()
    {
        TreeBackend backend;
        backend.snapshot.existed = true;
        const auto present = detection::RegistryTreeStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(present.stateId, u"present"_s);
        QCOMPARE(present.registryTreeFingerprints.size(), 1);
        QVERIFY(!present.fingerprint.isEmpty());

        backend.snapshot.existed = false;
        const auto missing = detection::RegistryTreeStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(missing.stateId, u"absent"_s);
        QVERIFY(!missing.fingerprint.isEmpty());
    }

    void reportsReadFailureAsUnknown()
    {
        TreeBackend backend;
        backend.success = false;
        const auto result = detection::RegistryTreeStateDetector{}.detect(
            definition(), backend, profile());
        QCOMPARE(result.status, domain::DetectionStatus::Unknown);
        QCOMPARE(result.stateId, u"unknown"_s);
    }
};

QTEST_APPLESS_MAIN(RegistryTreeStateDetectorTest)

#include "RegistryTreeStateDetectorTest.moc"
