#pragma once

#include "domain/DetectedState.h"
#include "domain/SystemProfile.h"
#include "domain/TweakDefinition.h"
#include "platform/IRegistryBackend.h"

namespace tweakopedia::detection {

class RegistryDwordStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const platform::IRegistryBackend& backend,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
