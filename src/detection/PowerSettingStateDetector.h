#pragma once
#include "domain/DetectedState.h"
#include "domain/TweakDefinition.h"
#include "platform/IPowerSettingBackend.h"
namespace tweakopedia::detection {
class PowerSettingStateDetector final
{
public:
    domain::DetectedState detect(const domain::TweakDefinition& tweak,
        const platform::IPowerSettingBackend& backend,
        const domain::SystemProfile& profile) const;
};
}
