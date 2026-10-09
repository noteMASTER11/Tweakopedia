#pragma once

#include "domain/DetectedState.h"
#include "domain/TweakDefinition.h"
#include "platform/IScheduledTaskBackend.h"

namespace tweakopedia::detection {

class ScheduledTaskStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        platform::IScheduledTaskBackend& backend,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
