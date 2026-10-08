#include "execution/TransactionRunner.h"

#include "execution/RegistryDwordExecutor.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

QJsonObject beforeDocument(const QVector<RegistrySnapshot>& snapshots)
{
    QJsonArray operations;
    for (const auto& snapshot : snapshots) operations.append(snapshot.toJson());
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
    persistence::TransactionRepository& repository)
    : backend_(&backend), files_(&files), repository_(&repository)
{
}

TransactionRunResult TransactionRunner::run(const planning::ExecutionPlan& plan)
{
    RegistryDwordExecutor executor(*backend_);
    QVector<RegistrySnapshot> snapshots;
    bool runningMarked = false;

    const auto finishFailure = [&](RegistryExecutionResult error, qsizetype rollbackCount) {
        bool restored = true;
        for (qsizetype index = rollbackCount - 1; index >= 0; --index) {
            if (!executor.restore(snapshots.at(index)).success) restored = false;
        }
        TransactionRunResult result{
            .success = false,
            .rolledBack = restored && rollbackCount > 0,
            .code = std::move(error.code),
            .message = std::move(error.message),
        };
        const auto status = result.rolledBack
            ? persistence::TransactionStatus::RolledBack
            : persistence::TransactionStatus::Failed;
        (void)repository_->updateStatus(plan.transactionId, status, result.message);
        (void)files_->writeResult(plan.transactionId, resultDocument(result));
        return result;
    };

    for (const auto& operation : plan.operations) {
        const auto* registry = std::get_if<planning::PlannedRegistryDwordChange>(&operation);
        if (!registry) {
            return finishFailure(
                {.success = false,
                 .code = u"operation.unsupported"_s,
                 .message = u"План содержит неподдерживаемую операцию."_s},
                snapshots.size());
        }

        const auto captured = executor.capture(registry->change.location);
        if (!captured.success) return finishFailure(captured, snapshots.size());
        const auto compared = executor.compareBefore(captured.snapshot, registry->beforeFingerprint);
        if (!compared.success) return finishFailure(compared, snapshots.size());

        snapshots.append(captured.snapshot);
        if (!files_->writeBefore(plan.transactionId, beforeDocument(snapshots))) {
            return finishFailure(
                {.success = false,
                 .code = u"transaction.snapshot_persist_failed"_s,
                 .message = u"Не удалось записать снимок транзакции."_s},
                snapshots.size() - 1);
        }
        if (!runningMarked) {
            if (!repository_->updateStatus(plan.transactionId, persistence::TransactionStatus::Running)) {
                return finishFailure(
                    {.success = false,
                     .code = u"transaction.status_failed"_s,
                     .message = repository_->lastError()},
                    snapshots.size() - 1);
            }
            runningMarked = true;
        }

        const auto applied = executor.apply(registry->change);
        if (!applied.success) return finishFailure(applied, snapshots.size());
    }

    TransactionRunResult result{.success = true};
    if (!repository_->updateStatus(plan.transactionId, persistence::TransactionStatus::Succeeded)) {
        return finishFailure(
            {.success = false,
             .code = u"transaction.status_failed"_s,
             .message = repository_->lastError()},
            snapshots.size());
    }
    if (!files_->writeResult(plan.transactionId, resultDocument(result))) {
        return finishFailure(
            {.success = false,
             .code = u"transaction.result_persist_failed"_s,
             .message = u"Не удалось записать результат транзакции."_s},
            snapshots.size());
    }
    return result;
}

} // namespace tweakopedia::execution
