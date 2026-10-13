#include "execution/SystemToolProtocol.h"

#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

template<typename T>
SystemToolDecodeResult<T> failure(QString code)
{
    return {.code = std::move(code)};
}

bool hasOnly(const QJsonObject& object, const QSet<QString>& allowed)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) return false;
    }
    return true;
}

std::optional<QUuid> requestId(const QJsonObject& object)
{
    if (!object.value(u"request_id"_s).isString()) return std::nullopt;
    const QUuid id(object.value(u"request_id"_s).toString());
    if (id.isNull()) return std::nullopt;
    return id;
}

std::optional<SystemToolKind> tool(const QString& value)
{
    if (value == u"component_store") return SystemToolKind::ComponentStore;
    if (value == u"file_system") return SystemToolKind::FileSystem;
    if (value == u"network_diagnostics") return SystemToolKind::NetworkDiagnostics;
    return std::nullopt;
}

std::optional<SystemToolOperation> operation(const QString& value)
{
    if (value == u"scan") return SystemToolOperation::Scan;
    if (value == u"restore") return SystemToolOperation::Restore;
    if (value == u"check") return SystemToolOperation::Check;
    if (value == u"diagnose") return SystemToolOperation::Diagnose;
    if (value == u"remove_firewall_rule") return SystemToolOperation::RemoveFirewallRule;
    return std::nullopt;
}

std::optional<SystemToolStage> stage(const QString& value)
{
    if (value == u"scan_health") return SystemToolStage::ScanHealth;
    if (value == u"check_health") return SystemToolStage::CheckHealth;
    if (value == u"restore_health") return SystemToolStage::RestoreHealth;
    if (value == u"verify_restore") return SystemToolStage::VerifyRestore;
    if (value == u"check_file_system") return SystemToolStage::CheckFileSystem;
    if (value == u"collect_network_adapters") return SystemToolStage::CollectNetworkAdapters;
    if (value == u"check_network_dns") return SystemToolStage::CheckNetworkDns;
    if (value == u"check_network_proxy") return SystemToolStage::CheckNetworkProxy;
    if (value == u"check_network_hosts") return SystemToolStage::CheckNetworkHosts;
    if (value == u"check_network_routes") return SystemToolStage::CheckNetworkRoutes;
    if (value == u"check_network_firewall") return SystemToolStage::CheckNetworkFirewall;
    if (value == u"remove_network_firewall_rule")
        return SystemToolStage::RemoveNetworkFirewallRule;
    return std::nullopt;
}

std::optional<SystemToolOutcome> outcome(const QString& value)
{
    if (value == u"healthy") return SystemToolOutcome::Healthy;
    if (value == u"repairable") return SystemToolOutcome::Repairable;
    if (value == u"non_repairable") return SystemToolOutcome::NonRepairable;
    if (value == u"issues_found") return SystemToolOutcome::IssuesFound;
    if (value == u"incomplete") return SystemToolOutcome::Incomplete;
    if (value == u"unknown") return SystemToolOutcome::Unknown;
    return std::nullopt;
}

std::optional<SystemToolResultStatus> status(const QString& value)
{
    if (value == u"completed") return SystemToolResultStatus::Completed;
    if (value == u"failed") return SystemToolResultStatus::Failed;
    return std::nullopt;
}

QString resultStatusName(SystemToolResultStatus value)
{
    return value == SystemToolResultStatus::Completed ? u"completed"_s : u"failed"_s;
}

bool validRequest(SystemToolKind kind, SystemToolOperation operation, const QString& target)
{
    if (kind == SystemToolKind::ComponentStore) {
        return target.isEmpty()
            && (operation == SystemToolOperation::Scan
                || operation == SystemToolOperation::Restore);
    }
    if (kind == SystemToolKind::FileSystem) {
        static const QRegularExpression driveTarget(u"^[A-Z]:$"_s);
        return operation == SystemToolOperation::Check
            && driveTarget.match(target).hasMatch();
    }
    if (kind != SystemToolKind::NetworkDiagnostics) return false;
    if (operation == SystemToolOperation::Diagnose) return target.isEmpty();
    return operation == SystemToolOperation::RemoveFirewallRule
        && !target.isEmpty() && target.size() <= 512
        && !target.contains(QChar{}) && !target.contains(u'\r') && !target.contains(u'\n');
}

} // namespace

