#include "app/AppController.h"

#include "detection/CompatibilityEvaluator.h"
#include "planning/PlanBuilder.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString restartName(domain::RestartRequirement restart)
{
    switch (restart) {
    case domain::RestartRequirement::None: return u"none"_s;
    case domain::RestartRequirement::Explorer: return u"explorer"_s;
    case domain::RestartRequirement::Service: return u"service"_s;
    case domain::RestartRequirement::SignOut: return u"sign_out"_s;
    case domain::RestartRequirement::Reboot: return u"reboot"_s;
    }
    return {};
}

QString registryObject(const domain::RegistryLocation& location)
{
    return (location.hive == domain::RegistryHive::LocalMachine ? u"HKLM\\"_s : u"HKCU\\"_s)
        + location.key + u"\\"_s + location.valueName;
}

} // namespace

AppController::AppController(IAppServices& services, QObject* parent)
    : QObject(parent), services_(&services), tweaksModel_(this), queueModel_(this), historyModel_(this)
{
}

TweakListModel* AppController::tweaks() noexcept { return &tweaksModel_; }
QueueListModel* AppController::queue() noexcept { return &queueModel_; }
HistoryListModel* AppController::history() noexcept { return &historyModel_; }
QString AppController::previewSummary() const { return preview_ ? preview_->summary : QString{}; }
bool AppController::previewReady() const noexcept { return preview_.has_value() && !preview_->operations.isEmpty(); }
QVariantList AppController::previewOperations() const
{
    QVariantList result;
    if (!preview_) return result;
    for (const auto& variant : preview_->operations) {
        const auto& operation = std::get<planning::PlannedRegistryDwordChange>(variant);
        const auto* tweak = catalog_.find(operation.tweakId);
        result.append(QVariantMap{
            {u"title"_s, tweak ? tweak->title : operation.tweakId.toString()},
            {u"beforeState"_s, detected_.value(operation.tweakId).stateId},
            {u"targetState"_s, operation.targetState},
            {u"registryObject"_s, registryObject(operation.change.location)},
            {u"restart"_s, restartName(operation.restart)},
        });
    }
    return result;
}
QString AppController::lastErrorCode() const { return lastErrorCode_; }
int AppController::applyProgress() const noexcept { return applyProgress_; }
QString AppController::applyStatus() const { return applyStatus_; }
QString AppController::applyMessage() const { return applyMessage_; }

bool AppController::startup()
{
    const auto loaded = services_->loadCatalog();
    if (!loaded.catalog) {
        setError(u"catalog.load_failed"_s,
                 loaded.errors.isEmpty() ? QString{} : loaded.errors.first().message);
        return false;
    }
    catalog_ = *loaded.catalog;
    profile_ = services_->currentProfile();
    refreshDetectedStates();
    refreshModels();
    historyModel_.reset(services_->history());
    setError({});
    return true;
}

bool AppController::selectTarget(const QString& id, const QString& state)
{
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) {
        setError(u"tweak.unknown"_s);
        return false;
    }
    const auto changed = queueData_.setTarget(*tweak, state);
    if (!changed.accepted) {
        setError(changed.errorCode);
        return false;
    }
    preview_.reset();
    (void)tweaksModel_.setTargetState(*parsed, state);
    queueModel_.reset(catalog_, queueData_, detected_);
    emit previewChanged();
    setError({});
    return true;
}

bool AppController::removeFromQueue(const QString& id)
{
    const auto parsed = domain::TweakId::parse(id);
    if (!parsed || !queueData_.remove(*parsed)) return false;
    preview_.reset();
    (void)tweaksModel_.setTargetState(*parsed, {});
    queueModel_.reset(catalog_, queueData_, detected_);
    emit previewChanged();
    return true;
}

QVariantMap AppController::openExplanation(const QString& id) const
{
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) return {};
    const auto& explanation = tweak->explanation;
    QString registryObject;
    if (tweak->detection) {
        const auto& location = tweak->detection->location;
        registryObject = (location.hive == domain::RegistryHive::LocalMachine ? u"HKLM\\"_s : u"HKCU\\"_s)
            + location.key + u"\\"_s + location.valueName;
    }
    return {
        {u"title"_s, tweak->title},
        {u"purpose"_s, explanation.purpose},
        {u"mechanism"_s, explanation.mechanism},
        {u"effect"_s, explanation.effect},
        {u"tradeoffs"_s, explanation.tradeoffs},
        {u"recommendation"_s, explanation.recommendation},
        {u"technicalDetails"_s, explanation.technicalDetails},
        {u"registryObject"_s, registryObject},
        {u"rollback"_s, u"При возврате восстанавливаются точный исходный тип и байты значения; если значения не было, оно удаляется."_s},
    };
}

