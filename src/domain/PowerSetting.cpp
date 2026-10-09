#include "domain/PowerSetting.h"

#include <QUuid>

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {

bool isValidPowerGuid(QStringView value)
{
    const QUuid parsed(value.toString());
    return !parsed.isNull();
}

bool isValidPowerLocation(const PowerSettingLocation& location)
{
    return (location.scheme == u"active" || isValidPowerGuid(location.scheme))
        && isValidPowerGuid(location.subgroup) && isValidPowerGuid(location.setting);
}

QJsonObject PowerSettingSnapshot::toJson() const
{
    return {
        {u"type"_s, u"power.setting"_s},
        {u"scheme"_s, location.scheme},
        {u"resolved_scheme"_s, resolvedScheme},
        {u"subgroup"_s, location.subgroup},
        {u"setting"_s, location.setting},
        {u"source"_s, location.source == PowerSource::Ac ? u"ac"_s : u"dc"_s},
        {u"index"_s, QString::number(index)},
    };
}

std::optional<PowerSettingSnapshot> PowerSettingSnapshot::fromJson(const QJsonObject& object)
{
    if (object.value(u"type"_s).toString() != u"power.setting"_s) return std::nullopt;
    const auto sourceName = object.value(u"source"_s).toString();
    if (sourceName != u"ac" && sourceName != u"dc") return std::nullopt;
    bool indexOk{};
    const auto index = object.value(u"index"_s).toString().toUInt(&indexOk, 10);
    PowerSettingSnapshot snapshot{
        .location = {
            .scheme = object.value(u"scheme"_s).toString(),
            .subgroup = object.value(u"subgroup"_s).toString(),
            .setting = object.value(u"setting"_s).toString(),
            .source = sourceName == u"ac" ? PowerSource::Ac : PowerSource::Dc,
        },
        .resolvedScheme = object.value(u"resolved_scheme"_s).toString(),
        .index = index,
    };
    if (!indexOk || !isValidPowerLocation(snapshot.location)
        || !isValidPowerGuid(snapshot.resolvedScheme)) return std::nullopt;
    return snapshot;
}

} // namespace tweakopedia::domain
