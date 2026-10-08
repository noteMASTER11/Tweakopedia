#pragma once

#include "platform/IFeatureStoreBackend.h"

namespace tweakopedia::platform {

class WindowsFeatureStoreBackend final : public IFeatureStoreBackend
{
public:
    [[nodiscard]] FeatureQueryResult query(quint32 featureId) const override;
    [[nodiscard]] FeatureMutationResult setUserState(
        quint32 featureId,
        domain::FeatureEnabledState state) override;
    [[nodiscard]] FeatureMutationResult setUserConfiguration(
        const FeatureConfiguration& configuration) override;
    [[nodiscard]] FeatureMutationResult resetUserConfiguration(quint32 featureId) override;
};

} // namespace tweakopedia::platform
