#include "app/AppController.h"

#include "detection/CompatibilityEvaluator.h"
#include "planning/PlanBuilder.h"
#include "app/SystemOverviewPresenter.h"

#include <QtConcurrentRun>
#include <QSet>
#include <algorithm>
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
    : QObject(parent)
    , services_(&services)
    , tweaksModel_(this)
    , filteredTweaksModel_(this)
    , categoriesModel_(this)
    , queueModel_(this)
    , historyModel_(this)
    , encyclopediaController_(this)
{
    filteredTweaksModel_.setSourceModel(&tweaksModel_);
    connect(&filteredTweaksModel_,
            &TweakFilterProxyModel::hideUnsupportedChanged,
            this,
            &AppController::refreshCategories);
    connect(&systemOverviewWatcher_,
            &QFutureWatcher<domain::SystemOverviewSnapshot>::finished,
            this,
            [this] {
                const auto snapshot = systemOverviewWatcher_.result();
                systemOverview_ = SystemOverviewPresenter::present(snapshot);
                systemOverviewError_ = snapshot.error;
                systemOverviewLoading_ = false;
                emit systemOverviewChanged();
            });
    connect(&appRemovalScanWatcher_,
            &QFutureWatcher<content::CatalogLoadResult>::finished,
            this,
            [this] {
                const auto result = appRemovalScanWatcher_.result();
                if (!result.catalog) {
                    appRemovalScanStatus_ = u"failed"_s;
                    appRemovalScanError_ = result.errors.isEmpty()
                        ? u"Не удалось определить установленные приложения."_s
                        : result.errors.first().message;
                    emit appRemovalScanChanged();
                    return;
                }

                auto definitions = catalog_.tweaks();
                definitions.erase(
                    std::remove_if(definitions.begin(), definitions.end(), [](const auto& tweak) {
                        return tweak.category == u"app-removal"_s;
                    }),
                    definitions.end());
                definitions += result.catalog->tweaks();
                catalog_ = content::TweakCatalog(std::move(definitions));
                refreshDetectedStates();
                refreshModels();
                refreshCategories();
                encyclopediaController_.reset(catalog_, categoryCatalog_);
                appRemovalScanStatus_ = u"succeeded"_s;
                appRemovalScanError_.clear();
                emit appRemovalScanChanged();
            });
}

AppController::~AppController()
{
    if (systemOverviewWatcher_.isRunning()) {
        systemOverviewWatcher_.waitForFinished();
    }
    if (appRemovalScanWatcher_.isRunning()) {
        appRemovalScanWatcher_.waitForFinished();
    }
}

TweakListModel* AppController::tweaks() noexcept { return &tweaksModel_; }
TweakFilterProxyModel* AppController::filteredTweaks() noexcept { return &filteredTweaksModel_; }
CategoryListModel* AppController::categories() noexcept { return &categoriesModel_; }
QueueListModel* AppController::queue() noexcept { return &queueModel_; }
HistoryListModel* AppController::history() noexcept { return &historyModel_; }
EncyclopediaController* AppController::encyclopedia() noexcept { return &encyclopediaController_; }
QString AppController::previewSummary() const { return preview_ ? preview_->summary : QString{}; }
bool AppController::previewReady() const noexcept { return preview_.has_value() && !preview_->operations.isEmpty(); }
QVariantList AppController::previewOperations() const
{
    QVariantList result;
    if (!preview_) return result;
    for (const auto& variant : preview_->operations) {
        std::visit([&](const auto& operation) {
            const auto* tweak = catalog_.find(operation.tweakId);
            QString object;
            if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                         planning::PlannedRegistryDwordChange>) {
                object = registryObject(operation.change.location);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedAppxRemoval>) {
                object = u"AppX: "_s + operation.change.packageName;
            } else {
                object = u"Feature ID: "_s + QString::number(operation.change.featureId);
            }
            result.append(QVariantMap{
                {u"title"_s, tweak ? tweak->title : operation.tweakId.toString()},
                {u"beforeState"_s, detected_.value(operation.tweakId).stateId},
                {u"targetState"_s, operation.targetState},
                {u"registryObject"_s, object},
                {u"restart"_s, restartName(operation.restart)},
            });
        }, variant);
    }
    return result;
}
QString AppController::lastErrorCode() const { return lastErrorCode_; }
int AppController::applyProgress() const noexcept { return applyProgress_; }
QString AppController::applyStatus() const { return applyStatus_; }
QString AppController::applyMessage() const { return applyMessage_; }
bool AppController::rebootRequired() const noexcept { return rebootRequired_; }
QVariantMap AppController::systemOverview() const { return systemOverview_; }
bool AppController::systemOverviewLoading() const noexcept { return systemOverviewLoading_; }
QString AppController::systemOverviewError() const { return systemOverviewError_; }
QString AppController::appRemovalScanStatus() const { return appRemovalScanStatus_; }
QString AppController::appRemovalScanError() const { return appRemovalScanError_; }

