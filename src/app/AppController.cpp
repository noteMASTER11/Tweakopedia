#include "app/AppController.h"

#include "content/CategoryMembershipValidator.h"
#include "detection/CompatibilityEvaluator.h"
#include "planning/PlanBuilder.h"
#include "app/SystemOverviewPresenter.h"

#include <QtConcurrentRun>
#include <QDebug>
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
    QString hive;
    switch (location.hive) {
    case domain::RegistryHive::CurrentUser: hive = u"HKCU\\"_s; break;
    case domain::RegistryHive::LocalMachine: hive = u"HKLM\\"_s; break;
    case domain::RegistryHive::ClassesRoot: hive = u"HKCR\\"_s; break;
    case domain::RegistryHive::Users: hive = u"HKU\\"_s; break;
    }
    return hive + location.key + u"\\"_s + location.valueName;
}

QString registryObject(const domain::RegistryKeyLocation& location)
{
    QString hive;
    switch (location.hive) {
    case domain::RegistryHive::CurrentUser: hive = u"HKCU\\"_s; break;
    case domain::RegistryHive::LocalMachine: hive = u"HKLM\\"_s; break;
    case domain::RegistryHive::ClassesRoot: hive = u"HKCR\\"_s; break;
    case domain::RegistryHive::Users: hive = u"HKU\\"_s; break;
    }
    return hive + location.key;
}

struct TelemetryProfileTarget {
    QString id;
    QString state;
    QString group;
};

const QVector<TelemetryProfileTarget>& telemetryProfileTargets(QStringView profileId)
{
    static const QVector<TelemetryProfileTarget> basic{
        {u"privacy.diagnostic-data-level"_s, u"required"_s, u"Диагностические данные"_s},
        {u"privacy.device-name-diagnostic-data"_s, u"disabled"_s, u"Диагностические данные"_s},
        {u"privacy.feedback-prompts"_s, u"disabled"_s, u"Диагностические данные"_s},
        {u"privacy.tailored-experiences"_s, u"disabled"_s, u"Персонализация"_s},
        {u"privacy.tailored-experiences-user"_s, u"disabled"_s, u"Персонализация"_s},
        {u"privacy.advertising-id-block"_s, u"enabled"_s, u"Персонализация"_s},
        {u"privacy.advertising-id-user"_s, u"disabled"_s, u"Персонализация"_s},
    };
    static const QVector<TelemetryProfileTarget> balanced = [] {
        auto targets = basic;
        targets.append({
            {u"privacy.onesettings-downloads"_s, u"disabled"_s, u"Фоновая конфигурация"_s},
            {u"privacy.onesettings-auditing"_s, u"disabled"_s, u"Фоновая конфигурация"_s},
            {u"privacy.experimentation"_s, u"disabled"_s, u"Фоновая конфигурация"_s},
            {u"privacy.activity-feed"_s, u"disabled"_s, u"История действий"_s},
            {u"privacy.publish-user-activities"_s, u"disabled"_s, u"История действий"_s},
            {u"privacy.upload-user-activities"_s, u"disabled"_s, u"История действий"_s},
            {u"privacy.inking-typing-improvement"_s, u"disabled"_s, u"Персонализация ввода"_s},
            {u"privacy.input-personalization-consent"_s, u"disabled"_s, u"Персонализация ввода"_s},
            {u"privacy.implicit-ink-collection"_s, u"disabled"_s, u"Персонализация ввода"_s},
            {u"privacy.implicit-text-collection"_s, u"disabled"_s, u"Персонализация ввода"_s},
        });
        return targets;
    }();
    static const QVector<TelemetryProfileTarget> strict = [] {
        auto targets = balanced;
        targets[0].state = u"off"_s;
        targets.append({
            {u"behavior.application-impact-telemetry"_s, u"disabled"_s, u"Совместимость приложений"_s},
            {u"apps.edge-diagnostic-data"_s, u"off"_s, u"Microsoft Edge"_s},
            {u"apps.edge-user-feedback"_s, u"disabled"_s, u"Microsoft Edge"_s},
            {u"privacy.app-access-diagnostic-info"_s, u"force_deny"_s, u"Доступ приложений"_s},
            {u"privacy.wer-reporting-device"_s, u"disabled"_s, u"Отчёты о сбоях"_s},
            {u"privacy.wer-reporting-user"_s, u"disabled"_s, u"Отчёты о сбоях"_s},
        });
        return targets;
    }();

    if (profileId == u"basic") return basic;
    if (profileId == u"strict") return strict;
    return balanced;
}

