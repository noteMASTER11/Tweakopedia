#include "execution/NetworkDiagnosticsWorkflow.h"
#include "fakes/FakeDismProcessRunner.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia::execution;

namespace {

QByteArray marker(const QJsonObject& object)
{
    return "@@TWEAKOPEDIA@@" + QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject check(
    const QString& id,
    const QString& title,
    const QString& status,
    const QString& summary)
{
    return {{u"type"_s, u"check"_s}, {u"id"_s, id}, {u"title"_s, title},
            {u"status"_s, status}, {u"summary"_s, summary},
            {u"details"_s, QJsonArray{u"Подробность"_s}}};
}

} // namespace

class NetworkDiagnosticsWorkflowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SystemToolStageEvent>();
        qRegisterMetaType<SystemToolResult>();
    }

    void runsFixedPowerShellAuditAndReturnsStructuredChecks()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        NetworkDiagnosticsWorkflow workflow(runner, logs.path());
        QSignalSpy stages(&workflow, &NetworkDiagnosticsWorkflow::stageChanged);
        QSignalSpy output(&workflow, &NetworkDiagnosticsWorkflow::outputReady);
        QSignalSpy finished(&workflow, &NetworkDiagnosticsWorkflow::finished);
        const SystemToolRequest request{QUuid::createUuid(),
            SystemToolKind::NetworkDiagnostics, SystemToolOperation::Diagnose, {}};

        QVERIFY(workflow.start(request));
        QCOMPARE(runner.invocations.size(), 1);
        const auto& invocation = runner.invocations.constFirst();
        QCOMPARE(QFileInfo(invocation.program).fileName().toLower(), u"powershell.exe"_s);
        QCOMPARE(invocation.arguments.first(5), QStringList({
            u"-NoLogo"_s, u"-NoProfile"_s, u"-NonInteractive"_s,
            u"-ExecutionPolicy"_s, u"Bypass"_s}));
        QCOMPARE(invocation.arguments.at(5), u"-EncodedCommand"_s);
        QVERIFY(!invocation.arguments.at(6).isEmpty());

        runner.emitOutput(0, "Network diagnostics\n");
        const auto stageMarker = marker({{u"type"_s, u"stage"_s},
                                         {u"id"_s, u"adapters"_s}});
        runner.emitOutput(0, stageMarker.first(9));
        runner.emitOutput(0, stageMarker.sliced(9));
        const QList<QJsonObject> checks{
            check(u"adapters"_s, u"Сетевые адаптеры"_s, u"ok"_s, u"Найден активный адаптер"_s),
            check(u"dns"_s, u"DNS"_s, u"attention"_s, u"Разрешение имён не выполнено"_s),
            check(u"proxy"_s, u"Прокси"_s, u"ok"_s, u"Прокси не задан"_s),
            check(u"hosts"_s, u"Файл hosts"_s, u"ok"_s, u"Нестандартных строк нет"_s),
            check(u"routes"_s, u"Маршруты"_s, u"ok"_s, u"Маршрут по умолчанию найден"_s),
            check(u"firewall"_s, u"Windows Firewall"_s, u"unavailable"_s, u"Cmdlet недоступен"_s),
        };
        for (const auto& item : checks) runner.emitOutput(0, marker(item));
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        QCOMPARE(stages.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolStageEvent>(stages.first().first()).stage,
                 SystemToolStage::CollectNetworkAdapters);
        QString displayed;
        for (const auto& event : output) displayed += event.at(1).toString();
        QVERIFY(displayed.contains(u"Network diagnostics"_s));
        QVERIFY(!displayed.contains(u"@@TWEAKOPEDIA@@"_s));
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.outcome, SystemToolOutcome::IssuesFound);
        QCOMPARE(result.details.value(u"checks"_s).toArray().size(), 6);
        QCOMPARE(result.details.value(u"ok_count"_s).toInt(), 4);
        QCOMPARE(result.details.value(u"attention_count"_s).toInt(), 1);
        QCOMPARE(result.details.value(u"unavailable_count"_s).toInt(), 1);
        QVERIFY(result.logPath.contains(u"Tools/NetworkDiagnostics"_s));
    }

    void fillsMissingChecksAsUnavailable()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        NetworkDiagnosticsWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &NetworkDiagnosticsWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
                                SystemToolOperation::Diagnose, {}}));
        runner.emitOutput(0, marker(check(u"adapters"_s, u"Сетевые адаптеры"_s,
                                          u"ok"_s, u"Готово"_s)));
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});

        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.outcome, SystemToolOutcome::Incomplete);
        QCOMPARE(result.details.value(u"checks"_s).toArray().size(), 6);
        QCOMPARE(result.details.value(u"unavailable_count"_s).toInt(), 5);
    }

    void rejectsWrongRequestAndReportsProcessFailure()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        NetworkDiagnosticsWorkflow workflow(runner, logs.path());
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::FileSystem,
                                 SystemToolOperation::Check, u"C:"_s}));
        QVERIFY(runner.invocations.isEmpty());

        QSignalSpy finished(&workflow, &NetworkDiagnosticsWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
                                SystemToolOperation::Diagnose, {}}));
        runner.finish(0, {.termination = DismProcessTermination::Normal,
                          .exitCode = 1, .error = u"PowerShell error"_s});
        QCOMPARE(qvariant_cast<SystemToolResult>(finished.first().first()).status,
                 SystemToolResultStatus::Failed);
    }
};

QTEST_GUILESS_MAIN(NetworkDiagnosticsWorkflowTest)
#include "NetworkDiagnosticsWorkflowTest.moc"
