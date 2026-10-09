#pragma once

#include "domain/DetectedState.h"
#include "domain/TweakDefinition.h"
#include "platform/IBcdBackend.h"

namespace tweakopedia::detection {

class BcdStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const platform::IBcdBackend& backend,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
