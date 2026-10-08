#include "planning/PlanBuilder.h"

#include "detection/CompatibilityEvaluator.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::planning {
namespace {

int restartRank(domain::RestartRequirement requirement)
{
    switch (requirement) {
    case domain::RestartRequirement::None: return 0;
    case domain::RestartRequirement::Explorer: return 1;
    case domain::RestartRequirement::Service: return 2;
    case domain::RestartRequirement::SignOut: return 3;
    case domain::RestartRequirement::Reboot: return 4;
    }
    return 0;
}

const domain::TweakStateDefinition* findState(
    const domain::TweakDefinition& tweak,
    QStringView stateId)
{
    for (const auto& state : tweak.states) {
        if (state.id == stateId) {
            return &state;
        }
    }
    return nullptr;
}

void addIssue(
    PlanBuildResult& result,
    const domain::TweakId& tweakId,
    QString code,
    QString message)
{
    result.issues.append(PlanIssue{
        .code = std::move(code),
        .message = std::move(message),
        .tweakId = tweakId,
    });
}

} // namespace

PlanBuildResult PlanBuilder::build(
    const content::TweakCatalog& catalog,
    const TweakQueue& queue,
    const QHash<domain::TweakId, domain::DetectedState>& detected,
    const domain::SystemProfile& profile) const
{
    PlanBuildResult result;
    ExecutionPlan plan{
        .schemaVersion = executionPlanSchemaVersion,
        .transactionId = QUuid::createUuid(),
        .profile = profile,
        .createdAtUtc = QDateTime::currentDateTimeUtc(),
    };

    auto items = queue.items();
    std::sort(items.begin(), items.end(), [](const QueueItem& left, const QueueItem& right) {
        return left.tweakId.toString() < right.tweakId.toString();
    });

    for (const auto& item : items) {
        const auto* tweak = catalog.find(item.tweakId);
        if (!tweak) {
            addIssue(result, item.tweakId, u"tweak.unknown"_s, u"Твик из очереди отсутствует в каталоге."_s);
            continue;
        }

        const auto compatibility = detection::evaluate(*tweak, profile);
        if (!compatibility.supported) {
            addIssue(result, item.tweakId, compatibility.reasonCode, compatibility.explanation);
            continue;
        }

        const auto detectedIterator = detected.constFind(item.tweakId);
        if (detectedIterator == detected.cend()) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Фактическое состояние не было определено."_s);
            continue;
        }
        const auto& current = *detectedIterator;
        if (current.status == domain::DetectionStatus::Unsupported) {
            addIssue(result, item.tweakId, u"state.unsupported"_s, u"Параметр не поддерживается текущей системой."_s);
            continue;
        }
        if (current.status == domain::DetectionStatus::Unknown
            || current.status == domain::DetectionStatus::Mixed) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Исходное состояние нельзя определить однозначно."_s);
            continue;
        }
        if (current.stateId == item.targetState) {
            continue;
        }
        if (current.fingerprint.isEmpty()) {
            addIssue(result, item.tweakId, u"state.fingerprint_missing"_s, u"Нет отпечатка исходного состояния."_s);
            continue;
        }

        const auto* target = findState(*tweak, item.targetState);
        if (!target) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Целевое состояние отсутствует в определении."_s);
            continue;
        }

        for (const auto& operation : target->operations) {
            if (const auto* registry = std::get_if<domain::SetRegistryDwordOperation>(&operation)) {
                plan.operations.append(PlannedRegistryDwordChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *registry,
                    .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart,
                });
            }
        }
        if (restartRank(tweak->restart) > restartRank(plan.restart)) {
            plan.restart = tweak->restart;
        }
    }

    if (!result.issues.isEmpty()) {
        return result;
    }
    plan.summary = plan.operations.isEmpty()
        ? u"Изменения не требуются."_s
        : u"Будет применено операций: "_s + QString::number(plan.operations.size()) + u"."_s;
    result.plan = std::move(plan);
    return result;
}

} // namespace tweakopedia::planning
