#include "app/FileSystemCheckController.h"
#include "app/ISystemIntegrityDesktopServices.h"
#include "fakes/FakeLocalVolumeProvider.h"
#include "fakes/FakeSystemToolRunner.h"

#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia;

namespace {

class FakeDesktopServices final : public app::ISystemIntegrityDesktopServices
{
public:
    QString dismLogPath() const override { return {}; }
    bool fileExists(const QString& path) const override
    {
        checkedPath = path;
        return exists;
    }
    bool copyText(const QString& text) override
    {
        copiedText = text;
        return copyResult;
    }
    bool openFile(const QString& path) override
    {
        openedPath = path;
        return openResult;
    }

    mutable QString checkedPath;
    QString copiedText;
    QString openedPath;
    bool exists{true};
    bool copyResult{true};
    bool openResult{true};
};

platform::LocalVolume volume(QString target, QString label, bool system = false)
{
    return {.rootPath = target + u"\\"_s,
            .target = std::move(target),
            .label = std::move(label),
            .fileSystem = u"NTFS"_s,
            .totalBytes = 1024ULL * 1024 * 1024 * 500,
            .freeBytes = 1024ULL * 1024 * 1024 * 125,
            .systemVolume = system};
}

execution::SystemToolResult result(execution::SystemToolOutcome outcome, int exitCode)
{
    return {.requestId = QUuid::createUuid(),
            .status = execution::SystemToolResultStatus::Completed,
            .exitCode = exitCode,
            .outcome = outcome,
            .logPath = u"D:/logs/chkdsk.log"_s};
}

} // namespace

class FileSystemCheckControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsVolumesSelectsSystemAndForwardsChosenTarget()
    {
        tests::FakeLocalVolumeProvider provider;
        provider.values = {volume(u"C:"_s, u"Windows"_s, true),
                           volume(u"D:"_s, u"Data"_s)};
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::FileSystemCheckController controller(provider, runner, desktop);

        QCOMPARE(controller.volumes().size(), 2);
        QCOMPARE(controller.selectedVolumeIndex(), 0);
        QVERIFY(controller.volumes().at(0).toMap().value(u"title"_s)
                    .toString().contains(u"Windows"_s));
        QVERIFY(controller.setSelectedVolumeIndex(1));
        QVERIFY(controller.startCheck());
        QCOMPARE(runner.lastTool, execution::SystemToolKind::FileSystem);
        QCOMPARE(runner.lastOperation, execution::SystemToolOperation::Check);
        QCOMPARE(runner.lastTarget, u"D:"_s);
        QCOMPARE(controller.state(), u"awaiting_elevation"_s);
        QVERIFY(!controller.setSelectedVolumeIndex(0));
    }

    void exposesNoVolumeState()
    {
        tests::FakeLocalVolumeProvider provider;
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::FileSystemCheckController controller(provider, runner, desktop);
        QVERIFY(controller.volumes().isEmpty());
        QCOMPARE(controller.selectedVolumeIndex(), -1);
        QVERIFY(!controller.canCheck());
        QVERIFY(!controller.startCheck());
        QCOMPARE(runner.startCalls, 0);
    }

    void mapsOutcomesToUserStates_data()
    {
        QTest::addColumn<execution::SystemToolOutcome>("outcome");
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("state");
        QTest::newRow("healthy") << execution::SystemToolOutcome::Healthy
                                  << 0 << u"healthy"_s;
        QTest::newRow("issues") << execution::SystemToolOutcome::IssuesFound
                                 << 2 << u"issues_found"_s;
        QTest::newRow("incomplete") << execution::SystemToolOutcome::Incomplete
                                     << 3 << u"incomplete"_s;
    }

    void mapsOutcomesToUserStates()
    {
        QFETCH(execution::SystemToolOutcome, outcome);
        QFETCH(int, exitCode);
        QFETCH(QString, state);
        tests::FakeLocalVolumeProvider provider;
        provider.values = {volume(u"C:"_s, u"Windows"_s, true)};
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::FileSystemCheckController controller(provider, runner, desktop);
        QVERIFY(controller.startCheck());
        emit runner.stageChanged({QUuid::createUuid(),
                                  execution::SystemToolStage::CheckFileSystem});
        emit runner.finished(result(outcome, exitCode));
        QCOMPARE(controller.state(), state);
        QCOMPARE(controller.exitCode(), exitCode);
        QCOMPARE(controller.sessionLogPath(), u"D:/logs/chkdsk.log"_s);
        QVERIFY(!controller.resultTitle().isEmpty());
        QVERIFY(controller.canCheck());
    }

    void cancellationKeepsSelectionAndPreviousOutput()
    {
        tests::FakeLocalVolumeProvider provider;
        provider.values = {volume(u"C:"_s, {}), volume(u"D:"_s, {})};
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::FileSystemCheckController controller(provider, runner, desktop);
        QVERIFY(controller.setSelectedVolumeIndex(1));
        QVERIFY(controller.startCheck());
        emit runner.stageChanged({QUuid::createUuid(),
                                  execution::SystemToolStage::CheckFileSystem});
        emit runner.outputReceived({QUuid::createUuid(), 0, u"previous\n"_s});
        emit runner.finished(result(execution::SystemToolOutcome::Healthy, 0));

        QVERIFY(controller.startCheck());
        emit runner.launchCancelled(u"Операция отменена пользователем."_s);
        QCOMPARE(controller.state(), u"idle"_s);
        QCOMPARE(controller.selectedVolumeIndex(), 1);
        QCOMPARE(controller.outputText(), u"previous\n"_s);
        QCOMPARE(controller.errorCode(), u"launch.cancelled"_s);
    }

    void trimsBufferAndDelegatesDesktopActions()
    {
        tests::FakeLocalVolumeProvider provider;
        provider.values = {volume(u"C:"_s, {})};
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::FileSystemCheckController controller(provider, runner, desktop);
        QVERIFY(controller.startCheck());
        emit runner.stageChanged({QUuid::createUuid(),
                                  execution::SystemToolStage::CheckFileSystem});
        emit runner.outputReceived({QUuid::createUuid(), 0,
                                    QString(2 * 1024 * 1024 + 4096, u'x')});
        QVERIFY(controller.outputText().toUtf8().size() <= 2 * 1024 * 1024);
        QVERIFY(controller.outputText().startsWith(u"[Начало вывода удалено"_s));
        emit runner.finished(result(execution::SystemToolOutcome::Healthy, 0));
        QVERIFY(controller.copyOutput());
        QCOMPARE(desktop.copiedText, controller.outputText());
        QVERIFY(controller.openSessionLog());
        QCOMPARE(desktop.openedPath, u"D:/logs/chkdsk.log"_s);
    }
};

QTEST_APPLESS_MAIN(FileSystemCheckControllerTest)
#include "FileSystemCheckControllerTest.moc"
