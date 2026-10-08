#include "detection/CompatibilityEvaluator.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {

CompatibilityResult evaluate(
    const domain::TweakDefinition& tweak,
    const domain::SystemProfile& profile)
{
    if (!tweak.compatibility.architectures.contains(profile.architecture)) {
        return {
            .supported = false,
            .reasonCode = u"architecture.unsupported"_s,
            .explanation = u"Архитектура текущей системы не поддерживается этим параметром."_s,
        };
    }
    if (profile.family == domain::WindowsFamily::Unknown) {
        return {
            .supported = false,
            .reasonCode = u"os.unknown"_s,
            .explanation = u"Не удалось определить семейство Windows."_s,
        };
    }
    if (!tweak.compatibility.operatingSystems.contains(profile.family)) {
        return {
            .supported = false,
            .reasonCode = u"os.unsupported"_s,
            .explanation = u"Параметр не объявлен для текущей версии Windows."_s,
        };
    }
    if (profile.build < tweak.compatibility.minimumBuild) {
        return {
            .supported = false,
            .reasonCode = u"build.too_old"_s,
            .explanation = u"Сборка Windows ниже минимальной поддерживаемой."_s,
        };
    }
    if (tweak.compatibility.maximumBuild && profile.build > *tweak.compatibility.maximumBuild) {
        return {
            .supported = false,
            .reasonCode = u"build.too_new"_s,
            .explanation = u"Сборка Windows выше проверенного диапазона параметра."_s,
        };
    }
    return {
        .supported = true,
        .reasonCode = u"supported"_s,
        .explanation = u"Параметр поддерживается текущей системой."_s,
    };
}

} // namespace tweakopedia::detection
