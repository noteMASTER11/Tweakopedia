#pragma once

#include "domain/RegistryTypes.h"

#include <QString>
#include <optional>

namespace tweakopedia::platform {

struct FeatureConfiguration {
    quint32 featureId{};
    quint32 priority{};
    domain::FeatureEnabledState state{domain::FeatureEnabledState::Default};
    quint32 enabledStateOptions{};
    quint32 variant{};
    quint32 variantPayloadKind{};
    quint32 variantPayload{};

    friend bool operator==(const FeatureConfiguration&, const FeatureConfiguration&) = default;
};

struct FeatureQueryResult {
    bool success{};
    std::optional<FeatureConfiguration> configuration;
    QString error;
};

struct FeatureMutationResult {
    bool success{};
    QString error;
};

class IFeatureStoreBackend
{
public:
    virtual ~IFeatureStoreBackend() = default;
    [[nodiscard]] virtual FeatureQueryResult query(quint32 featureId) const = 0;
    [[nodiscard]] virtual FeatureMutationResult setUserState(
        quint32 featureId,
        domain::FeatureEnabledState state) = 0;
    [[nodiscard]] virtual FeatureMutationResult setUserConfiguration(
        const FeatureConfiguration& configuration) = 0;
    [[nodiscard]] virtual FeatureMutationResult resetUserConfiguration(quint32 featureId) = 0;
};

} // namespace tweakopedia::platform
