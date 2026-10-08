#include "execution/TransactionRunner.h"

#include "execution/AppxPackageExecutor.h"
#include "execution/RegistryDwordExecutor.h"
#include "execution/FeatureStateExecutor.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

using CapturedSnapshot = std::variant<RegistrySnapshot, FeatureSnapshot, AppxPackageSnapshot>;

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

} // namespace

TransactionRunner::TransactionRunner(
    platform::IRegistryBackend& backend,
    persistence::TransactionFiles& files,
    persistence::TransactionRepository& repository,
    platform::IAppxPackageBackend* appxBackend,
    platform::IFeatureStoreBackend* featureBackend)
    : backend_(&backend), appxBackend_(appxBackend), featureBackend_(featureBackend),
      files_(&files), repository_(&repository)
{
}

TransactionRunResult TransactionRunner::run(const planning::ExecutionPlan& plan)
{
    RegistryDwordExecutor executor(*backend_);
    std::optional<AppxPackageExecutor> appxExecutor;
    if (appxBackend_) appxExecutor.emplace(*appxBackend_);
    std::optional<FeatureStateExecutor> featureExecutor;
    if (featureBackend_) featureExecutor.emplace(*featureBackend_);
    QVector<CapturedSnapshot> snapshots;
    bool runningMarked = false;
    bool appxAttempted = false;

    const auto finishFailure = [&](QString code, QString message) {
        bool restored = true;
        bool restoredAny = false;
        for (qsizetype index = snapshots.size() - 1; index >= 0; --index) {
            if (const auto* registry = std::get_if<RegistrySnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!executor.restore(*registry).success) restored = false;
            } else if (const auto* feature = std::get_if<FeatureSnapshot>(&snapshots[index])) {
                restoredAny = true;
                if (!featureExecutor || !featureExecutor->restore(*feature).success) restored = false;
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

    for (const auto& operation : plan.operations) {
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