bool AppController::buildPreview()
{
    const auto result = planning::PlanBuilder{}.build(catalog_, queueData_, detected_, profile_);
    if (!result.plan) {
        setError(result.issues.isEmpty() ? u"plan.failed"_s : result.issues.first().code);
        preview_.reset();
        emit previewChanged();
        return false;
    }
    preview_ = *result.plan;
    emit previewChanged();
    setError({});
    return true;
}

bool AppController::applyQueue(const QString& packageName)
{
    if (packageName.trimmed().isEmpty()) {
        setError(u"package.name_empty"_s);
        return false;
    }
    if (!buildPreview() || !preview_ || preview_->operations.isEmpty()) return false;
    applyProgress_ = 0;
    applyStatus_ = u"running"_s;
    applyMessage_ = u"Запуск Executor"_s;
    emit operationChanged();
    const auto result = services_->apply(
        *preview_,
        packageName.trimmed(),
        [this](int progress, const QString& message) {
            applyProgress_ = progress;
            applyMessage_ = message;
            emit operationChanged();
        });
    if (result.status != AppOperationStatus::Succeeded) {
        applyStatus_ = result.status == AppOperationStatus::Cancelled ? u"cancelled"_s : u"failed"_s;
        applyMessage_ = result.message;
        emit operationChanged();
        setError(result.code, result.message);
        return false;
    }
    applyProgress_ = 100;
    applyStatus_ = u"succeeded"_s;
    applyMessage_ = u"Пакет применён и проверен."_s;
    emit operationChanged();
    const auto queued = queueData_.items();
    for (const auto& item : queued) (void)queueData_.remove(item.tweakId);
    preview_.reset();
    refreshDetectedStates();
    refreshModels();
    historyModel_.reset(services_->history());
    emit previewChanged();
    setError({});
    return true;
}

bool AppController::rollback(const QString& transactionId)
{
    const QUuid id(transactionId);
    if (id.isNull()) {
        setError(u"transaction.invalid_id"_s);
        return false;
    }
    applyProgress_ = 0;
    applyStatus_ = u"running"_s;
    applyMessage_ = u"Запуск возврата"_s;
    emit operationChanged();
    const auto result = services_->rollback(id, [this](int progress, const QString& message) {
        applyProgress_ = progress;
        applyMessage_ = message;
        emit operationChanged();
    });
    if (result.status != AppOperationStatus::Succeeded) {
        applyStatus_ = result.status == AppOperationStatus::Cancelled ? u"cancelled"_s : u"failed"_s;
        applyMessage_ = result.message;
        emit operationChanged();
        setError(result.code, result.message);
        return false;
    }
    applyProgress_ = 100;
    applyStatus_ = u"rolled_back"_s;
    applyMessage_ = u"Исходные значения восстановлены."_s;
    emit operationChanged();
    refreshDetectedStates();
    refreshModels();
    historyModel_.reset(services_->history());
    setError({});
    return true;
}

void AppController::refreshDetectedStates()
{
    detected_.clear();
    supported_.clear();
    for (const auto& tweak : catalog_.tweaks()) {
        const auto compatibility = detection::evaluate(tweak, profile_);
        supported_.insert(tweak.id, compatibility.supported);
        detected_.insert(tweak.id, services_->detect(tweak, profile_));
    }
}

void AppController::refreshModels()
{
    tweaksModel_.reset(catalog_, detected_, supported_);
    for (const auto& item : queueData_.items()) {
        (void)tweaksModel_.setTargetState(item.tweakId, item.targetState);
    }
    queueModel_.reset(catalog_, queueData_, detected_);
}

void AppController::setError(QString code, QString message)
{
    if (lastErrorCode_ == code && lastErrorMessage_ == message) return;
    lastErrorCode_ = std::move(code);
    lastErrorMessage_ = std::move(message);
    emit errorChanged();
}

} // namespace tweakopedia::app