bool AppController::startup()
{
    const auto loaded = services_->loadCatalog();
    if (!loaded.catalog) {
        setError(u"catalog.load_failed"_s,
                 loaded.errors.isEmpty() ? QString{} : loaded.errors.first().message);
        return false;
    }
    catalog_ = *loaded.catalog;
    const auto loadedCategories = services_->loadCategories();
    if (!loadedCategories.catalog) {
        setError(u"categories.load_failed"_s,
                 loadedCategories.errors.isEmpty() ? QString{} : loadedCategories.errors.first().message);
        return false;
    }
    categoryCatalog_ = *loadedCategories.catalog;
    profile_ = services_->currentProfile();
    refreshDetectedStates();
    refreshModels();
    refreshCategories();
    encyclopediaController_.reset(catalog_, categoryCatalog_);
    historyModel_.reset(services_->history(), catalog_);
    refreshSystemOverview();
    setError({});
    return true;
}

void AppController::setTweakSearch(const QString& query)
{
    filteredTweaksModel_.setQuery(query);
}

void AppController::setTweakCategory(const QString& categoryId)
{
    filteredTweaksModel_.setCategoryId(categoryId);
}

void AppController::setHideUnsupportedTweaks(bool hide)
{
    filteredTweaksModel_.setHideUnsupported(hide);
}

int AppController::revealTweak(const QString& id)
{
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) {
        setError(u"tweak.unknown"_s);
        return -1;
    }

    filteredTweaksModel_.setQuery({});
    filteredTweaksModel_.setHideUnsupported(false);
    filteredTweaksModel_.setCategoryId(tweak->category);

    for (int row = 0; row < filteredTweaksModel_.rowCount(); ++row) {
        const auto index = filteredTweaksModel_.index(row, 0);
        if (filteredTweaksModel_.data(index, TweakListModel::IdRole).toString() == id) {
            setError({});
            return row;
        }
    }

    setError(u"tweak.not_visible"_s);
    return -1;
}

bool AppController::selectTarget(const QString& id, const QString& state)
{
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) {
        setError(u"tweak.unknown"_s);
        return false;
    }
    const auto actual = detected_.value(*parsed);
    if (actual.status == domain::DetectionStatus::Named && actual.stateId == state) {
        (void)queueData_.remove(*parsed);
        preview_.reset();
        (void)tweaksModel_.setTargetState(*parsed, {});
        queueModel_.reset(catalog_, queueData_, detected_);
        emit previewChanged();
        resetOperationState();
        setError({});
        return true;
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
    resetOperationState();
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
    resetOperationState();
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
    } else if (tweak->appxDetection) {
        registryObject = u"AppX: "_s + tweak->appxDetection->packageName;
    } else if (tweak->featureDetection) {
        registryObject = u"Feature ID: "_s + QString::number(tweak->featureDetection->featureId);
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
        {u"rollback"_s, tweak->appxDetection
            ? u"Автоматический возврат удалённого пакета не выполняется; потребуется повторная установка приложения."_s
            : tweak->featureDetection
                ? u"При возврате восстанавливается исходное пользовательское переопределение Feature Store либо оно удаляется, если его не было."_s
                : u"При возврате восстанавливаются точный исходный тип и байты значения; если значения не было, оно удаляется."_s},
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
    const auto requiresReboot = preview_->restart == domain::RestartRequirement::Reboot;
    applyProgress_ = 0;
    applyStatus_ = u"running"_s;
    applyMessage_ = u"Запуск Executor"_s;
    rebootRequired_ = false;
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
    rebootRequired_ = requiresReboot;
    emit operationChanged();
    const auto queued = queueData_.items();
    for (const auto& item : queued) (void)queueData_.remove(item.tweakId);
    preview_.reset();
    const auto reloaded = services_->loadCatalog();
    if (reloaded.catalog) {
        catalog_ = *reloaded.catalog;
        if (appRemovalScanStatus_ == u"succeeded"_s) {
            const auto removals = services_->loadAppRemovalCatalog();
            if (removals.catalog) {
                auto definitions = catalog_.tweaks();
                definitions += removals.catalog->tweaks();
                catalog_ = content::TweakCatalog(std::move(definitions));
            } else {
                appRemovalScanStatus_ = u"failed"_s;
                appRemovalScanError_ = removals.errors.isEmpty()
                    ? u"Не удалось обновить список установленных приложений."_s
                    : removals.errors.first().message;
                emit appRemovalScanChanged();
            }
        }
    }
    refreshDetectedStates();
    refreshModels();
    encyclopediaController_.reset(catalog_, categoryCatalog_);
    historyModel_.reset(services_->history(), catalog_);
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
    historyModel_.reset(services_->history(), catalog_);
    setError({});
    return true;
}

