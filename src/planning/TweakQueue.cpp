#include "planning/TweakQueue.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::planning {

QueueChangeResult TweakQueue::setTarget(
    const domain::TweakDefinition& tweak,
    QStringView targetState)
{
    if (!tweak.supportsTargetState(targetState)) {
        return {.accepted = false, .errorCode = u"state.unknown"_s};
    }

    for (auto& item : items_) {
        if (item.tweakId == tweak.id) {
            item.targetState = targetState.toString();
            return {.accepted = true};
        }
    }
    items_.append(QueueItem{.tweakId = tweak.id, .targetState = targetState.toString()});
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
