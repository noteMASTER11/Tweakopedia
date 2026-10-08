#include "platform/WindowsSystemOverviewProvider.h"

#include <QtTest/QTest>

using namespace tweakopedia;

class WindowsSystemOverviewProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsNativeComputerOverview()
    {
        const auto actual = platform::WindowsSystemOverviewProvider{}.collect();

        QVERIFY2(actual.error.isEmpty(), qPrintable(actual.error));
        QVERIFY(!actual.computerName.isEmpty());
        QVERIFY(!actual.os.caption.isEmpty());
        QVERIFY(actual.os.buildNumber >= 10240);
        QVERIFY(!actual.processor.name.isEmpty());
        QVERIFY(actual.processor.coreCount > 0);
        QVERIFY(actual.memory.totalBytes > 0);
        QVERIFY(!actual.graphics.isEmpty());
        QVERIFY(!actual.disks.isEmpty());
    }
};

QTEST_APPLESS_MAIN(WindowsSystemOverviewProviderTest)

#include "WindowsSystemOverviewProviderTest.moc"
