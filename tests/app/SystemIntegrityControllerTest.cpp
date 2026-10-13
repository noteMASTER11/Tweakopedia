#include "app/ISystemIntegrityDesktopServices.h"
#include "app/SystemIntegrityController.h"
#include "fakes/FakeSystemToolRunner.h"

#include <QtTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia;

namespace {

class FakeDesktopServices final : public app::ISystemIntegrityDesktopServices
{
public:
    QString dismLogPath() const override { return logPath; }
    bool fileExists(const QString& path) const override
    {
        lastExistsPath = path;
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

    QString logPath{u"C:/Windows/Logs/DISM/dism.log"_s};
    mutable QString lastExistsPath;
    QString copiedText;
    QString openedPath;
    bool exists{true};
    bool copyResult{true};
    bool openResult{true};
};

execution::SystemToolStageEvent stage(execution::SystemToolStage value)
{
    return {QUuid::createUuid(), value};
}

execution::SystemToolOutputEvent output(QString text)
{
    return {QUuid::createUuid(), 0, std::move(text)};
}

execution::SystemToolResult result(execution::SystemToolOutcome outcome,
                                   int exitCode = 0,
                                   QString logPath = u"D:/logs/session.log"_s)
{
    execution::SystemToolResult value;
    value.requestId = QUuid::createUuid();
    value.status = execution::SystemToolResultStatus::Completed;
    value.exitCode = exitCode;
    value.outcome = outcome;
    value.logPath = std::move(logPath);
    return value;
}

} // namespace

class SystemIntegrityControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void scanTransitionsToHealthy()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QCOMPARE(controller.state(), u"idle"_s);
        QVERIFY(controller.canScan());
        QVERIFY(controller.startScan());
        QCOMPARE(controller.state(), u"awaiting_elevation"_s);
        QVERIFY(controller.busy());
        QVERIFY(!controller.startScan());
        QCOMPARE(runner.startCalls, 1);

        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        QCOMPARE(controller.state(), u"scanning"_s);
        QCOMPARE(controller.stageText(), u"Проверка хранилища компонентов"_s);
        emit runner.progressChanged({QUuid::createUuid(),
                                     execution::SystemToolStage::ScanHealth, 42});
        QVERIFY(controller.progressKnown());
        QCOMPARE(controller.progress(), 42);
        emit runner.stageChanged(stage(execution::SystemToolStage::CheckHealth));
        QCOMPARE(controller.state(), u"checking"_s);
        QVERIFY(!controller.progressKnown());

        emit runner.finished(result(execution::SystemToolOutcome::Healthy));
        QCOMPARE(controller.state(), u"healthy"_s);
        QCOMPARE(controller.resultTitle(), u"Хранилище компонентов исправно"_s);
        QVERIFY(controller.canScan());
        QVERIFY(!controller.canRestore());
        QCOMPARE(controller.exitCode(), 0);
        QCOMPARE(controller.sessionLogPath(), u"D:/logs/session.log"_s);
    }

    void repairableResultCanStartRestoreAndVerify()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        emit runner.finished(result(execution::SystemToolOutcome::Repairable));
        QCOMPARE(controller.state(), u"repairable"_s);
        QVERIFY(controller.canRestore());

        QVERIFY(controller.startRestore());
        QCOMPARE(runner.lastOperation, execution::SystemToolOperation::Restore);
        QCOMPARE(controller.state(), u"awaiting_elevation"_s);
        emit runner.stageChanged(stage(execution::SystemToolStage::RestoreHealth));
        QCOMPARE(controller.state(), u"repairing"_s);
        QCOMPARE(controller.stageText(), u"Восстановление хранилища компонентов"_s);
        emit runner.stageChanged(stage(execution::SystemToolStage::VerifyRestore));
        QCOMPARE(controller.state(), u"verifying_repair"_s);
        QCOMPARE(controller.stageText(), u"Контрольная проверка"_s);
        emit runner.finished(result(execution::SystemToolOutcome::Healthy));
        QCOMPARE(controller.state(), u"healthy"_s);
    }

    void nonRepairableAndFailedResultsExposeDetails()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        emit runner.finished(result(execution::SystemToolOutcome::NonRepairable, 5));
        QCOMPARE(controller.state(), u"non_repairable"_s);
        QVERIFY(controller.resultTitle().contains(u"недоступно"));
        QCOMPARE(controller.exitCode(), 5);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        emit runner.outputReceived(output(u"partial output\n"_s));
        emit runner.failed(u"ipc.disconnected"_s, u"Соединение разорвано"_s);
        QCOMPARE(controller.state(), u"failed"_s);
        QCOMPARE(controller.errorCode(), u"ipc.disconnected"_s);
        QCOMPARE(controller.resultDescription(), u"Соединение разорвано"_s);
        QCOMPARE(controller.outputText(), u"partial output\n"_s);
        QVERIFY(controller.canScan());
    }

    void cancellationReturnsToIdleWithoutDiscardingPreviousResult()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        emit runner.outputReceived(output(u"previous\n"_s));
        emit runner.finished(result(execution::SystemToolOutcome::Healthy));
        const auto title = controller.resultTitle();

        QVERIFY(controller.startScan());
        emit runner.launchCancelled(u"Операция отменена пользователем."_s);
        QCOMPARE(controller.state(), u"idle"_s);
        QCOMPARE(controller.resultTitle(), title);
        QCOMPARE(controller.outputText(), u"previous\n"_s);
        QCOMPARE(controller.errorCode(), u"launch.cancelled"_s);
        QVERIFY(controller.canScan());
    }

    void trimsOnlyTheScreenBuffer()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        const QString large(2 * 1024 * 1024 + 4096, u'x');
        emit runner.outputReceived(output(large));
        QVERIFY(controller.outputText().toUtf8().size() <= 2 * 1024 * 1024);
        QVERIFY(controller.outputText().startsWith(u"[Начало вывода удалено"));
        QVERIFY(controller.outputText().endsWith(QString(128, u'x')));
    }

    void delegatesDesktopActionsAndReportsMissingDismLog()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::SystemIntegrityController controller(runner, desktop);

        QVERIFY(controller.startScan());
        emit runner.stageChanged(stage(execution::SystemToolStage::ScanHealth));
        emit runner.outputReceived(output(u"DISM output"_s));
        QVERIFY(controller.copyOutput());
        QCOMPARE(desktop.copiedText, u"DISM output"_s);
        QVERIFY(controller.openDismLog());
        QCOMPARE(desktop.openedPath, desktop.logPath);

        desktop.exists = false;
        QVERIFY(!controller.openDismLog());
        QCOMPARE(controller.errorCode(), u"dism_log.not_found"_s);
        QVERIFY(controller.resultDescription().contains(u"не найден"));
    }
};

QTEST_APPLESS_MAIN(SystemIntegrityControllerTest)

#include "SystemIntegrityControllerTest.moc"
