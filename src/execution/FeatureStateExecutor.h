#pragma once

#include "platform/IFeatureStoreBackend.h"

#include <QByteArray>
#include <QJsonObject>

namespace tweakopedia::execution {

struct FeatureSnapshot {
    platform::FeatureConfiguration configuration;

    [[nodiscard]] QByteArray fingerprint() const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<FeatureSnapshot> fromJson(const QJsonObject& object);
};

struct FeatureExecutionResult {
    bool success{};
    QString code;
    QString message;
    FeatureSnapshot snapshot;
};

class FeatureStateExecutor final
{
public:
    explicit FeatureStateExecutor(platform::IFeatureStoreBackend& backend);

    [[nodiscard]] FeatureExecutionResult capture(quint32 featureId) const;
    [[nodiscard]] FeatureExecutionResult compareBefore(
        const FeatureSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] FeatureExecutionResult apply(const domain::SetFeatureStateOperation& operation);
    [[nodiscard]] FeatureExecutionResult restore(const FeatureSnapshot& snapshot);

private:
    platform::IFeatureStoreBackend* backend_{};
};

} // namespace tweakopedia::execution
