#include "app/SettingsController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace tweakopedia;

namespace {

class FakeSettingsServices final : public app::ISettingsServices
{
public:
    bool debugLoggingEnabled() const override { return enabled; }
    bool setDebugLoggingEnabled(bool value) override
    {
        ++setCalls;
        enabled = value;
        return setResult;
    }
    bool openLogsDirectory() override
    {
        ++openCalls;
        return openResult;
    }

    bool enabled{};
    bool setResult{true};
    bool openResult{true};
    int setCalls{};
    int openCalls{};
};

} // namespace

class SettingsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void togglesLoggingAndPublishesChangedState()
    {
        FakeSettingsServices services;
        app::SettingsController controller(services);
        QSignalSpy changed(&controller, &app::SettingsController::debugLoggingEnabledChanged);

        QVERIFY(!controller.debugLoggingEnabled());
        QVERIFY(controller.setDebugLoggingEnabled(true));
        QVERIFY(controller.debugLoggingEnabled());
        QCOMPARE(services.setCalls, 1);
        QCOMPARE(changed.count(), 1);

        services.setResult = false;
        QVERIFY(!controller.setDebugLoggingEnabled(false));
        QVERIFY(controller.debugLoggingEnabled());
        QCOMPARE(changed.count(), 1);
    }

    void delegatesOpeningLogDirectory()
    {
        FakeSettingsServices services;
        app::SettingsController controller(services);

        QVERIFY(controller.openLogsDirectory());
        QCOMPARE(services.openCalls, 1);
    }
};

QTEST_APPLESS_MAIN(SettingsControllerTest)

#include "SettingsControllerTest.moc"
