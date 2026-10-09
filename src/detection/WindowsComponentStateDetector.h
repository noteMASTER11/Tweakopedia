#pragma once

#include "domain/DetectedState.h"
#include "domain/TweakDefinition.h"
#include "platform/IWindowsComponentBackend.h"

namespace tweakopedia::detection {

class WindowsComponentStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const platform::IWindowsComponentBackend& backend,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