QString stateTitle(const domain::TweakDefinition& tweak, QStringView stateId)
{
    const auto state = std::find_if(tweak.states.cbegin(), tweak.states.cend(),
        [stateId](const auto& candidate) { return candidate.id == stateId; });
    return state == tweak.states.cend() ? stateId.toString() : state->title;
}

} // namespace

AppController::AppController(IAppServices& services, QObject* parent)
    : QObject(parent)
    , services_(&services)
    , tweaksModel_(this)
    , filteredTweaksModel_(this)
    , tweakGroupsModel_(this)
    , categoriesModel_(this)
    , queueModel_(this)
    , historyModel_(this)
    , encyclopediaController_(this)
{
    filteredTweaksModel_.setSourceModel(&tweaksModel_);
    connect(&systemOverviewWatcher_,
            &QFutureWatcher<domain::SystemOverviewSnapshot>::finished,
            this,
            [this] {
                const auto snapshot = systemOverviewWatcher_.result();
                systemOverview_ = SystemOverviewPresenter::present(snapshot);
                systemOverviewError_ = snapshot.error;
                systemOverviewLoading_ = false;
                qDebug().noquote() << "System overview loaded; error:"
                                   << (snapshot.error.isEmpty() ? u"none"_s : snapshot.error);
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
                    qWarning().noquote() << "Installed application scan failed:"
                                         << appRemovalScanError_;
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
                refreshTweakGroups();
                encyclopediaController_.reset(catalog_, categoryCatalog_);
                appRemovalScanStatus_ = u"succeeded"_s;
                appRemovalScanError_.clear();
                qInfo().noquote() << "Installed application scan completed; tweaks:"
                                  << result.catalog->tweaks().size();
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
TweakGroupListModel* AppController::tweakGroups() noexcept { return &tweakGroupsModel_; }
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
                                                planning::PlannedRegistryValueChange>) {
                object = std::visit(
                    [](const auto& change) { return registryObject(change.location); },
                    operation.change);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedRegistryTreeChange>) {
                object = std::visit(
                    [](const auto& change) { return registryObject(change.location); },
                    operation.change);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedAppxRemoval>) {
                object = u"AppX: "_s + operation.change.packageName;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedFileChange>) {
                object = operation.change.destination;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedScheduledTaskChange>) {
                object = u"Задача: "_s + operation.change.location.folder
                    + u"\\"_s + operation.change.location.name;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedBcdElementChange>) {
                object = u"BCD: "_s + operation.change.spec.objectId + u" / 0x"_s
                    + QString::number(operation.change.spec.elementType, 16);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedPowerSettingChange>) {
                object = u"Питание: "_s + operation.change.location.subgroup
                    + u" / "_s + operation.change.location.setting;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(operation)>,
                                                planning::PlannedWindowsComponentChange>) {
                object = u"Компонент Windows: "_s + operation.change.target.name;
            } else {
                object = u"Feature ID: "_s + QString::number(operation.change.featureId);
            }
            result.append(QVariantMap{
                {u"title"_s, tweak ? tweak->title : operation.tweakId.toString()},
                {u"beforeState"_s, detected_.value(operation.tweakId).stateId},
                {u"targetState"_s, operation.targetState},
                {u"registryObject"_s, object},
                {u"restart"_s, restartName(operation.restart)},
                {u"inputs"_s, domain::inputMapToVariantMap(operation.inputs)},
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
    return startup({});
}

bool AppController::startup(const StartupProgress& progress)
{
    const auto report = [&progress](startup::StartupStage stage) {
        if (progress) progress(stage);
    };

    report(startup::StartupStage::CatalogLoad);
    qDebug() << "Loading tweak and category catalogs";
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
    report(startup::StartupStage::CatalogValidate);
    const auto membershipErrors = content::CategoryMembershipValidator{}.validate(
        categoryCatalog_, catalog_);
    if (!membershipErrors.isEmpty()) {
        setError(membershipErrors.first().code, membershipErrors.first().message);
        return false;
    }
    report(startup::StartupStage::WindowsProfile);
    profile_ = services_->currentProfile();
    report(startup::StartupStage::StateDetection);
    refreshDetectedStates();
    report(startup::StartupStage::Models);
    refreshModels();
    refreshCategories();
    refreshTweakGroups();
    encyclopediaController_.reset(catalog_, categoryCatalog_);
    historyModel_.reset(services_->history(), catalog_);
    refreshSystemOverview();
    setError({});
    qInfo().noquote() << "Application catalog initialized; tweaks:"
                      << catalog_.tweaks().size()
                      << "categories:" << categoryCatalog_.categories().size()
                      << "history records:" << historyModel_.rowCount();
    return true;
}

void AppController::setTweakSearch(const QString& query)
{
    filteredTweaksModel_.setQuery(query);
}

void AppController::setTweakCategory(const QString& categoryId)
{
    filteredTweaksModel_.setCategoryId(categoryId);
    refreshTweakGroups();
}

void AppController::setTweakSubcategory(const QString& subcategoryId)
{
    if (!tweakGroupsModel_.select(subcategoryId)) return;
    filteredTweaksModel_.setSubcategoryId(tweakGroupsModel_.selectedId());
}

void AppController::stepTweakSubcategory(int delta)
{
    if (delta == 0) return;
    setTweakSubcategory(tweakGroupsModel_.adjacentId(delta));
}

void AppController::setHideUnsupportedTweaks(bool hide)
{
    filteredTweaksModel_.setHideUnsupported(hide);
    refreshCategories();
    refreshTweakGroups();
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
    refreshCategories();
    refreshTweakGroups();
    setTweakSubcategory(tweak->subcategory);

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
    return selectParameterizedTarget(id, state, {});
}

bool AppController::selectParameterizedTarget(
    const QString& id,
    const QString& state,
    const QVariantMap& inputs)
{
    qDebug().noquote() << "Selecting tweak target:" << id << "->" << state;
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) {
        setError(u"tweak.unknown"_s);
        return false;
    }
    const auto actual = detected_.value(*parsed);
    if (actual.status == domain::DetectionStatus::Named && actual.stateId == state
        && inputs.isEmpty()) {
        (void)queueData_.remove(*parsed);
        preview_.reset();
        (void)tweaksModel_.setTargetState(*parsed, {});
        queueModel_.reset(catalog_, queueData_, detected_);
        emit previewChanged();
        resetOperationState();
        setError({});
        qDebug().noquote() << "Tweak target matches current state; removed from queue:"
                           << id << "queue size:" << queueData_.items().size();
        return true;
    }
    const auto changed = queueData_.setTarget(*tweak, state, inputs);
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
    qDebug().noquote() << "Tweak target queued:" << id
                       << "queue size:" << queueData_.items().size();
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

QVariantMap AppController::telemetryProfile(const QString& profileId) const
{
    const auto normalizedProfileId = profileId == u"basic" || profileId == u"strict"
        ? profileId : u"balanced"_s;
    QVariantList entries;
    int configuredCount{};
    int changeCount{};
    int unsupportedCount{};
    int queuedCount{};

    for (const auto& profileTarget : telemetryProfileTargets(normalizedProfileId)) {
        const auto parsed = domain::TweakId::parse(profileTarget.id);
        const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
        if (!tweak) continue;

        const auto supported = supported_.value(*parsed, false);
        const auto actual = detected_.value(*parsed);
        const auto queued = std::find_if(
            queueData_.items().cbegin(), queueData_.items().cend(),
            [&parsed](const auto& item) { return item.tweakId == *parsed; });
        const auto queuedState = queued == queueData_.items().cend()
            ? QString{} : queued->targetState;

        QString status;
        if (!supported) {
            status = u"unsupported"_s;
            ++unsupportedCount;
        } else if (queuedState == profileTarget.state) {
            status = u"queued"_s;
            ++queuedCount;
        } else if (actual.status == domain::DetectionStatus::Named
                   && actual.stateId == profileTarget.state) {
            status = u"configured"_s;
            ++configuredCount;
        } else {
            status = queuedState.isEmpty() ? u"change"_s : u"replace"_s;
            ++changeCount;
        }

        entries.append(QVariantMap{
            {u"id"_s, profileTarget.id},
            {u"title"_s, tweak->title},
            {u"summary"_s, tweak->summary},
            {u"group"_s, profileTarget.group},
            {u"currentState"_s, actual.stateId},
            {u"currentTitle"_s,
             actual.status == domain::DetectionStatus::Named
                 ? stateTitle(*tweak, actual.stateId) : u"Не определено"_s},
            {u"targetState"_s, profileTarget.state},
            {u"targetTitle"_s, stateTitle(*tweak, profileTarget.state)},
            {u"queuedState"_s, queuedState},
            {u"status"_s, status},
            {u"supported"_s, supported},
            {u"restart"_s, restartName(tweak->restart)},
        });
    }

    return {
        {u"profileId"_s, normalizedProfileId},
        {u"entries"_s, entries},
        {u"totalCount"_s, entries.size()},
        {u"configuredCount"_s, configuredCount},
        {u"changeCount"_s, changeCount},
        {u"unsupportedCount"_s, unsupportedCount},
        {u"queuedCount"_s, queuedCount},
    };
}

int AppController::queueTelemetryProfile(const QString& profileId)
{
    const auto normalizedProfileId = profileId == u"basic" || profileId == u"strict"
        ? profileId : u"balanced"_s;
    int queued{};
    for (const auto& profileTarget : telemetryProfileTargets(normalizedProfileId)) {
        const auto parsed = domain::TweakId::parse(profileTarget.id);
        if (!parsed || !catalog_.find(*parsed) || !supported_.value(*parsed, false)) continue;
        const auto actual = detected_.value(*parsed);
        const auto queuedTarget = std::find_if(
            queueData_.items().cbegin(), queueData_.items().cend(),
            [&parsed](const auto& item) { return item.tweakId == *parsed; });
        if (queuedTarget != queueData_.items().cend()
            && queuedTarget->targetState == profileTarget.state) {
            continue;
        }
        if (actual.status == domain::DetectionStatus::Named
            && actual.stateId == profileTarget.state) {
            if (queuedTarget != queueData_.items().cend())
                (void)selectTarget(profileTarget.id, profileTarget.state);
            continue;
        }
        if (selectTarget(profileTarget.id, profileTarget.state)) ++queued;
    }
    return queued;
}

QVariantMap AppController::openExplanation(const QString& id) const
{
    const auto parsed = domain::TweakId::parse(id);
    const auto* tweak = parsed ? catalog_.find(*parsed) : nullptr;
    if (!tweak) return {};
    const auto& explanation = tweak->explanation;
    QString technicalObject;
    if (tweak->detection) {
        technicalObject = registryObject(tweak->detection->location);
    } else if (tweak->valueDetection) {
        technicalObject = registryObject(tweak->valueDetection->location);
    } else if (tweak->treeDetection) {
        technicalObject = registryObject(tweak->treeDetection->location);
    } else if (tweak->appxDetection) {
        technicalObject = u"AppX: "_s + tweak->appxDetection->packageName;
    } else if (tweak->featureDetection) {
        technicalObject = u"Feature ID: "_s + QString::number(tweak->featureDetection->featureId);
    } else if (tweak->windowsComponentDetection) {
        technicalObject = u"Компонент Windows: "_s
            + tweak->windowsComponentDetection->target.name;
    }
    return {
        {u"title"_s, tweak->title},
        {u"purpose"_s, explanation.purpose},
        {u"mechanism"_s, explanation.mechanism},
        {u"effect"_s, explanation.effect},
        {u"tradeoffs"_s, explanation.tradeoffs},
        {u"recommendation"_s, explanation.recommendation},
        {u"technicalDetails"_s, explanation.technicalDetails},
        {u"registryObject"_s, technicalObject},
        {u"rollback"_s, tweak->appxDetection
            ? u"Автоматический возврат удалённого пакета не выполняется; потребуется повторная установка приложения."_s
            : tweak->featureDetection
                ? u"При возврате восстанавливается исходное пользовательское переопределение Feature Store либо оно удаляется, если его не было."_s
                : tweak->windowsComponentDetection
                    ? u"При возврате DISM восстанавливает исходное состояние компонента Windows."_s
                : u"При возврате восстанавливаются точный исходный тип и байты значения; если значения не было, оно удаляется."_s},
    };
}

bool AppController::buildPreview()
{
    qDebug().noquote() << "Building execution preview; queue size:"
                       << queueData_.items().size();
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
    qDebug().noquote() << "Execution preview ready; operations:"
                       << preview_->operations.size();
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
    qInfo().noquote() << "Applying tweak package:" << packageName.trimmed()
                      << "operations:" << preview_->operations.size();
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
        qWarning().noquote() << "Tweak package failed; code:" << result.code
                             << "message:" << result.message;
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
    refreshCategories();
    refreshTweakGroups();
    encyclopediaController_.reset(catalog_, categoryCatalog_);
    historyModel_.reset(services_->history(), catalog_);
    emit previewChanged();
    setError({});
    qInfo().noquote() << "Tweak package applied; reboot required:" << rebootRequired_;
    return true;
}

bool AppController::rollback(const QString& transactionId)
{
    qInfo().noquote() << "Starting rollback:" << transactionId;
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
        qWarning().noquote() << "Rollback failed; code:" << result.code
                             << "message:" << result.message;
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
    qInfo().noquote() << "Rollback completed:" << transactionId;
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

void AppController::refreshTweakGroups()
{
    tweakGroupsModel_.reset(
        categoryCatalog_, catalog_, supported_, filteredTweaksModel_.categoryId(),
        filteredTweaksModel_.hideUnsupported());
    filteredTweaksModel_.setSubcategoryId(tweakGroupsModel_.selectedId());
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
    if (!code.isEmpty()) {
        qWarning().noquote() << "Application error; code:" << code
                             << "message:" << message;
    }
    lastErrorCode_ = std::move(code);
    lastErrorMessage_ = std::move(message);
    emit errorChanged();
}

} // namespace tweakopedia::app
