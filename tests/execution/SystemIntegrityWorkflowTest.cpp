#include "execution/SystemIntegrityWorkflow.h"
#include "fakes/FakeDismProcessRunner.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

class SystemIntegrityWorkflowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SystemToolStageEvent>();
        qRegisterMetaType<SystemToolProgressEvent>();
        qRegisterMetaType<SystemToolResult>();
    }

    void scanRunsFixedCommandsAndReturnsTypedHealth()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        SystemIntegrityWorkflow workflow(runner, logs.path());
        QSignalSpy stages(&workflow, &SystemIntegrityWorkflow::stageChanged);
        QSignalSpy output(&workflow, &SystemIntegrityWorkflow::outputReady);
        QSignalSpy progress(&workflow, &SystemIntegrityWorkflow::progressChanged);
        QSignalSpy finished(&workflow, &SystemIntegrityWorkflow::finished);
        const SystemToolRequest request{QUuid::createUuid(),
                                        SystemToolKind::ComponentStore,
                                        SystemToolOperation::Scan, {}};

        QVERIFY(workflow.start(request));
        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(runner.invocations.at(0).program, u"DISM.exe"_s);
        QCOMPARE(runner.invocations.at(0).arguments,
                 QStringList({u"/Online"_s, u"/Cleanup-Image"_s,
                              u"/ScanHealth"_s, u"/English"_s}));
        runner.emitOutput(0, "[===== 43.7% =====]\r");
        QCOMPARE(progress.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolProgressEvent>(progress.first().first()).percent, 43);
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QCOMPARE(runner.invocations.size(), 2);
        QCOMPARE(runner.invocations.at(1).arguments,
                 QStringList({u"/Online"_s, u"/Cleanup-Image"_s,
                              u"/CheckHealth"_s, u"/English"_s}));
        runner.emitOutput(1, "No component store corruption detected.\r\n");
        runner.finish(1, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QCOMPARE(stages.size(), 2);
        QVERIFY(output.size() >= 2); // command banners plus completed text
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.outcome, SystemToolOutcome::Healthy);
        QVERIFY(!result.logPath.isEmpty());
    }

    void restoreRunsVerificationAndRequiresRecognizedResult()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        SystemIntegrityWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &SystemIntegrityWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::ComponentStore,
                                SystemToolOperation::Restore, {}}));
        QCOMPARE(runner.invocations.at(0).arguments.at(2), u"/RestoreHealth"_s);
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});
        QCOMPARE(runner.invocations.at(1).arguments.at(2), u"/CheckHealth"_s);
        runner.emitOutput(1, "The operation completed successfully.\r\n");
        runner.finish(1, {.termination = DismProcessTermination::Normal, .exitCode = 0});
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Failed);
        QCOMPARE(result.code, u"dism.health_unknown"_s);
    }

    void stopsAfterFailedFirstCommand()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        SystemIntegrityWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &SystemIntegrityWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::ComponentStore,
                                SystemToolOperation::Scan, {}}));
        runner.emitOutput(0, "Error: 87\r\n");
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 87});
        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Failed);
        QCOMPARE(result.exitCode, 87);
    }

    void failsBeforeProcessWhenLogCannotOpen()
    {
        QTemporaryDir root;
        QFile blocker(root.filePath(u"blocked"_s));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        FakeDismProcessRunner runner;
        SystemIntegrityWorkflow workflow(runner, blocker.fileName());
        QSignalSpy finished(&workflow, &SystemIntegrityWorkflow::finished);
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::ComponentStore,
                                 SystemToolOperation::Scan, {}}));
        QCOMPARE(runner.invocations.size(), 0);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolResult>(finished.first().first()).code,
                 u"tool.log_open_failed"_s);
    }
};

QTEST_GUILESS_MAIN(SystemIntegrityWorkflowTest)
#include "SystemIntegrityWorkflowTest.moc"
