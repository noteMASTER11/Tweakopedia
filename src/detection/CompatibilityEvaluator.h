#pragma once

#include "domain/SystemProfile.h"
#include "domain/TweakDefinition.h"

#include <QString>

namespace tweakopedia::detection {

struct CompatibilityResult {
    bool supported{};
    QString reasonCode;
    QString explanation;
};

[[nodiscard]] CompatibilityResult evaluate(
    const domain::TweakDefinition& tweak,
    const domain::SystemProfile& profile);

} // namespace tweakopedia::detection
