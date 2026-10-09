#include "detection/BcdStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {
namespace {

QByteArray fingerprint(const domain::BcdElementSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

} // namespace

domain::DetectedState BcdStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const platform::IBcdBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {.status = domain::DetectionStatus::Unsupported,
                .details = compatibility.explanation};
    }
    if (!tweak.bcdDetection) {
        return {.status = domain::DetectionStatus::Unknown,
                .details = u"Определение BCD-элемента отсутствует."_s};
    }
    const auto& detection = *tweak.bcdDetection;
    const auto read = backend.read(detection.spec);
    if (!read.success && !read.missing) {
        return {.status = domain::DetectionStatus::Unknown, .details = read.error};
    }
    const domain::BcdElementSnapshot snapshot{
        .spec = detection.spec, .existed = !read.missing, .value = read.value};
    if (read.missing) {
        return {.status = domain::DetectionStatus::Named,
                .stateId = detection.missingState,
                .details = u"BCD-элемент отсутствует."_s,
                .fingerprint = fingerprint(snapshot)};
    }
    for (auto iterator = detection.valuesByState.cbegin();
         iterator != detection.valuesByState.cend(); ++iterator) {
        if (read.value && *read.value == iterator.value()) {
            return {.status = domain::DetectionStatus::Named,
                    .stateId = iterator.key(),
                    .details = u"Состояние определено по точному значению BCD."_s,
                    .fingerprint = fingerprint(snapshot)};
        }
    }
    return {.status = domain::DetectionStatus::Custom,
            .stateId = u"custom"_s,
            .details = u"Значение BCD не сопоставлено с именованным состоянием."_s,
            .fingerprint = fingerprint(snapshot)};
}

} // namespace tweakopedia::detection
