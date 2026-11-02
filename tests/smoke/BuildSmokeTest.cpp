#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QTest>

class BuildSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void qtTestRuntimeStarts()
    {
        QVERIFY(true);
    }

    void payloadExecutablesHaveContainerDistinctNames()
    {
        QCOMPARE(QFileInfo(QStringLiteral(TWEAKOPEDIA_TEST_GUI)).fileName(),
                 QStringLiteral("Tweakopedia.App.exe"));
        QCOMPARE(QFileInfo(QStringLiteral(TWEAKOPEDIA_TEST_EXECUTOR)).fileName(),
                 QStringLiteral("Tweakopedia.Executor.exe"));
    }

    void guiAcceptsOnlyItsOwnInjectedRuntimeRoot()
    {
        const auto builtGui = QStringLiteral(TWEAKOPEDIA_TEST_GUI);
        QTemporaryDir unicodeRoot(QStringLiteral("D:/ChatGPT/Temp/Тест runtime с пробелом-XXXXXX"));
        QVERIFY(unicodeRoot.isValid());
        const auto gui = QDir(unicodeRoot.path()).filePath(QStringLiteral("Tweakopedia.App.exe"));
        QVERIFY(QFile::copy(builtGui, gui));
        const auto runtimeRoot = QFileInfo(gui).absolutePath();
        auto run = [&](const QString& injectedRoot) {
            QProcess process;
            process.setProgram(gui);
            process.setArguments({QStringLiteral("--self-check"),
                                  QStringLiteral("--no-elevation"),
                                  QStringLiteral("--runtime-root"), injectedRoot,
                                  QStringLiteral("--container-path"), gui});
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
            environment.insert(QStringLiteral("TEMP"), QStringLiteral("D:/ChatGPT/Temp"));
            environment.insert(QStringLiteral("TMP"), QStringLiteral("D:/ChatGPT/Temp"));
            process.setProcessEnvironment(environment);
            process.start();
            if (!process.waitForFinished(15000)) return -1;
            return process.exitCode();
        };

        QCOMPARE(run(runtimeRoot), 0);
        QTemporaryDir other(QStringLiteral("D:/ChatGPT/Temp/Tweakopedia-wrong-runtime-XXXXXX"));
        QVERIFY(other.isValid());
        QCOMPARE(run(other.path()), 5);
    }

    void systemIntegrityPageIsEmbeddedInApplicationResources()
    {
        const QDir sourceRoot(QStringLiteral(TWEAKOPEDIA_TEST_SOURCE_ROOT));
        QFile resources(sourceRoot.filePath(QStringLiteral("apps/tweakopedia/resources.qrc")));
        QVERIFY(resources.open(QIODevice::ReadOnly));
        const auto resourceText = resources.readAll();
        QVERIFY(resourceText.contains("qml/pages/SystemIntegrityPage.qml"));

        QFile page(sourceRoot.filePath(
            QStringLiteral("apps/tweakopedia/qml/pages/SystemIntegrityPage.qml")));
        QVERIFY(page.open(QIODevice::ReadOnly));
        const auto pageText = page.readAll();
        QVERIFY(pageText.contains("systemIntegrityPage"));
        QVERIFY(pageText.contains("systemIntegrityTerminal"));
    }

    void networkDiagnosticsPageIsEmbeddedInApplicationResources()
    {
        const QDir sourceRoot(QStringLiteral(TWEAKOPEDIA_TEST_SOURCE_ROOT));
        QFile resources(sourceRoot.filePath(QStringLiteral("apps/tweakopedia/resources.qrc")));
        QVERIFY(resources.open(QIODevice::ReadOnly));
        const auto resourceText = resources.readAll();
        QVERIFY(resourceText.contains("qml/pages/NetworkDiagnosticsPage.qml"));

        QFile page(sourceRoot.filePath(
            QStringLiteral("apps/tweakopedia/qml/pages/NetworkDiagnosticsPage.qml")));
        QVERIFY(resourceText.contains("qml/pages/TelemetryToolPage.qml"));
        QVERIFY(QFileInfo::exists(QDir(sourceRoot).filePath(
            QStringLiteral("apps/tweakopedia/qml/pages/TelemetryToolPage.qml"))));
        QVERIFY(page.open(QIODevice::ReadOnly));
        const auto pageText = page.readAll();
        QVERIFY(pageText.contains("networkDiagnosticsPage"));
        QVERIFY(pageText.contains("networkDiagnosticsTerminal"));
        QVERIFY(pageText.contains("networkDiagnosticsCheckRepeater"));
    }

    void executorAdvertisesFixtureSystemToolMode()
    {
        QProcess process;
        process.setProgram(QStringLiteral(TWEAKOPEDIA_TEST_EXECUTOR));
        process.setArguments({QStringLiteral("--help")});
        process.start();
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        const auto help = process.readAllStandardOutput() + process.readAllStandardError();
        QVERIFY(help.contains("--test-tool-mode"));
    }
};

QTEST_MAIN(BuildSmokeTest)

#include "BuildSmokeTest.moc"
