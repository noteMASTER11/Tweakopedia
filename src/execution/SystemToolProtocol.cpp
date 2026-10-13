#include "execution/SystemToolProtocol.h"

#include <QJsonValue>
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

std::optional<SystemToolOperation> operation(const QString& value)
{
    if (value == u"scan") return SystemToolOperation::Scan;
    if (value == u"restore") return SystemToolOperation::Restore;
    return std::nullopt;
}

std::optional<SystemToolStage> stage(const QString& value)
{
    if (value == u"scan_health") return SystemToolStage::ScanHealth;
    if (value == u"check_health") return SystemToolStage::CheckHealth;
    if (value == u"restore_health") return SystemToolStage::RestoreHealth;
    if (value == u"verify_restore") return SystemToolStage::VerifyRestore;
    return std::nullopt;
}

std::optional<ComponentStoreHealth> health(const QString& value)
{
    if (value == u"healthy") return ComponentStoreHealth::Healthy;
    if (value == u"repairable") return ComponentStoreHealth::Repairable;
    if (value == u"non_repairable") return ComponentStoreHealth::NonRepairable;
    if (value == u"unknown") return ComponentStoreHealth::Unknown;
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

} // namespace

QJsonObject SystemToolProtocol::encode(const SystemToolRequest& value)
{
    return {{u"type"_s, u"tool_request"_s},
            {u"request_id"_s, value.requestId.toString(QUuid::WithoutBraces)},
            {u"tool"_s, u"system_integrity"_s},
            {u"operation"_s, operationName(value.operation)}};
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
            {u"health"_s, healthName(value.health)},
            {u"log_path"_s, value.logPath},
            {u"code"_s, value.code},
            {u"message"_s, value.message}};
}

SystemToolDecodeResult<SystemToolRequest> SystemToolProtocol::decodeRequest(
    const QJsonObject& object)
{
    if (!hasOnly(object, {u"type"_s, u"request_id"_s, u"tool"_s, u"operation"_s})
        || object.value(u"type"_s).toString() != u"tool_request"_s
        || object.value(u"tool"_s).toString() != u"system_integrity"_s) {
        return failure<SystemToolRequest>(u"tool_request.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedOperation = operation(object.value(u"operation"_s).toString());
    if (!id || !parsedOperation) return failure<SystemToolRequest>(u"tool_request.invalid"_s);
    return {.value = SystemToolRequest{*id, *parsedOperation}};
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
                          u"health"_s, u"log_path"_s, u"code"_s, u"message"_s})
        || object.value(u"type"_s).toString() != u"tool_result"_s
        || !object.value(u"exit_code"_s).isDouble()
        || !object.value(u"log_path"_s).isString()) {
        return failure<SystemToolResult>(u"tool_result.invalid"_s);
    }
    const auto id = requestId(object);
    const auto parsedStatus = status(object.value(u"status"_s).toString());
    const auto parsedHealth = health(object.value(u"health"_s).toString());
    if (!id || !parsedStatus || !parsedHealth) {
        return failure<SystemToolResult>(u"tool_result.invalid"_s);
    }
    return {.value = SystemToolResult{
        .requestId = *id,
        .status = *parsedStatus,
        .exitCode = object.value(u"exit_code"_s).toInt(-1),
        .health = *parsedHealth,
        .logPath = object.value(u"log_path"_s).toString(),
        .code = object.value(u"code"_s).toString(),
        .message = object.value(u"message"_s).toString(),
    }};
}

QString SystemToolProtocol::operationName(SystemToolOperation value)
{
    return value == SystemToolOperation::Scan ? u"scan"_s : u"restore"_s;
}

QString SystemToolProtocol::stageName(SystemToolStage value)
{
    switch (value) {
    case SystemToolStage::ScanHealth: return u"scan_health"_s;
    case SystemToolStage::CheckHealth: return u"check_health"_s;
    case SystemToolStage::RestoreHealth: return u"restore_health"_s;
    case SystemToolStage::VerifyRestore: return u"verify_restore"_s;
    }
    return {};
}

QString SystemToolProtocol::healthName(ComponentStoreHealth value)
{
    switch (value) {
    case ComponentStoreHealth::Healthy: return u"healthy"_s;
    case ComponentStoreHealth::Repairable: return u"repairable"_s;
    case ComponentStoreHealth::NonRepairable: return u"non_repairable"_s;
    case ComponentStoreHealth::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

} // namespace tweakopedia::execution
