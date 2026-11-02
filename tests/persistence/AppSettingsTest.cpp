#include "persistence/AppSettings.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia;

class AppSettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsDebugLoggingPreference()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto path = root.filePath(u"settings.ini"_s);

        persistence::AppSettings initial(path);
        QVERIFY(!initial.debugLoggingEnabled());
        QVERIFY(initial.setDebugLoggingEnabled(true));

        persistence::AppSettings restored(path);
        QVERIFY(restored.debugLoggingEnabled());
        QVERIFY(restored.setDebugLoggingEnabled(false));

        persistence::AppSettings disabled(path);
        QVERIFY(!disabled.debugLoggingEnabled());
    }

    void persistsAppearancePreferences()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto path = root.filePath(u"settings.ini"_s);

        persistence::AppSettings initial(path);
        QCOMPARE(initial.appearanceMode(), u"system"_s);
        QCOMPARE(initial.lightThemeId(), u"light_cool"_s);
        QCOMPARE(initial.darkThemeId(), u"dark_graphite"_s);
        QCOMPARE(initial.accentId(), u"blue"_s);
        QCOMPARE(initial.fontId(), u"segoe"_s);
        QCOMPARE(initial.uiScale(), 1.0);
        QCOMPARE(initial.motionMode(), u"system"_s);

        QVERIFY(initial.setAppearanceMode(u"dark"_s));
        QVERIFY(initial.setLightThemeId(u"light_warm"_s));
        QVERIFY(initial.setDarkThemeId(u"dark_midnight"_s));
        QVERIFY(initial.setAccentId(u"teal"_s));
        QVERIFY(initial.setFontId(u"system"_s));
        QVERIFY(initial.setUiScale(1.25));
        QVERIFY(initial.setMotionMode(u"on"_s));

        persistence::AppSettings restored(path);
        QCOMPARE(restored.appearanceMode(), u"dark"_s);
        QCOMPARE(restored.lightThemeId(), u"light_warm"_s);
        QCOMPARE(restored.darkThemeId(), u"dark_midnight"_s);
        QCOMPARE(restored.accentId(), u"teal"_s);
        QCOMPARE(restored.fontId(), u"system"_s);
        QCOMPARE(restored.uiScale(), 1.25);
        QCOMPARE(restored.motionMode(), u"on"_s);
    }
};

QTEST_APPLESS_MAIN(AppSettingsTest)

#include "AppSettingsTest.moc"
