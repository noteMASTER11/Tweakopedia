#include "execution/FirewallRuleRemovalWorkflow.h"

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QSysInfo>

#include <windows.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

QString powerShellPath()
{
    wchar_t directory[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(directory, MAX_PATH + 1);
    const auto systemDirectory = length > 0 && length <= MAX_PATH
        ? QString::fromWCharArray(directory, static_cast<qsizetype>(length))
        : QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot", "C:\\Windows")
                                   + u"/System32"_s);
    return QDir(systemDirectory).filePath(u"WindowsPowerShell/v1.0/powershell.exe"_s);
}

QString base64Utf8(const QString& value)
{
    return QString::fromLatin1(value.toUtf8().toBase64());
}

QString encodedScript(const QString& ruleKey, const QString& backupPath)
{
    const auto script = QStringLiteral(R"PS(
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$ruleName = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('%1'))
$backupPath = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('%2'))

Write-Output 'Создание резервной копии политики Windows Firewall...'
& "$env:SystemRoot\System32\netsh.exe" advfirewall export $backupPath
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
    throw 'Не удалось экспортировать текущую политику Windows Firewall.'
}

$rules = @(Get-NetFirewallRule -Name $ruleName -ErrorAction Stop)
if ($rules.Count -ne 1) {
    throw ("Ожидалось одно правило с указанным внутренним именем, найдено: {0}." -f $rules.Count)
}

Write-Output ("Удаление правила: {0}" -f $rules[0].DisplayName)
Remove-NetFirewallRule -Name $ruleName -Confirm:$false -ErrorAction Stop
if (Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue) {
    throw 'Правило по-прежнему найдено после команды удаления.'
}
Write-Output 'Правило Windows Firewall удалено.'
)PS").arg(base64Utf8(ruleKey), base64Utf8(QDir::toNativeSeparators(backupPath)));
    const QByteArray utf16(
        reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}

bool validRuleKey(const QString& value)
{
    return !value.isEmpty() && value.size() <= 512
        && !value.contains(QChar{}) && !value.contains(u'\r') && !value.contains(u'\n');
}

} // namespace

FirewallRuleRemovalWorkflow::FirewallRuleRemovalWorkflow(
    IDismProcessRunner& runner,
    QString logsRoot,
    QObject* parent)
    : QObject(parent), runner_(&runner), logsRoot_(std::move(logsRoot))
{
}

bool FirewallRuleRemovalWorkflow::start(const SystemToolRequest& request)
{
    if (active_ || request.requestId.isNull()
        || request.tool != SystemToolKind::NetworkDiagnostics
        || request.operation != SystemToolOperation::RemoveFirewallRule
        || !validRuleKey(request.target)) {
        return false;
    }
    request_ = request;
    if (!log_.open(logsRoot_, request.tool, request.operation,
                   {u"Windows: "_s + QSysInfo::prettyProductName(),
                    u"Architecture: "_s + QSysInfo::currentCpuArchitecture()})) {
        emit finished({.requestId = request.requestId,
                       .status = SystemToolResultStatus::Failed,
                       .code = u"tool.log_open_failed"_s,
                       .message = log_.errorString()});
        return false;
    }

    const auto backupDirectory = QDir(logsRoot_).filePath(
        u"Tools/NetworkDiagnostics/Backups"_s);
    if (!QDir{}.mkpath(backupDirectory)) {
        log_.close();
        emit finished({.requestId = request.requestId,
                       .status = SystemToolResultStatus::Failed,
                       .code = u"firewall_rule.backup_directory_failed"_s,
                       .message = u"Не удалось создать каталог резервных копий."_s});
        return false;
    }
    backupPath_ = QDir(backupDirectory).filePath(
        QDateTime::currentDateTime().toString(u"yyyy-MM-dd_HH-mm-ss-zzz"_s)
        + u"_firewall-policy.wfw"_s);
    decoder_.resetState();
    active_ = true;

    const QStringList arguments{
        u"-NoLogo"_s, u"-NoProfile"_s, u"-NonInteractive"_s,
        u"-ExecutionPolicy"_s, u"Bypass"_s,
        u"-OutputFormat"_s, u"Text"_s,
        u"-EncodedCommand"_s, encodedScript(request.target, backupPath_),
    };
    log_.appendCommand(u"powershell.exe"_s,
                       {u"-NoLogo"_s, u"-NoProfile"_s, u"-NonInteractive"_s,
                        u"-ExecutionPolicy"_s, u"Bypass"_s,
                        u"-OutputFormat"_s, u"Text"_s,
                        u"-EncodedCommand"_s, u"<embedded>"_s});
    emit stageChanged({request.requestId, SystemToolStage::RemoveNetworkFirewallRule});
    emit outputReady(request.requestId,
                     u"> Резервирование политики и удаление выбранного правила\n"_s);
    if (!runner_->start(
            powerShellPath(), arguments,
            [this](const QByteArray& bytes) { consumeOutput(bytes); },
            [this](DismProcessResult result) { processFinished(std::move(result)); })) {
        complete({.requestId = request.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .code = u"firewall_rule.runner_busy"_s,
                  .message = u"Не удалось запустить удаление правила."_s});
    }
    return active_;
}

bool FirewallRuleRemovalWorkflow::active() const noexcept
{
    return active_;
}

void FirewallRuleRemovalWorkflow::consumeOutput(const QByteArray& bytes)
{
    log_.appendOutput(bytes);
    const QString text = decoder_(bytes);
    if (!text.isEmpty()) emit outputReady(request_.requestId, text);
}

void FirewallRuleRemovalWorkflow::processFinished(DismProcessResult result)
{
    const QString tail = decoder_({});
    if (!tail.isEmpty()) emit outputReady(request_.requestId, tail);
    if (result.termination != DismProcessTermination::Normal || result.exitCode != 0) {
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = u"firewall_rule.remove_failed"_s,
                  .message = result.error.isEmpty()
                      ? u"Не удалось удалить выбранное правило Windows Firewall."_s
                      : result.error,
                  .details = {{u"rule_key"_s, request_.target},
                              {u"backup_path"_s, backupPath_}}});
        return;
    }
    complete({.requestId = request_.requestId,
              .status = SystemToolResultStatus::Completed,
              .exitCode = 0,
              .outcome = SystemToolOutcome::Healthy,
              .message = u"Правило Windows Firewall удалено."_s,
              .details = {{u"rule_key"_s, request_.target},
                          {u"backup_path"_s, backupPath_}}});
}

void FirewallRuleRemovalWorkflow::complete(SystemToolResult result)
{
    if (!active_) return;
    active_ = false;
    log_.close();
    result.logPath = log_.path();
    emit finished(result);
}

} // namespace tweakopedia::execution