QJsonObject SystemToolProtocol::encode(const SystemToolRequest& value)
{
    return {{u"type"_s, u"tool_request"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"tool"_s, toolName(value.tool)},
            {u"operation"_s, operationName(value.operation)},
            {u"target"_s, value.target}};
}

QJsonObject SystemToolProtocol::encode(const SystemToolStageEvent& value)
{
    return {{u"type"_s, u"tool_stage"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"stage"_s, stageName(value.stage)}};
}

QJsonObject SystemToolProtocol::encode(const SystemToolOutputEvent& value)
{
    return {{u"type"_s, u"tool_output"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"sequence"_s, value.sequence},
            {u"text"_s, value.text}};
}

QJsonObject SystemToolProtocol::encode(const SystemToolProgressEvent& value)
{
    return {{u"type"_s, u"tool_progress"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"stage"_s, stageName(value.stage)},
            {u"percent"_s, value.percent}};
}

QJsonObject SystemToolProtocol::encode(const SystemToolResult& value)
{
    return {{u"type"_s, u"tool_result"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"status"_s, resultStatusName(value.status)},
            {u"exit_code"_s, value.exitCode},
            {u"outcome"_s, outcomeName(value.outcome)},
            {u"log_path"_s, value.logPath},
            {u"code"_s, value.code},
            {u"message"_s, value.message},
            {u"details"_s, value.details}};
}

SystemToolDecodeResult<SystemToolRequest> SystemToolProtocol::decodeRequest(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"tool"_s,
                          u"operation"_s, u"target"_s})
        || object.value(u"type"_s).toString() != u"tool_request"_s
        || !object.value(u"target"_s).isString()) {
        return failure<SystemToolRequest>(u"tool_request.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedTool = tool(object.value(u"tool"_s).toString());
    const auto parsedOperation = operation(object.value(u"operation"_s).toString());
    const auto target = object.value(u"target"_s).toString();
    if (!id || !parsedTool || !parsedOperation
        || !validRequest(*parsedTool, *parsedOperation, target)) {
        return failure<SystemToolRequest>(u"tool_request.invalid"_s);
    }
    return {.value = SystemToolRequest{*id, *parsedTool, *parsedOperation, target}};
}

SystemToolDecodeResult<SystemToolStageEvent> SystemToolProtocol::decodeStage(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"stage"_s})
        || object.value(u"type"_s).toString() != u"tool_stage"_s) {
        return failure<SystemToolStageEvent>(u"tool_stage.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedStage = stage(object.value(u"stage"_s).toString());
    if (!id || !parsedStage) return failure<SystemToolStageEvent>(u"tool_stage.invalid"_s);
    return {.value = SystemToolStageEvent{*id, *parsedStage}};
}

SystemToolDecodeResult<SystemToolOutputEvent> SystemToolProtocol::decodeOutput(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"sequence"_s, u"text"_s})
        || object.value(u"type"_s).toString() != u"tool_output"_s
        || !object.value(u"sequence"_s).isDouble()
        || !object.value(u"text"_s).isString()) {
        return failure<SystemToolOutputEvent>(u"tool_output.invalid"_s);
    }
    const auto id = requestId(object);
    const auto sequence = object.value(u"sequence"_s).toInteger(-1);
    if (!id || sequence < 0) return failure<SystemToolOutputEvent>(u"tool_output.invalid"_s);
    return {.value = SystemToolOutputEvent{*id, sequence, object.value(u"text"_s).toString()}};
}

