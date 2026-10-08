#include "platform/WindowsFeatureStoreBackend.h"

#include <QtTest/QTest>

using namespace tweakopedia;

class WindowsFeatureStoreBackendTest final : public QObject
{
    Q_OBJECT
private slots:
    void callsNativeQueryWithoutAssumingFeatureAvailability()
    {
        platform::WindowsFeatureStoreBackend backend;
        const auto result = backend.query(1);
        if (result.success) {
            QVERIFY(result.configuration.has_value());
            QCOMPARE(result.configuration->featureId, 1U);
        } else {
            QVERIFY(!result.error.isEmpty());
        }
    }

    void rejectsZeroFeatureId()
    {
        platform::WindowsFeatureStoreBackend backend;
        const auto result = backend.query(0);
        QVERIFY(!result.success);
    }
};

QTEST_APPLESS_MAIN(WindowsFeatureStoreBackendTest)
#include "WindowsFeatureStoreBackendTest.moc"
