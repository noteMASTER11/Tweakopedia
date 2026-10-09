#include "execution/TransactionRunner.h"

#include "execution/AppxPackageExecutor.h"
#include "execution/RegistryDwordExecutor.h"
#include "execution/RegistryValueExecutor.h"
#include "execution/RegistryTreeExecutor.h"
#include "execution/FeatureStateExecutor.h"
#include "execution/FileOperationExecutor.h"
#include "execution/ScheduledTaskExecutor.h"
#include "execution/BcdElementExecutor.h"
#include "execution/PowerSettingExecutor.h"
#include "execution/WindowsComponentExecutor.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

using CapturedSnapshot = std::variant<
    RegistrySnapshot,
    domain::RegistryTreeSnapshot,
    domain::FileSnapshot,
    domain::ScheduledTaskSnapshot,
    domain::BcdElementSnapshot,
    domain::PowerSettingSnapshot,
    domain::WindowsComponentSnapshot,
    FeatureSnapshot,
    AppxPackageSnapshot>;

QJsonObject beforeDocument(const QVector<CapturedSnapshot>& snapshots)
{
    QJsonArray operations;
    for (const auto& snapshot : snapshots) {
        operations.append(std::visit([](const auto& value) { return value.toJson(); }, snapshot));
    }
    return {{u"operations"_s, operations}};
}

QJsonObject resultDocument(const TransactionRunResult& result)
{
    return {
        {u"status"_s, result.success ? u"succeeded"_s
                                     : result.rolledBack ? u"rolled_back"_s : u"failed"_s},
        {u"code"_s, result.code},
        {u"message"_s, result.message},
    };
}

QString operationType(const planning::PlannedOperation& operation)
{
    return std::visit([](const auto& value) -> QString {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, planning::PlannedRegistryDwordChange>) return u"registry.dword"_s;
        if constexpr (std::is_same_v<T, planning::PlannedRegistryValueChange>) return u"registry.value"_s;
        if constexpr (std::is_same_v<T, planning::PlannedRegistryTreeChange>) return u"registry.tree"_s;
        if constexpr (std::is_same_v<T, planning::PlannedFileChange>) return u"file"_s;
        if constexpr (std::is_same_v<T, planning::PlannedScheduledTaskChange>) return u"scheduled_task"_s;
        if constexpr (std::is_same_v<T, planning::PlannedBcdElementChange>) return u"bcd"_s;
        if constexpr (std::is_same_v<T, planning::PlannedPowerSettingChange>) return u"power"_s;
        if constexpr (std::is_same_v<T, planning::PlannedWindowsComponentChange>) return u"windows_component"_s;
        if constexpr (std::is_same_v<T, planning::PlannedFeatureStateChange>) return u"feature_store"_s;
        return u"appx"_s;
    }, operation);
}

const domain::TweakId& operationTweakId(const planning::PlannedOperation& operation)
{
    return std::visit([](const auto& value) -> const domain::TweakId& {
        return value.tweakId;
    }, operation);
}

} // namespace

TransactionRunner::TransactionRunner(
    ExecutionBackends backends,
    persistence::TransactionFiles& files,
    persistence::TransactionRepository& repository)
    : backend_(backends.registry), appxBackend_(backends.appx),
      featureBackend_(backends.featureStore), scheduledTaskBackend_(backends.scheduledTasks),
      bcdBackend_(backends.bcd), powerBackend_(backends.powerSettings),
      windowsComponentBackend_(backends.windowsComponents),
      files_(&files), repository_(&repository)
{
}