bool AppController::restartComputer()
{
    if (applyStatus_ != u"succeeded" || !rebootRequired_) {
        setError(u"restart.not_required"_s);
        return false;
    }
    if (!services_->restartComputer()) {
        setError(u"restart.launch_failed"_s);
        return false;
    }
    setError({});
    return true;
}

void AppController::refreshSystemOverview()
{
    if (systemOverviewWatcher_.isRunning()) return;
    systemOverviewLoading_ = true;
    systemOverviewError_.clear();
    emit systemOverviewChanged();
    auto* services = services_;
    systemOverviewWatcher_.setFuture(
        QtConcurrent::run([services] { return services->systemOverview(); }));
}

void AppController::scanInstalledApps()
{
    if (appRemovalScanWatcher_.isRunning()) return;
    appRemovalScanStatus_ = u"running"_s;
    appRemovalScanError_.clear();
    emit appRemovalScanChanged();
    auto* services = services_;
    appRemovalScanWatcher_.setFuture(
        QtConcurrent::run([services] { return services->loadAppRemovalCatalog(); }));
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

void AppController::refreshCategories()
{
    if (!filteredTweaksModel_.hideUnsupported()) {
        categoriesModel_.reset(categoryCatalog_);
        return;
    }

    QSet<QString> visibleCategoryIds;
    for (const auto& tweak : catalog_.tweaks()) {
        if (supported_.value(tweak.id)) visibleCategoryIds.insert(tweak.category);
    }

    QVector<content::CategoryDefinition> visibleCategories;
    for (const auto& category : categoryCatalog_.categories()) {
        if (category.id == u"app-removal"_s || visibleCategoryIds.contains(category.id)) {
            visibleCategories.append(category);
        }
    }

    const auto selectedCategory = filteredTweaksModel_.categoryId();
    if (!selectedCategory.isEmpty()
        && selectedCategory != u"app-removal"_s
        && !visibleCategoryIds.contains(selectedCategory)) {
        filteredTweaksModel_.setCategoryId({});
    }
    categoriesModel_.reset(content::CategoryCatalog(std::move(visibleCategories)));
}

void AppController::resetOperationState()
{
    if (applyStatus_ == u"running") return;
    if (applyStatus_ == u"idle" && applyProgress_ == 0
        && applyMessage_.isEmpty() && !rebootRequired_) return;
    applyProgress_ = 0;
    applyStatus_ = u"idle"_s;
    applyMessage_.clear();
    rebootRequired_ = false;
    emit operationChanged();
}

void AppController::setError(QString code, QString message)
{
    if (lastErrorCode_ == code && lastErrorMessage_ == message) return;
    lastErrorCode_ = std::move(code);
    lastErrorMessage_ = std::move(message);
    emit errorChanged();
}

} // namespace tweakopedia::app
