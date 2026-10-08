#include "detection/CompatibilityEvaluator.h"

#include <QtTest/QTest>

using namespace tweakopedia::detection;
using namespace tweakopedia::domain;
using namespace Qt::StringLiterals;

namespace {

TweakDefinition compatibleTweak()
{
    TweakDefinition tweak;
    tweak.id = *TweakId::parse(u"filesystem.win32-long-paths");
    tweak.compatibility.architectures = {CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {WindowsFamily::Windows10, WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 14393;
    return tweak;
}

SystemProfile profile(WindowsFamily family, quint32 build, CpuArchitecture architecture)
{
    return SystemProfile{
        .family = family,
        .build = build,
        .ubr = 1,
        .edition = u"Professional"_s,
        .architecture = architecture,
    };
}

} // namespace

class CompatibilityEvaluatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void supportsWindows10X64()
    {
        const auto result = evaluate(compatibleTweak(), profile(WindowsFamily::Windows10, 19045, CpuArchitecture::X64));

        QVERIFY(result.supported);
        QCOMPARE(result.reasonCode, u"supported"_s);
        QVERIFY(!result.explanation.isEmpty());
    }

    void supportsWindows11X64()
    {
        const auto result = evaluate(compatibleTweak(), profile(WindowsFamily::Windows11, 26200, CpuArchitecture::X64));

        QVERIFY(result.supported);
        QCOMPARE(result.reasonCode, u"supported"_s);
    }

    void rejectsBuildBelowMinimum()
    {
        const auto result = evaluate(compatibleTweak(), profile(WindowsFamily::Windows10, 10240, CpuArchitecture::X64));

        QVERIFY(!result.supported);
        QCOMPARE(result.reasonCode, u"build.too_old"_s);
        QVERIFY(!result.explanation.isEmpty());
    }

    void rejectsArm64()
    {
        const auto result = evaluate(compatibleTweak(), profile(WindowsFamily::Windows11, 26200, CpuArchitecture::Arm64));

        QVERIFY(!result.supported);
        QCOMPARE(result.reasonCode, u"architecture.unsupported"_s);
        QVERIFY(!result.explanation.isEmpty());
    }

    void rejectsUnknownWindowsFamily()
    {
        const auto result = evaluate(compatibleTweak(), profile(WindowsFamily::Unknown, 26200, CpuArchitecture::X64));

        QVERIFY(!result.supported);
        QCOMPARE(result.reasonCode, u"os.unknown"_s);
        QVERIFY(!result.explanation.isEmpty());
    }
};

QTEST_APPLESS_MAIN(CompatibilityEvaluatorTest)

#include "CompatibilityEvaluatorTest.moc"
