#include "platform/WindowsAppxPackageProvider.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class WindowsAppxPackageProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesPowerShellJsonAndDeduplicatesPackages()
    {
        const QByteArray json = R"json([
          {"Name":"Clipchamp.Clipchamp","PackageFullName":"Clipchamp.Clipchamp_3.0.0.0_x64__abc"},
          {"Name":"Microsoft.WindowsStore","PackageFullName":"Microsoft.WindowsStore_1.0.0.0_x64__abc"},
          {"Name":"clipchamp.clipchamp","PackageFullName":"Clipchamp.Clipchamp_3.0.0.0_x64__abc"}
        ])json";

        const auto result = platform::WindowsAppxPackageProvider::parse(json);

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.packages.size(), 2);
        QCOMPARE(result.packageNames(), QSet<QString>({
            u"Clipchamp.Clipchamp"_s,
            u"Microsoft.WindowsStore"_s,
        }));
    }

    void rejectsMalformedOutput()
    {
        const auto result = platform::WindowsAppxPackageProvider::parse("not-json");

        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.packages.isEmpty());
    }
};

QTEST_APPLESS_MAIN(WindowsAppxPackageProviderTest)

#include "WindowsAppxPackageProviderTest.moc"
