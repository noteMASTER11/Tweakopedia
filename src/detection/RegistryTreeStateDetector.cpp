#include "detection/RegistryTreeStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {

domain::DetectedState RegistryTreeStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const platform::IRegistryBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {.status = domain::DetectionStatus::Unsupported,
                .stateId = u"unsupported"_s,
                .details = compatibility.explanation};
    }
    if (!tweak.treeDetection) {
        return {.status = domain::DetectionStatus::Unknown,
                .stateId = u"unknown"_s,
                .details = u"В определении отсутствует способ чтения ветви реестра."_s};
    }
    const auto read = backend.readTree(tweak.treeDetection->location);
    if (!read.success) {
        return {.status = domain::DetectionStatus::Unknown,
                .stateId = u"unknown"_s,
                .details = u"Не удалось прочитать ветвь реестра: "_s + read.code};
    }
    const auto fingerprint = QCryptographicHash::hash(
        QJsonDocument(read.snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
    return {
        .status = domain::DetectionStatus::Named,
        .stateId = read.snapshot.existed
            ? tweak.treeDetection->presentState : tweak.treeDetection->missingState,
        .details = read.snapshot.existed
            ? u"Ветвь реестра существует."_s : u"Ветвь реестра отсутствует."_s,
        .fingerprint = fingerprint,
        .registryTreeFingerprints = {{tweak.treeDetection->location, fingerprint}},
    };
}

} // namespace tweakopedia::detection
