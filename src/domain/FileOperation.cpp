#include "domain/FileOperation.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {

QJsonObject FileSnapshot::toJson() const
{
    return {
        {u"type"_s, u"file"_s},
        {u"destination"_s, destination},
        {u"existed"_s, existed},
        {u"backup"_s, backupRelativePath},
        {u"size"_s, QString::number(size)},
        {u"sha256"_s, QString::fromLatin1(sha256)},
    };
}

std::optional<FileSnapshot> FileSnapshot::fromJson(const QJsonObject& object)
{
    if (object.value(u"type"_s).toString() != u"file"_s) return std::nullopt;
    bool ok{};
    const auto size = object.value(u"size"_s).toString().toULongLong(&ok);
    const auto destination = object.value(u"destination"_s).toString();
    if (!ok || destination.isEmpty()) return std::nullopt;
    return FileSnapshot{
        .destination = destination,
        .existed = object.value(u"existed"_s).toBool(),
        .backupRelativePath = object.value(u"backup"_s).toString(),
        .size = size,
        .sha256 = object.value(u"sha256"_s).toString().toLatin1(),
    };
}

} // namespace tweakopedia::domain
