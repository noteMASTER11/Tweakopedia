#include "execution/FeatureStateExecutor.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

constexpr quint32 userPriority = 8;

FeatureExecutionResult failure(QString code, QString message)
{
    return {.success = false, .code = std::move(code), .message = std::move(message)};
}

QJsonObject configurationObject(const platform::FeatureConfiguration& value)
{
    return {
        {u"feature_id"_s, static_cast<qint64>(value.featureId)},
        {u"priority"_s, static_cast<qint64>(value.priority)},
        {u"state"_s, static_cast<qint64>(value.state)},
        {u"state_options"_s, static_cast<qint64>(value.enabledStateOptions)},
        {u"variant"_s, static_cast<qint64>(value.variant)},
        {u"payload_kind"_s, static_cast<qint64>(value.variantPayloadKind)},
        {u"payload"_s, static_cast<qint64>(value.variantPayload)},
    };
}

} // namespace

QByteArray FeatureSnapshot::fingerprint() const
{
    return QCryptographicHash::hash(
        QJsonDocument(configurationObject(configuration)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

QJsonObject FeatureSnapshot::toJson() const
{
    return {{u"type"_s, u"feature.configuration"_s},
            {u"configuration"_s, configurationObject(configuration)}};
}

std::optional<FeatureSnapshot> FeatureSnapshot::fromJson(const QJsonObject& object)
{
    if (object.value(u"type"_s).toString() != u"feature.configuration"_s
        || !object.value(u"configuration"_s).isObject()) return std::nullopt;
    const auto value = object.value(u"configuration"_s).toObject();
    const auto id = value.value(u"feature_id"_s).toInteger();
    const auto state = value.value(u"state"_s).toInteger(-1);
    if (id <= 0 || state < 0 || state > 2) return std::nullopt;
    return FeatureSnapshot{.configuration = {
        .featureId = static_cast<quint32>(id),
        .priority = static_cast<quint32>(value.value(u"priority"_s).toInteger()),
        .state = static_cast<domain::FeatureEnabledState>(state),
        .enabledStateOptions = static_cast<quint32>(value.value(u"state_options"_s).toInteger()),
        .variant = static_cast<quint32>(value.value(u"variant"_s).toInteger()),
        .variantPayloadKind = static_cast<quint32>(value.value(u"payload_kind"_s).toInteger()),
        .variantPayload = static_cast<quint32>(value.value(u"payload"_s).toInteger()),
    }};
}

FeatureStateExecutor::FeatureStateExecutor(platform::IFeatureStoreBackend& backend)
    : backend_(&backend)
{
}

FeatureExecutionResult FeatureStateExecutor::capture(quint32 featureId) const
{
    const auto queried = backend_->query(featureId);
    if (!queried.success || !queried.configuration) {
        return failure(u"feature.read_failed"_s,
                       queried.error.isEmpty() ? u"Функция не найдена."_s : queried.error);
    }
    return {.success = true, .snapshot = FeatureSnapshot{*queried.configuration}};
}

FeatureExecutionResult FeatureStateExecutor::compareBefore(
    const FeatureSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (snapshot.fingerprint() != expectedFingerprint) {
        return failure(u"state.changed"_s,
                       u"Состояние функции изменилось после построения плана."_s);
    }
    return {.success = true};
}

FeatureExecutionResult FeatureStateExecutor::apply(
    const domain::SetFeatureStateOperation& operation)
{
    const auto changed = operation.state == domain::FeatureEnabledState::Default
        ? backend_->resetUserConfiguration(operation.featureId)
        : backend_->setUserState(operation.featureId, operation.state);
    if (!changed.success) return failure(u"feature.write_failed"_s, changed.error);

    const auto verified = backend_->query(operation.featureId);
    if (!verified.success || !verified.configuration) {
        return failure(u"feature.verify_failed"_s, u"Не удалось прочитать функцию после изменения."_s);
    }
    const auto matches = operation.state == domain::FeatureEnabledState::Default
        ? verified.configuration->priority != userPriority
        : verified.configuration->state == operation.state;
    if (!matches) {
        return failure(u"feature.verify_failed"_s,
                       u"Контрольное чтение Feature Store не совпало с целевым состоянием."_s);
    }
    return {.success = true};
}

FeatureExecutionResult FeatureStateExecutor::restore(const FeatureSnapshot& snapshot)
{
    const auto restored = snapshot.configuration.priority == userPriority
        ? backend_->setUserConfiguration(snapshot.configuration)
        : backend_->resetUserConfiguration(snapshot.configuration.featureId);
    if (!restored.success) return failure(u"feature.restore_failed"_s, restored.error);

    const auto verified = backend_->query(snapshot.configuration.featureId);
    if (!verified.success || !verified.configuration) {
        return failure(u"feature.restore_verify_failed"_s,
                       u"Не удалось прочитать функцию после возврата."_s);
    }
    if (snapshot.configuration.priority == userPriority
        && *verified.configuration != snapshot.configuration) {
        return failure(u"feature.restore_verify_failed"_s,
                       u"Контрольное чтение функции не совпало со снимком."_s);
    }
    if (snapshot.configuration.priority != userPriority
        && verified.configuration->priority == userPriority) {
        return failure(u"feature.restore_verify_failed"_s,
                       u"Пользовательское переопределение функции не удалено."_s);
    }
    return {.success = true};
}

} // namespace tweakopedia::execution
