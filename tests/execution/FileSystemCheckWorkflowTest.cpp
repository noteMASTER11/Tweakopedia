#include "execution/FileSystemCheckWorkflow.h"
#include "fakes/FakeDismProcessRunner.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <windows.h>

using namespace Qt::StringLiterals;
using namespace tweakopedia::execution;

namespace {

QByteArray encodeWindowsOem(const QString& text)
{
    const auto required = WideCharToMultiByte(
        GetOEMCP(), 0, reinterpret_cast<const wchar_t*>(text.utf16()),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    QByteArray encoded(required, Qt::Uninitialized);
    WideCharToMultiByte(
        GetOEMCP(), 0, reinterpret_cast<const wchar_t*>(text.utf16()),
        static_cast<int>(text.size()), encoded.data(), required, nullptr, nullptr);
    return encoded;
}

} // namespace

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
        const QFileInfo executable(runner.invocations.at(0).program);
        QVERIFY(executable.isAbsolute());
        QCOMPARE(executable.fileName().toLower(), u"chkdsk.exe"_s);
        QCOMPARE(QFileInfo(executable.absolutePath()).fileName().toLower(), u"system32"_s);
        QCOMPARE(runner.invocations.at(0).arguments, QStringList{u"C:"_s});
        runner.emitOutput(0, "Stage 1\r17 percent\r\ncomplete\n");
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QCOMPARE(stages.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolStageEvent>(stages.first().first()).stage,
                 SystemToolStage::CheckFileSystem);
        QVERIFY(output.size() >= 2);
        QString streamed;
        for (const auto& event : output) streamed += event.at(1).toString();
        QVERIFY(streamed.contains(u"Stage 1\r17 percent\r\ncomplete\n"_s));
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

    void preservesMultibyteCharactersSplitAcrossChunks()
    {
        QString expected;
        switch (GetOEMCP()) {
        case 932: expected = u"検査完了\n"_s; break;
        case 936: expected = u"检查完成\n"_s; break;
        case 949: expected = u"검사 완료\n"_s; break;
        case 950: expected = u"檢查完成\n"_s; break;
        case CP_UTF8: expected = u"Проверка завершена\n"_s; break;
        default: QSKIP("Текущая OEM-кодировка не является многобайтовой.");
        }
        const auto encoded = encodeWindowsOem(expected);
        qsizetype split = -1;
        if (GetOEMCP() == CP_UTF8) {
            for (qsizetype index = 1; index < encoded.size(); ++index) {
                if ((static_cast<unsigned char>(encoded.at(index)) & 0xc0U) == 0x80U) {
                    split = index;
                    break;
                }
            }
        } else {
            for (qsizetype index = 0; index + 1 < encoded.size(); ++index) {
                if (IsDBCSLeadByteEx(GetOEMCP(), static_cast<BYTE>(encoded.at(index)))) {
                    split = index + 1;
                    break;
                }
            }
        }
        if (split < 0) {
            QSKIP("Не удалось найти границу многобайтового символа.");
        }

        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QSignalSpy output(&workflow, &FileSystemCheckWorkflow::outputReady);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                SystemToolOperation::Check, u"C:"_s}));
        runner.emitOutput(0, encoded.first(split));
        runner.emitOutput(0, encoded.sliced(split));
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QString combined;
        for (const auto& event : output) combined += event.at(1).toString();
        QVERIFY(combined.contains(expected));
    }

    void decodesChkdskOutputUsingWindowsOemCodePage()
    {
        if (GetACP() == GetOEMCP()) {
            QSKIP("ANSI and OEM code pages are identical on this system.");
        }

        const auto expected = u"Проверка файловой системы завершена.\n"_s;
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FileSystemCheckWorkflow workflow(runner, logs.path());
        QSignalSpy output(&workflow, &FileSystemCheckWorkflow::outputReady);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                SystemToolOperation::Check, u"C:"_s}));

        runner.emitOutput(0, encodeWindowsOem(expected));
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QString combined;
        for (const auto& event : output) combined += event.at(1).toString();
        QVERIFY2(combined.contains(expected), qPrintable(combined));
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
