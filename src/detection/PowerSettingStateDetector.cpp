#include "detection/PowerSettingStateDetector.h"
#include "detection/CompatibilityEvaluator.h"
#include <QCryptographicHash>
#include <QJsonDocument>
using namespace Qt::StringLiterals;
namespace tweakopedia::detection {
domain::DetectedState PowerSettingStateDetector::detect(
    const domain::TweakDefinition& tweak, const platform::IPowerSettingBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) return {.status = domain::DetectionStatus::Unsupported,
                                          .details = compatibility.explanation};
    if (!tweak.powerDetection) return {.status = domain::DetectionStatus::Unknown,
                                       .details = u"Определение параметра питания отсутствует."_s};
    const auto read = backend.read(tweak.powerDetection->location);
    if (read.missing) return {.status = domain::DetectionStatus::Unsupported,
                              .details = u"Параметр отсутствует в текущей схеме питания."_s};
    if (!read.success) return {.status = domain::DetectionStatus::Unknown, .details = read.error};
    const domain::PowerSettingSnapshot snapshot{tweak.powerDetection->location,
                                                 read.resolvedScheme, read.index};
    const auto fingerprint = QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
    const auto state = tweak.powerDetection->statesByIndex.constFind(read.index);
    if (state == tweak.powerDetection->statesByIndex.cend()) {
        return {.status = domain::DetectionStatus::Custom, .stateId = u"custom"_s,
                .details = u"Индекс не сопоставлен с именованным состоянием."_s,
                .fingerprint = fingerprint};
    }
    return {.status = domain::DetectionStatus::Named, .stateId = *state,
            .details = u"Состояние определено по индексу схемы питания."_s,
            .fingerprint = fingerprint};
}
}
