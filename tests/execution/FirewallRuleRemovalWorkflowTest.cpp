#include "execution/FirewallRuleRemovalWorkflow.h"
#include "fakes/FakeDismProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia::execution;

namespace {

QString decodedPowerShell(const QStringList& arguments)
{
    const auto encodedIndex = arguments.indexOf(u"-EncodedCommand"_s);
    if (encodedIndex < 0 || encodedIndex + 1 >= arguments.size()) return {};
    const auto bytes = QByteArray::fromBase64(arguments.at(encodedIndex + 1).toLatin1());
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData()),
                              bytes.size() / 2);
}

} // namespace

class FirewallRuleRemovalWorkflowTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SystemToolStageEvent>();
        qRegisterMetaType<SystemToolResult>();
    }

    void exportsPolicyAndTreatsRuleKeyAsBase64Data()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FirewallRuleRemovalWorkflow workflow(runner, logs.path());
        QSignalSpy stages(&workflow, &FirewallRuleRemovalWorkflow::stageChanged);
        QSignalSpy finished(&workflow, &FirewallRuleRemovalWorkflow::finished);
        const auto key = u"rule'; Remove-Item C:\\ -Recurse; #'"_s;
        const SystemToolRequest request{QUuid::createUuid(),
            SystemToolKind::NetworkDiagnostics,
            SystemToolOperation::RemoveFirewallRule, key};

        QVERIFY(workflow.start(request));
        QCOMPARE(runner.invocations.size(), 1);
        const auto& invocation = runner.invocations.constFirst();
        QCOMPARE(QFileInfo(invocation.program).fileName().toLower(), u"powershell.exe"_s);
        QVERIFY(invocation.arguments.contains(u"-OutputFormat"_s));
        const auto script = decodedPowerShell(invocation.arguments);
        QVERIFY(script.contains(u"advfirewall export"_s));
        QVERIFY(script.contains(u"Get-NetFirewallRule -Name"_s));
        QVERIFY(script.contains(u"Remove-NetFirewallRule -Name"_s));
        QVERIFY(script.contains(QString::fromLatin1(key.toUtf8().toBase64())));
        QVERIFY(!script.contains(key));
        QVERIFY(script.indexOf(u"advfirewall export"_s)
                < script.indexOf(u"Remove-NetFirewallRule -Name"_s));

        runner.emitOutput(0, "Rule removed\n");
        runner.finish(0, {.termination = DismProcessTermination::Normal, .exitCode = 0});
        QCOMPARE(stages.size(), 1);
        QCOMPARE(qvariant_cast<SystemToolStageEvent>(stages.first().first()).stage,
                 SystemToolStage::RemoveNetworkFirewallRule);
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Completed);
        QCOMPARE(result.details.value(u"rule_key"_s).toString(), key);
        const auto backupPath = result.details.value(u"backup_path"_s).toString();
        QVERIFY(backupPath.contains(u"Tools/NetworkDiagnostics/Backups"_s));
        QVERIFY(backupPath.endsWith(u".wfw"_s));
    }

    void reportsProcessFailureWithoutClaimingRemoval()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FirewallRuleRemovalWorkflow workflow(runner, logs.path());
        QSignalSpy finished(&workflow, &FirewallRuleRemovalWorkflow::finished);
        QVERIFY(workflow.start({QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
                                SystemToolOperation::RemoveFirewallRule, u"rule-id"_s}));
        runner.finish(0, {.termination = DismProcessTermination::Normal,
                          .exitCode = 1, .error = u"export failed"_s});
        const auto result = qvariant_cast<SystemToolResult>(finished.first().first());
        QCOMPARE(result.status, SystemToolResultStatus::Failed);
        QCOMPARE(result.code, u"firewall_rule.remove_failed"_s);
        QVERIFY(!result.message.isEmpty());
    }

    void rejectsRequestsOutsideTypedOperation()
    {
        QTemporaryDir logs;
        FakeDismProcessRunner runner;
        FirewallRuleRemovalWorkflow workflow(runner, logs.path());
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
                                 SystemToolOperation::Diagnose, {}}));
        QVERIFY(!workflow.start({QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
                                 SystemToolOperation::RemoveFirewallRule, {}}));
        QVERIFY(runner.invocations.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FirewallRuleRemovalWorkflowTest)
#include "FirewallRuleRemovalWorkflowTest.moc"