SystemToolDecodeResult<SystemToolProgressEvent> SystemToolProtocol::decodeProgress(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"stage"_s, u"percent"_s})
        || object.value(u"type"_s).toString() != u"tool_progress"_s
        || !object.value(u"percent"_s).isDouble()) {
        return failure<SystemToolProgressEvent>(u"tool_progress.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedStage = stage(object.value(u"stage"_s).toString());
    const auto percent = object.value(u"percent"_s).toInt(-1);
    if (!id || !parsedStage || percent < 0 || percent > 100) {
        return failure<SystemToolProgressEvent>(u"tool_progress.invalid"_s);
    }
    return {.value = SystemToolProgressEvent{*id, *parsedStage, percent}};
}

SystemToolDecodeResult<SystemToolResult> SystemToolProtocol::decodeResult(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"status"_s, u"exit_code"_s,
                          u"outcome"_s, u"log_path"_s, u"code"_s, u"message"_s,
                          u"details"_s})
        || object.value(u"type"_s).toString() != u"tool_result"_s
        || !object.value(u"exit_code"_s).isDouble()
        || !object.value(u"log_path"_s).isString()
        || !object.value(u"details"_s).isObject()) {
        return failure<SystemToolResult>(u"tool_result.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedStatus = status(object.value(u"status"_s).toString());
    const auto parsedOutcome = outcome(object.value(u"outcome"_s).toString());
    if (!id || !parsedStatus || !parsedOutcome) {
        return failure<SystemToolResult>(u"tool_result.invalid"_s);
    }
    return {.value = SystemToolResult{
        .requestId = *id,
        .status = *parsedStatus,
        .exitCode = object.value(u"exit_code"_s).toInt(-1),
        .outcome = *parsedOutcome,
        .logPath = object.value(u"log_path"_s).toString(),
        .code = object.value(u"code"_s).toString(),
        .message = object.value(u"message"_s).toString(),
        .details = object.value(u"details"_s).toObject(),
    }};
}

QString SystemToolProtocol::toolName(SystemToolKind value)
{
    switch (value) {
    case SystemToolKind::ComponentStore: return u"component_store"_s;
    case SystemToolKind::FileSystem: return u"file_system"_s;
    case SystemToolKind::NetworkDiagnostics: return u"network_diagnostics"_s;
    }
    return {};
}

QString SystemToolProtocol::operationName(SystemToolOperation value)
{
    switch (value) {
    case SystemToolOperation::Scan: return u"scan"_s;
    case SystemToolOperation::Restore: return u"restore"_s;
    case SystemToolOperation::Check: return u"check"_s;
    case SystemToolOperation::Diagnose: return u"diagnose"_s;
    case SystemToolOperation::RemoveFirewallRule: return u"remove_firewall_rule"_s;
    }
    return {};
}

QString SystemToolProtocol::stageName(SystemToolStage value)
{
    switch (value) {
    case SystemToolStage::ScanHealth: return u"scan_health"_s;
    case SystemToolStage::CheckHealth: return u"check_health"_s;
    case SystemToolStage::RestoreHealth: return u"restore_health"_s;
    case SystemToolStage::VerifyRestore: return u"verify_restore"_s;
    case SystemToolStage::CheckFileSystem: return u"check_file_system"_s;
    case SystemToolStage::CollectNetworkAdapters: return u"collect_network_adapters"_s;
    case SystemToolStage::CheckNetworkDns: return u"check_network_dns"_s;
    case SystemToolStage::CheckNetworkProxy: return u"check_network_proxy"_s;
    case SystemToolStage::CheckNetworkHosts: return u"check_network_hosts"_s;
    case SystemToolStage::CheckNetworkRoutes: return u"check_network_routes"_s;
    case SystemToolStage::CheckNetworkFirewall: return u"check_network_firewall"_s;
    case SystemToolStage::RemoveNetworkFirewallRule:
        return u"remove_network_firewall_rule"_s;
    }
    return {};
}

QString SystemToolProtocol::outcomeName(SystemToolOutcome value)
{
    switch (value) {
    case SystemToolOutcome::Healthy: return u"healthy"_s;
    case SystemToolOutcome::Repairable: return u"repairable"_s;
    case SystemToolOutcome::NonRepairable: return u"non_repairable"_s;
    case SystemToolOutcome::IssuesFound: return u"issues_found"_s;
    case SystemToolOutcome::Incomplete: return u"incomplete"_s;
    case SystemToolOutcome::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

} // namespace tweakopedia::execution
