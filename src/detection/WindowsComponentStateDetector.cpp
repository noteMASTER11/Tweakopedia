#include "detection/WindowsComponentStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {

domain::DetectedState WindowsComponentStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const platform::IWindowsComponentBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {.status = domain::DetectionStatus::Unsupported,
                .details = compatibility.explanation};
    }
    if (!tweak.windowsComponentDetection) {
        return {.status = domain::DetectionStatus::Unknown,
                .details = u"Определение компонента Windows отсутствует."_s};
    }
    const auto result = backend.query(tweak.windowsComponentDetection->target);
    if (result.isUnsupported) {
        return {.status = domain::DetectionStatus::Unsupported, .details = result.error};
    }
    if (!result.success) {
        return {.status = domain::DetectionStatus::Unknown, .details = result.error};
    }
    const domain::WindowsComponentSnapshot snapshot{
        tweak.windowsComponentDetection->target, result.state};
    const auto fingerprint = QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
    const auto state = tweak.windowsComponentDetection->states.constFind(result.state);
    if (state == tweak.windowsComponentDetection->states.cend()) {
        return {.status = domain::DetectionStatus::Custom,
                .stateId = u"custom"_s,
                .details = u"Состояние компонента не сопоставлено с вариантом твика."_s,
                .fingerprint = fingerprint};
    }
    return {.status = domain::DetectionStatus::Named,
            .stateId = *state,
            .details = u"Состояние определено через DISM."_s,
            .fingerprint = fingerprint};
}

} // namespace tweakopedia::detection
