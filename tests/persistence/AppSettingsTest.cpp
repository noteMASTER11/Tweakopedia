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
};

QTEST_APPLESS_MAIN(AppSettingsTest)

#include "AppSettingsTest.moc"