TransactionRunResult TransactionRunner::run(
    const planning::ExecutionPlan& plan,
    const TransactionProgressCallback& progress)
{
    if (!backend_) {
        return {.code = u"operation.unsupported"_s,
                .message = u"Исполнитель реестра недоступен."_s};
    }
    for (const auto& operation : plan.operations) {
        const auto available = std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, planning::PlannedAppxRemoval>) {
                return appxBackend_ != nullptr;
            } else if constexpr (std::is_same_v<T, planning::PlannedFeatureStateChange>) {
                return featureBackend_ != nullptr;
            } else if constexpr (std::is_same_v<T, planning::PlannedScheduledTaskChange>) {
                return scheduledTaskBackend_ != nullptr;
            } else if constexpr (std::is_same_v<T, planning::PlannedBcdElementChange>) {
                return bcdBackend_ != nullptr;
            } else if constexpr (std::is_same_v<T, planning::PlannedPowerSettingChange>) {
                return powerBackend_ != nullptr;
            } else if constexpr (std::is_same_v<T, planning::PlannedWindowsComponentChange>) {
                return windowsComponentBackend_ != nullptr;
            }
            return true;
        }, operation);
        if (!available) {
            return {.code = u"operation.unsupported"_s,
                    .message = u"Для одной из операций отсутствует исполнитель."_s};
        }
    }
    RegistryDwordExecutor executor(*backend_);
    RegistryValueExecutor valueExecutor(*backend_);
    RegistryTreeExecutor treeExecutor(*backend_);
    FileOperationExecutor fileExecutor(files_->directory(plan.transactionId));
    std::optional<AppxPackageExecutor> appxExecutor;
    if (appxBackend_) appxExecutor.emplace(*appxBackend_);
    std::optional<FeatureStateExecutor> featureExecutor;
    if (featureBackend_) featureExecutor.emplace(*featureBackend_);
    std::optional<ScheduledTaskExecutor> scheduledTaskExecutor;
    if (scheduledTaskBackend_) scheduledTaskExecutor.emplace(*scheduledTaskBackend_);
    std::optional<BcdElementExecutor> bcdExecutor;
    if (bcdBackend_) bcdExecutor.emplace(*bcdBackend_);
    std::optional<PowerSettingExecutor> powerExecutor;
    if (powerBackend_) powerExecutor.emplace(*powerBackend_);
    std::optional<WindowsComponentExecutor> windowsComponentExecutor;
    if (windowsComponentBackend_) windowsComponentExecutor.emplace(*windowsComponentBackend_);
    QVector<CapturedSnapshot> snapshots;
    bool runningMarked = false;
    bool appxAttempted = false;
    QStringList touchedPowerSchemes;

    const auto finishFailure = [&](QString code, QString message) {
        bool restored = true;
        bool restoredAny = false;
        for (qsizetype index = snapshots.size() - 1; index >= 0; --index) {
            if (const auto* registry = std::get_if<RegistrySnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!valueExecutor.restore(*registry).success) restored = false;
            } else if (const auto* tree = std::get_if<domain::RegistryTreeSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!treeExecutor.restore(*tree).success) restored = false;
            } else if (const auto* feature = std::get_if<FeatureSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!featureExecutor || !featureExecutor->restore(*feature).success) restored = false;
            } else if (const auto* file = std::get_if<domain::FileSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!fileExecutor.restore(*file).success) restored = false;
            } else if (const auto* task = std::get_if<domain::ScheduledTaskSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!scheduledTaskExecutor || !scheduledTaskExecutor->restore(*task).success) {
                    restored = false;
                }
            } else if (const auto* bcd = std::get_if<domain::BcdElementSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!bcdExecutor || !bcdExecutor->restore(*bcd).success) restored = false;
            } else if (const auto* power = std::get_if<domain::PowerSettingSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!powerExecutor || !powerExecutor->restore(*power).success) restored = false;
            } else if (const auto* component =
                           std::get_if<domain::WindowsComponentSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!windowsComponentExecutor
                    || !windowsComponentExecutor->restore(*component).success) restored = false;
            }
        }
        TransactionRunResult result{
            .success = false,
            .rolledBack = restored && restoredAny && !appxAttempted,
            .code = std::move(code),
            .message = std::move(message),
        };
        const auto status = result.rolledBack
            ? persistence::TransactionStatus::RolledBack
            : persistence::TransactionStatus::Failed;
        (void)repository_->updateStatus(plan.transactionId, status, result.message);
        (void)files_->writeResult(plan.transactionId, resultDocument(result));
        return result;
    };

    const auto markRunning = [&]() -> std::optional<TransactionRunResult> {
        if (runningMarked) return std::nullopt;
        if (!repository_->updateStatus(plan.transactionId, persistence::TransactionStatus::Running)) {
            return finishFailure(u"transaction.status_failed"_s, repository_->lastError());
        }
        runningMarked = true;
        return std::nullopt;
    };

    for (qsizetype operationIndex = 0; operationIndex < plan.operations.size(); ++operationIndex) {
        const auto& operation = plan.operations.at(operationIndex);
        if (progress) {
            progress(operationIndex + 1, plan.operations.size(),
                     operationTweakId(operation), operationType(operation));
        }
        if (const auto* registry = std::get_if<planning::PlannedRegistryDwordChange>(&operation)) {
            const auto captured = executor.capture(registry->change.location);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = executor.compareBefore(captured.snapshot, registry->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);

            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(
                    u"transaction.snapshot_persist_failed"_s,
                    u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;

            const auto applied = executor.apply(registry->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* registry = std::get_if<planning::PlannedRegistryValueChange>(&operation)) {
            const auto location = std::visit(
                [](const auto& change) { return change.location; }, registry->change);
            const auto captured = valueExecutor.capture(location);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = valueExecutor.compareBefore(
                captured.snapshot, registry->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);

            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(
                    u"transaction.snapshot_persist_failed"_s,
                    u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;

            const auto applied = std::visit(
                [&](const auto& change) { return valueExecutor.apply(change); },
                registry->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* tree = std::get_if<planning::PlannedRegistryTreeChange>(&operation)) {
            const auto location = std::visit(
                [](const auto& change) { return change.location; }, tree->change);
            const auto captured = treeExecutor.capture(location);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = treeExecutor.compareBefore(
                captured.snapshot, tree->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);

            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(
                    u"transaction.snapshot_persist_failed"_s,
                    u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = std::visit(
                [&](const auto& change) { return treeExecutor.apply(change); },
                tree->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* feature = std::get_if<planning::PlannedFeatureStateChange>(&operation)) {
            if (!featureExecutor) {
                return finishFailure(u"operation.unsupported"_s,
                                     u"Исполнитель Feature Store недоступен."_s);
            }
            const auto captured = featureExecutor->capture(feature->change.featureId);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = featureExecutor->compareBefore(
                captured.snapshot, feature->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = featureExecutor->apply(feature->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* task = std::get_if<planning::PlannedScheduledTaskChange>(&operation)) {
            if (!scheduledTaskExecutor) {
                return finishFailure(u"operation.unsupported"_s,
                                     u"Исполнитель задач планировщика недоступен."_s);
            }
            const auto captured = scheduledTaskExecutor->capture(task->change.location);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = scheduledTaskExecutor->compareBefore(
                captured.snapshot, task->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = scheduledTaskExecutor->apply(task->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* bcd = std::get_if<planning::PlannedBcdElementChange>(&operation)) {
            if (!bcdExecutor) {
                return finishFailure(u"operation.unsupported"_s,
                                     u"Исполнитель BCD недоступен."_s);
            }
            const auto captured = bcdExecutor->capture(bcd->change.spec);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = bcdExecutor->compareBefore(
                captured.snapshot, bcd->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = bcdExecutor->apply(bcd->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* power = std::get_if<planning::PlannedPowerSettingChange>(&operation)) {
            if (!powerExecutor) return finishFailure(u"operation.unsupported"_s,
                                                     u"Исполнитель схем питания недоступен."_s);
            const auto captured = powerExecutor->capture(power->change.location);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = powerExecutor->compareBefore(captured.snapshot, power->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = powerExecutor->apply(power->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            if (!touchedPowerSchemes.contains(captured.snapshot.resolvedScheme))
                touchedPowerSchemes.append(captured.snapshot.resolvedScheme);
            continue;
        }
        if (const auto* component =
                std::get_if<planning::PlannedWindowsComponentChange>(&operation)) {
            if (!windowsComponentExecutor) return finishFailure(
                u"operation.unsupported"_s,
                u"Исполнитель компонентов Windows недоступен."_s);
            const auto captured = windowsComponentExecutor->capture(component->change.target);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = windowsComponentExecutor->compareBefore(
                captured.snapshot, component->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = windowsComponentExecutor->apply(component->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        if (const auto* file = std::get_if<planning::PlannedFileChange>(&operation)) {
            const auto captured = fileExecutor.capture(file->change);
            if (!captured.success) return finishFailure(captured.code, captured.message);
            const auto compared = fileExecutor.compareBefore(
                captured.snapshot, file->beforeFingerprint);
            if (!compared.success) return finishFailure(compared.code, compared.message);
            snapshots.append(captured.snapshot);
            if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
                snapshots.removeLast();
                return finishFailure(u"transaction.snapshot_persist_failed"_s,
                                     u"Не удалось записать снимок транзакции."_s);
            }
            if (const auto failed = markRunning()) return *failed;
            const auto applied = fileExecutor.apply(file->change);
            if (!applied.success) return finishFailure(applied.code, applied.message);
            continue;
        }
        const auto* appx = std::get_if<planning::PlannedAppxRemoval>(&operation);
        if (!appx || !appxExecutor) {
            return finishFailure(
                u"operation.unsupported"_s,
                u"План содержит неподдерживаемую операцию."_s);
        }
        const auto captured = appxExecutor->capture(appx->change.packageName);
        if (!captured.success) return finishFailure(captured.code, captured.message);
        const auto compared = appxExecutor->compareBefore(captured.snapshot, appx->beforeFingerprint);
        if (!compared.success) return finishFailure(compared.code, compared.message);

        snapshots.append(captured.snapshot);
        if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
            snapshots.removeLast();
            return finishFailure(
                u"transaction.snapshot_persist_failed"_s,
                u"Не удалось записать снимок транзакции."_s);
        }
        if (const auto failed = markRunning()) return *failed;
        appxAttempted = true;
        const auto applied = appxExecutor->apply(captured.snapshot);
        if (!applied.success) return finishFailure(applied.code, applied.message);
    }

    for (const auto& scheme : touchedPowerSchemes) {
        const auto activated = powerBackend_->activateScheme(scheme);
        if (!activated.success) return finishFailure(u"power.activate_failed"_s, activated.error);
    }
    TransactionRunResult result{.success = true};
    if (!repository_->updateStatus(plan.transactionId, persistence::TransactionStatus::Succeeded)) {
        return finishFailure(u"transaction.status_failed"_s, repository_->lastError());
    }
    if (!files_->writeResult(plan.transactionId, resultDocument(result))) {
        return finishFailure(
            u"transaction.result_persist_failed"_s,
            u"Не удалось записать результат транзакции."_s);
    }
    return result;
}

} // namespace tweakopedia::execution
