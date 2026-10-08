#pragma once

#include "domain/DetectedState.h"
#include "domain/SystemProfile.h"
#include "domain/TweakDefinition.h"
#include "platform/IFeatureStoreBackend.h"

namespace tweakopedia::detection {

class FeatureStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const platform::IFeatureStoreBackend& backend,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
