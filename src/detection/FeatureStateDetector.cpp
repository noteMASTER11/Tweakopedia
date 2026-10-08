#include "detection/FeatureStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {
namespace {

QByteArray fingerprint(const platform::FeatureConfiguration& configuration)
{
    const QJsonObject object{
        {u"feature_id"_s, static_cast<qint64>(configuration.featureId)},
        {u"priority"_s, static_cast<qint64>(configuration.priority)},
        {u"state"_s, static_cast<qint64>(configuration.state)},
        {u"state_options"_s, static_cast<qint64>(configuration.enabledStateOptions)},
        {u"variant"_s, static_cast<qint64>(configuration.variant)},
        {u"payload_kind"_s, static_cast<qint64>(configuration.variantPayloadKind)},
        {u"payload"_s, static_cast<qint64>(configuration.variantPayload)},
    };
    return QCryptographicHash::hash(
        QJsonDocument(object).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

QString stateId(domain::FeatureEnabledState state)
{
    switch (state) {
    case domain::FeatureEnabledState::Default: return u"default"_s;
    case domain::FeatureEnabledState::Disabled: return u"disabled"_s;
    case domain::FeatureEnabledState::Enabled: return u"enabled"_s;
    }
    return u"unknown"_s;
}

} // namespace

domain::DetectedState FeatureStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const platform::IFeatureStoreBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {.status = domain::DetectionStatus::Unsupported,
                .stateId = u"unsupported"_s,
                .details = compatibility.explanation};
    }
    if (!tweak.featureDetection) {
        return {.status = domain::DetectionStatus::Unknown,
                .stateId = u"unknown"_s,
                .details = u"В определении отсутствует Feature ID."_s};
    }
    const auto queried = backend.query(tweak.featureDetection->featureId);
    if (!queried.success || !queried.configuration) {
        return {.status = domain::DetectionStatus::Unsupported,
                .stateId = u"unsupported"_s,
                .details = queried.error.isEmpty()
                    ? u"Функция отсутствует в текущей сборке Windows."_s
                    : queried.error};
    }
    const auto& configuration = *queried.configuration;
    constexpr quint32 userPriority = 8;
    const auto effectiveState = stateId(configuration.state);
    return {.status = domain::DetectionStatus::Named,
            .stateId = configuration.priority == userPriority ? effectiveState : u"default"_s,
            .details = u"Feature ID %1, приоритет %2, эффективное состояние: %3."_s
                .arg(configuration.featureId).arg(configuration.priority, 0, 10).arg(effectiveState),
            .fingerprint = fingerprint(configuration)};
}

} // namespace tweakopedia::detection
