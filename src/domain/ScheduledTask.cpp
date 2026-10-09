#include "domain/ScheduledTask.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {

QJsonObject ScheduledTaskSnapshot::toJson() const
{
    return {
        {u"type"_s, u"scheduled_task"_s},
        {u"folder"_s, location.folder},
        {u"name"_s, location.name},
        {u"enabled"_s, enabled},
    };
}

std::optional<ScheduledTaskSnapshot> ScheduledTaskSnapshot::fromJson(const QJsonObject& object)
{
    if (object.value(u"type"_s).toString() != u"scheduled_task"_s
        || !object.value(u"enabled"_s).isBool()) return std::nullopt;
    const auto folder = object.value(u"folder"_s).toString();
    const auto name = object.value(u"name"_s).toString();
    if (folder.isEmpty() || name.isEmpty()) return std::nullopt;
    return ScheduledTaskSnapshot{
        .location = {.folder = folder, .name = name},
        .enabled = object.value(u"enabled"_s).toBool(),
    };
}

} // namespace tweakopedia::domain
