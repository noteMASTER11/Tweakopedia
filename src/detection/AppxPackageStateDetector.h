#pragma once

#include "domain/DetectedState.h"
#include "domain/SystemProfile.h"
#include "domain/TweakDefinition.h"
#include "platform/WindowsAppxPackageProvider.h"

namespace tweakopedia::detection {

class AppxPackageStateDetector final
{
public:
    [[nodiscard]] domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const QVector<platform::AppxPackageIdentity>& packages,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::detection
