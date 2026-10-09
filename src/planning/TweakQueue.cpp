#include "planning/TweakQueue.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::planning {

QueueChangeResult TweakQueue::setTarget(
    const domain::TweakDefinition& tweak,
    QStringView targetState,
    const QVariantMap& inputs)
{
    if (!tweak.supportsTargetState(targetState)) {
        return {.accepted = false, .errorCode = u"state.unknown"_s};
    }

    const auto state = std::find_if(
        tweak.states.cbegin(), tweak.states.cend(),
        [targetState](const auto& candidate) { return candidate.id == targetState; });
    const auto referenced = domain::referencedInputIds(*state);
    QVector<domain::TweakInputDefinition> activeDefinitions;
    activeDefinitions.reserve(referenced.size());
    for (const auto& definition : tweak.inputs) {
        if (referenced.contains(definition.id)) activeDefinitions.append(definition);
    }
    const auto normalized = domain::normalizeTweakInputs(activeDefinitions, inputs);
    if (!normalized.values) {
        return {
            .accepted = false,
            .errorCode = normalized.issues.isEmpty()
                ? u"input.invalid"_s : normalized.issues.first().code,
        };
    }

    for (auto& item : items_) {
        if (item.tweakId == tweak.id) {
            item.targetState = targetState.toString();
            item.inputs = *normalized.values;
            return {.accepted = true};
        }
    }
    items_.append(QueueItem{
        .tweakId = tweak.id,
        .targetState = targetState.toString(),
        .inputs = *normalized.values,
    });
    return {.accepted = true};
}

bool TweakQueue::remove(const domain::TweakId& tweakId)
{
    for (auto iterator = items_.begin(); iterator != items_.end(); ++iterator) {
        if (iterator->tweakId == tweakId) {
            items_.erase(iterator);
            return true;
        }
    }
    return false;
}

bool TweakQueue::isEmpty() const noexcept
{
    return items_.isEmpty();
}

qsizetype TweakQueue::size() const noexcept
{
    return items_.size();
}

const QVector<QueueItem>& TweakQueue::items() const noexcept
{
    return items_;
}

} // namespace tweakopedia::planning
