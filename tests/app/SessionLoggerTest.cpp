#include "app/SessionLogger.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia;

class SessionLoggerTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesDebugAndHigherMessagesToTimestampedSessionFile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        app::SessionLogger logger(root.path());

        QVERIFY(logger.setEnabled(true));
        QVERIFY(logger.enabled());
        const auto path = logger.currentFilePath();
        QVERIFY(QRegularExpression(
            u"^Tweakopedia-\\d{8}-\\d{6}-\\d+\\.log$"_s)
                    .match(QFileInfo(path).fileName()).hasMatch());

        qDebug().noquote() << "debug-session-marker";
        qInfo().noquote() << "info-session-marker";
        QTest::ignoreMessage(QtWarningMsg, "warning-session-marker");
        qWarning().noquote() << "warning-session-marker";
        QVERIFY(logger.setEnabled(false));
        QVERIFY(!logger.enabled());

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const auto contents = QString::fromUtf8(file.readAll());
        QVERIFY(contents.contains(u"[DEBUG]"_s));
        QVERIFY(contents.contains(u"debug-session-marker"_s));
        QVERIFY(contents.contains(u"[INFO]"_s));
        QVERIFY(contents.contains(u"info-session-marker"_s));
        QVERIFY(contents.contains(u"[WARNING]"_s));
        QVERIFY(contents.contains(u"warning-session-marker"_s));
    }
};

QTEST_APPLESS_MAIN(SessionLoggerTest)

#include "SessionLoggerTest.moc"
