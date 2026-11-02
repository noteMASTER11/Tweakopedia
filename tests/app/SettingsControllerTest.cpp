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
    QString appearanceMode() const override { return mode; }
    QString lightThemeId() const override { return lightTheme; }
    QString darkThemeId() const override { return darkTheme; }
    QString accentId() const override { return accent; }
    QString fontId() const override { return font; }
    qreal uiScale() const override { return scale; }
    QString motionMode() const override { return motion; }
    bool systemDarkMode() const override { return systemDark; }
    bool systemReduceMotion() const override { return systemMotion; }
    bool setAppearanceMode(const QString& value) override { mode = value; return appearanceResult; }
    bool setLightThemeId(const QString& value) override { lightTheme = value; return appearanceResult; }
    bool setDarkThemeId(const QString& value) override { darkTheme = value; return appearanceResult; }
    bool setAccentId(const QString& value) override { accent = value; return appearanceResult; }
    bool setFontId(const QString& value) override { font = value; return appearanceResult; }
    bool setUiScale(qreal value) override { scale = value; return appearanceResult; }
    bool setMotionMode(const QString& value) override { motion = value; return appearanceResult; }

    bool enabled{};
    bool setResult{true};
    bool openResult{true};
    int setCalls{};
    int openCalls{};
    QString mode{QStringLiteral("system")};
    QString lightTheme{QStringLiteral("light_cool")};
    QString darkTheme{QStringLiteral("dark_graphite")};
    QString accent{QStringLiteral("blue")};
    QString font{QStringLiteral("segoe")};
    qreal scale{1.0};
    QString motion{QStringLiteral("system")};
    bool systemDark{};
    bool systemMotion{};
    bool appearanceResult{true};
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

    void updatesAppearanceAndComputesEffectiveValues()
    {
        FakeSettingsServices services;
        app::SettingsController controller(services);
        QSignalSpy changed(&controller, &app::SettingsController::appearanceChanged);

        QCOMPARE(controller.effectiveThemeId(), QStringLiteral("light_cool"));
        QVERIFY(!controller.effectiveDark());
        QVERIFY(controller.setDarkThemeId(QStringLiteral("dark_midnight")));
        QVERIFY(controller.setAppearanceMode(QStringLiteral("dark")));
        QVERIFY(controller.setAccentId(QStringLiteral("violet")));
        QVERIFY(controller.setUiScale(1.25));
        QVERIFY(controller.uiScaleRestartRequired());
        QVERIFY(controller.setMotionMode(QStringLiteral("on")));

        QCOMPARE(controller.effectiveThemeId(), QStringLiteral("dark_midnight"));
        QVERIFY(controller.effectiveDark());
        QVERIFY(controller.reduceMotion());
        QCOMPARE(changed.count(), 5);
        QVERIFY(!controller.setUiScale(1.13));
        QVERIFY(!controller.setAccentId(QStringLiteral("unknown")));
        QVERIFY(controller.setLightThemeId(QStringLiteral("light_sage")));
        QVERIFY(controller.setDarkThemeId(QStringLiteral("dark_slate")));
        QVERIFY(!controller.setLightThemeId(QStringLiteral("light_unknown")));
        QVERIFY(!controller.setDarkThemeId(QStringLiteral("dark_unknown")));
        QVERIFY(controller.setUiScale(1.0));
        QVERIFY(!controller.uiScaleRestartRequired());
    }

    void followsSystemThemeInSystemMode()
    {
        FakeSettingsServices services;
        app::SettingsController controller(services);
        QSignalSpy changed(&controller, &app::SettingsController::appearanceChanged);

        controller.setSystemDarkMode(true);
        QVERIFY(controller.effectiveDark());
        QCOMPARE(controller.effectiveThemeId(), QStringLiteral("dark_graphite"));
        QCOMPARE(changed.count(), 1);
    }
};

QTEST_APPLESS_MAIN(SettingsControllerTest)

#include "SettingsControllerTest.moc"
