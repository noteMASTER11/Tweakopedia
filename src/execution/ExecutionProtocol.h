#pragma once

#include "planning/ExecutionPlan.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace tweakopedia::execution {

struct ProtocolError {
    QString code;
    QString message;
};

struct ProtocolDecodeResult {
    std::optional<planning::ExecutionPlan> plan;
    QByteArray bodyHash;
    QVector<ProtocolError> errors;
};

class ExecutionProtocol final
{
public:
    [[nodiscard]] static QByteArray encode(const planning::ExecutionPlan& plan);
    [[nodiscard]] static ProtocolDecodeResult decode(const QByteArray& json);
    [[nodiscard]] static QByteArray bodyHash(const planning::ExecutionPlan& plan);
    [[nodiscard]] static QByteArray canonicalHash(const QJsonObject& body);

private:
    [[nodiscard]] static QJsonObject bodyObject(const planning::ExecutionPlan& plan);
};

} // namespace tweakopedia::execution
