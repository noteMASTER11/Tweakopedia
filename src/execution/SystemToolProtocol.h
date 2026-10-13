#pragma once

#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <optional>

namespace tweakopedia::execution {

enum class SystemToolOperation { Scan, Restore };
enum class SystemToolStage { ScanHealth, CheckHealth, RestoreHealth, VerifyRestore };
enum class ComponentStoreHealth { Healthy, Repairable, NonRepairable, Unknown };
enum class SystemToolResultStatus { Completed, Failed };

struct SystemToolRequest {
    QUuid requestId;
    SystemToolOperation operation{SystemToolOperation::Scan};
};

struct SystemToolStageEvent {
    QUuid requestId;
    SystemToolStage stage{SystemToolStage::ScanHealth};
};

struct SystemToolOutputEvent {
    QUuid requestId;
    qint64 sequence{};
    QString text;
};

struct SystemToolProgressEvent {
    QUuid requestId;
    SystemToolStage stage{SystemToolStage::ScanHealth};
    int percent{};
};

struct SystemToolResult {
    QUuid requestId;
    SystemToolResultStatus status{SystemToolResultStatus::Failed};
    int exitCode{-1};
    ComponentStoreHealth health{ComponentStoreHealth::Unknown};
    QString logPath;
    QString code;
    QString message;
};

template<typename T>
struct SystemToolDecodeResult {
    std::optional<T> value;
    QString code;
};

class SystemToolProtocol final
{
public:
    [[nodiscard]] static QJsonObject encode(const SystemToolRequest& value);
    [[nodiscard]] static QJsonObject encode(const SystemToolStageEvent& value);
    [[nodiscard]] static QJsonObject encode(const SystemToolOutputEvent& value);
    [[nodiscard]] static QJsonObject encode(const SystemToolProgressEvent& value);
    [[nodiscard]] static QJsonObject encode(const SystemToolResult& value);

    [[nodiscard]] static SystemToolDecodeResult<SystemToolRequest> decodeRequest(
        const QJsonObject& object);
    [[nodiscard]] static SystemToolDecodeResult<SystemToolStageEvent> decodeStage(
        const QJsonObject& object);
    [[nodiscard]] static SystemToolDecodeResult<SystemToolOutputEvent> decodeOutput(
        const QJsonObject& object);
    [[nodiscard]] static SystemToolDecodeResult<SystemToolProgressEvent> decodeProgress(
        const QJsonObject& object);
    [[nodiscard]] static SystemToolDecodeResult<SystemToolResult> decodeResult(
        const QJsonObject& object);

    [[nodiscard]] static QString operationName(SystemToolOperation value);
    [[nodiscard]] static QString stageName(SystemToolStage value);
    [[nodiscard]] static QString healthName(ComponentStoreHealth value);
};

} // namespace tweakopedia::execution

Q_DECLARE_METATYPE(tweakopedia::execution::SystemToolRequest)
Q_DECLARE_METATYPE(tweakopedia::execution::SystemToolStageEvent)
Q_DECLARE_METATYPE(tweakopedia::execution::SystemToolOutputEvent)
Q_DECLARE_METATYPE(tweakopedia::execution::SystemToolProgressEvent)
Q_DECLARE_METATYPE(tweakopedia::execution::SystemToolResult)
