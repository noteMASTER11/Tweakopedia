#include "execution/FileSystemCheckWorkflow.h"
#include "fakes/FakeDismProcessRunner.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia::execution;

class FileSystemCheckWorkflowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SystemToolStageEvent>();
        qRegisterMetaType<SystemToolResult>();
    }

    void runsExactReadOnlyCommandAndStreamsOutput()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QSignalSpy stages(&workflow, &FileSystemCheckWorkflow::stageChanged);
        QSignalSpy output(&workflow, &FileSystemCheckWorkflow::outputReady);
        QSignalSpy finished(&workflow, &FileSystemCheckWorkflow::finished);
        const SystemToolRequest request{
            QUuid::createUuid(), SystemToolKind::FileSystem,
            SystemToolOperation::Check, u"C:"_s};

        QVERIFY(workflow.start(request));
        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(runner.invocations.at(0).program, u"chkdsk.exe"_s);
        QCOMPARE(runner.invocations.at(0).arguments, QStringList{u"C:"_s});
        runner.emitOutput(0, "Stage 1\r17 percent\r\ncomplete\n");
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QCOMPARE(stages.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolStageEvent>(stages.first().first()).stage,
                 SystemToolStage::CheckFileSystem);
        QVERIFY(output.size() >= 2);
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.outcome, SystemToolOutcome::Healthy);
        QCOMPARE(result.exitCode, 0);
        QVERIFY(QFileInfo::exists(result.logPath));
        QVERIFY(result.logPath.contains(u"Tools/FileSystemCheck"_s));
    }

    void mapsDocumentedReadOnlyExitCodes_data()
    {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<SystemToolOutcome>("outcome");
        QTest::newRow("issues-not-fixed") << 2 << SystemToolOutcome::IssuesFound;
        QTest::newRow("incomplete") << 3 << SystemToolOutcome::Incomplete;
        QTest::newRow("unexpected-fixed") << 1 << SystemToolOutcome::Incomplete;
    }

    void mapsDocumentedReadOnlyExitCodes()
    {
        QFETCH(int, exitCode);
        QFETCH(SystemToolOutcome, outcome);
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &FileSystemCheckWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                SystemToolOperation::Check, u"D:"_s}));
        runner.finish(0, {.termination = DismProcessTermination::Normal,
                          .exitCode = exitCode});
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.outcome, outcome);
        QCOMPARE(result.exitCode, exitCode);
    }

    void rejectsInvalidTargetBeforeLaunching()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                 SystemToolOperation::Check, u"C: /F"_s}));
        QVERIFY(runner.invocations.isEmpty());
    }

    void reportsStartAndUnexpectedExitFailures()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        runner.acceptStart = false;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &FileSystemCheckWorkflow::finished);
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                 SystemToolOperation::Check, u"C:"_s}));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolResult>(finished.first().first()).code,
                 u"chkdsk.runner_busy"_s);

        FakeDismProcessRunner secondRunner;
        FileSystemCheckWorkflow second(secondRunner, logs.path());
        QSignalSpy secondFinished(&second, &FileSystemCheckWorkflow::finished);
        QVERIFY(second.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                              SystemToolOperation::Check, u"C:"_s}));
        secondRunner.finish(0, {.termination = DismProcessTermination::Normal,
                                .exitCode = 87});
        QCOMPARE(qvariant_cast<SystemToolResult>(secondFinished.first().first()).status,
                 SystemToolResultStatus::Failed);
    }
};

QTEST_GUILESS_MAIN(FileSystemCheckWorkflowTest)
#include "FileSystemCheckWorkflowTest.moc"

