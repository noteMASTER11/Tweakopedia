#include "detection/AppxPackageStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class AppxPackageStateDetectorTest final : public QObject
{
    Q_OBJECT

private slots:
    void fingerprintsExactInstalledPackage()
    {
        domain::TweakDefinition tweak;
        tweak.appxDetection = domain::AppxPackageDetection{u"Clipchamp.Clipchamp"_s};
        tweak.compatibility = {
            .architectures = {domain::CpuArchitecture::X64},
            .operatingSystems = {domain::WindowsFamily::Windows11},
            .minimumBuild = 22000,
        };
        const QVector<platform::AppxPackageIdentity> packages{
            {u"Microsoft.WindowsStore"_s, u"Microsoft.WindowsStore_1.0_x64__abc"_s},
            {u"clipchamp.clipchamp"_s, u"Clipchamp.Clipchamp_3.0_x64__abc"_s},
        };
        const domain::SystemProfile profile{
            .family = domain::WindowsFamily::Windows11,
            .build = 26200,
            .architecture = domain::CpuArchitecture::X64,
        };

        const auto state = detection::AppxPackageStateDetector{}.detect(tweak, packages, profile);

        QCOMPARE(state.status, domain::DetectionStatus::Named);
        QCOMPARE(state.stateId, u"installed"_s);
        QCOMPARE(state.fingerprint.size(), 64);
        QVERIFY(state.details.contains(u"Clipchamp.Clipchamp_3.0"));
    }

    void reportsMissingPackage()
    {
        domain::TweakDefinition tweak;
        tweak.appxDetection = domain::AppxPackageDetection{u"Clipchamp.Clipchamp"_s};
        tweak.compatibility = {
            .architectures = {domain::CpuArchitecture::X64},
            .operatingSystems = {domain::WindowsFamily::Windows11},
            .minimumBuild = 22000,
        };
        const domain::SystemProfile profile{
            .family = domain::WindowsFamily::Windows11,
            .build = 26200,
            .architecture = domain::CpuArchitecture::X64,
        };

        const auto state = detection::AppxPackageStateDetector{}.detect(tweak, {}, profile);

        QCOMPARE(state.status, domain::DetectionStatus::Named);
        QCOMPARE(state.stateId, u"removed"_s);
        QCOMPARE(state.fingerprint.size(), 64);
    }
};

QTEST_APPLESS_MAIN(AppxPackageStateDetectorTest)

#include "AppxPackageStateDetectorTest.moc"
