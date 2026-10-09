#include "detection/ScheduledTaskStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {
namespace {

QByteArray fingerprint(const domain::ScheduledTaskSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

} // namespace

domain::DetectedState ScheduledTaskStateDetector::detect(
    const domain::TweakDefinition& tweak,
    platform::IScheduledTaskBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {.status = domain::DetectionStatus::Unsupported,
                .details = compatibility.explanation};
    }
    if (!tweak.scheduledTaskDetection) {
        return {.status = domain::DetectionStatus::Unknown,
                .details = u"Определение задачи планировщика отсутствует."_s};
    }
    const auto& detection = *tweak.scheduledTaskDetection;
    const auto read = backend.read(detection.location);
    if (read.missing) {
        return {.status = domain::DetectionStatus::Unsupported,
                .details = u"Задача планировщика отсутствует в текущей Windows."_s};
    }
    if (!read.success) {
        return {.status = domain::DetectionStatus::Unknown, .details = read.error};
    }
    const domain::ScheduledTaskSnapshot snapshot{
        .location = detection.location,
        .enabled = read.enabled,
    };
    return {
        .status = domain::DetectionStatus::Named,
        .stateId = read.enabled ? detection.enabledState : detection.disabledState,
        .details = read.enabled ? u"Задача включена."_s : u"Задача выключена."_s,
        .fingerprint = fingerprint(snapshot),
    };
}

} // namespace tweakopedia::detection
