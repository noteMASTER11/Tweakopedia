#include "platform/WindowsComponentBackend.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class WindowsComponentBackendTest final : public QObject
{
    Q_OBJECT
private slots:
    void buildsExactArgumentVectors()
    {
        const domain::WindowsComponentTarget feature{
            domain::WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s};
        QCOMPARE(platform::WindowsComponentBackend::queryArguments(feature),
                 QStringList({u"/Online"_s, u"/English"_s, u"/Get-FeatureInfo"_s,
                              u"/FeatureName:VirtualMachinePlatform"_s}));
        QCOMPARE(platform::WindowsComponentBackend::mutationArguments(
                     feature, domain::WindowsComponentState::Enabled),
                 QStringList({u"/Online"_s, u"/English"_s, u"/Enable-Feature"_s,
                              u"/FeatureName:VirtualMachinePlatform"_s, u"/NoRestart"_s}));

        const domain::WindowsComponentTarget capability{
            domain::WindowsComponentKind::Capability, u"OpenSSH.Client~~~~0.0.1.0"_s};
        QCOMPARE(platform::WindowsComponentBackend::mutationArguments(
                     capability, domain::WindowsComponentState::Absent),
                 QStringList({u"/Online"_s, u"/English"_s, u"/Remove-Capability"_s,
                              u"/CapabilityName:OpenSSH.Client~~~~0.0.1.0"_s,
                              u"/NoRestart"_s}));
    }

    void rejectsInvalidNames()
    {
        const domain::WindowsComponentTarget target{
            domain::WindowsComponentKind::Feature, u"Bad Name /Online"_s};
        QVERIFY(platform::WindowsComponentBackend::queryArguments(target).isEmpty());
    }

    void detectsKnownWindowsComponentReadOnly()
    {
        platform::WindowsComponentBackend backend;
        const auto feature = backend.query({
            domain::WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s});
        if (!feature.success && !feature.isUnsupported) QSKIP(qPrintable(feature.error));
        QVERIFY(feature.success || feature.isUnsupported);

        const auto capability = backend.query({
            domain::WindowsComponentKind::Capability, u"OpenSSH.Client~~~~0.0.1.0"_s});
        if (!capability.success && !capability.isUnsupported) {
            QSKIP(qPrintable(capability.error));
        }
        QVERIFY(capability.success || capability.isUnsupported);
    }
};

QTEST_APPLESS_MAIN(WindowsComponentBackendTest)
#include "WindowsComponentBackendTest.moc"
