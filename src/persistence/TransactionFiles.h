#pragma once

#include "domain/TweakInput.h"

#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <optional>

namespace tweakopedia::persistence {

struct ArtifactMaterializeResult {
    std::optional<domain::InputArtifact> artifact;
    QString code;
};

class TransactionFiles final
{
public:
    explicit TransactionFiles(QString transactionsRoot);

    [[nodiscard]] bool create(const QUuid& id) const;
    [[nodiscard]] QString directory(const QUuid& id) const;
    [[nodiscard]] bool writePlan(const QUuid& id, const QJsonObject& json) const;
    [[nodiscard]] bool writeBefore(const QUuid& id, const QJsonObject& json) const;
    [[nodiscard]] bool writeResult(const QUuid& id, const QJsonObject& json) const;
    [[nodiscard]] std::optional<QJsonObject> readPlan(const QUuid& id) const;
    [[nodiscard]] std::optional<QJsonObject> readBefore(const QUuid& id) const;
    [[nodiscard]] std::optional<QJsonObject> readResult(const QUuid& id) const;
    [[nodiscard]] ArtifactMaterializeResult materializeInput(
        const QUuid& id, const domain::InputArtifact& artifact) const;

private:
    [[nodiscard]] bool write(const QUuid& id, QStringView fileName, const QJsonObject& json) const;
    [[nodiscard]] std::optional<QJsonObject> read(const QUuid& id, QStringView fileName) const;

    QString transactionsRoot_;
};

} // namespace tweakopedia::persistence
