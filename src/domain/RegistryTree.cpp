#include "domain/RegistryTree.h"

#include <QJsonArray>

#include <limits>

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {

QJsonObject RegistryTreeSnapshot::toJson() const
{
    QJsonArray nodesJson;
    for (const auto& node : nodes) {
        QJsonArray valuesJson;
        for (const auto& value : node.values) {
            valuesJson.append(QJsonObject{
                {u"name"_s, value.name},
                {u"nativeType"_s, static_cast<qint64>(value.value.nativeType)},
                {u"rawBase64"_s, QString::fromLatin1(value.value.rawValue.toBase64())},
            });
        }
        nodesJson.append(QJsonObject{
            {u"path"_s, node.relativePath},
            {u"values"_s, valuesJson},
        });
    }
    return {
        {u"type"_s, u"registry.tree"_s},
        {u"hive"_s, static_cast<int>(location.hive)},
        {u"key"_s, location.key},
        {u"view"_s, static_cast<int>(location.view)},
        {u"existed"_s, existed},
        {u"nodes"_s, nodesJson},
    };
}

std::optional<RegistryTreeSnapshot> RegistryTreeSnapshot::fromJson(const QJsonObject& object)
{
    const auto hive = object.value(u"hive"_s).toInt(-1);
    const auto view = object.value(u"view"_s).toInt(-1);
    const auto key = object.value(u"key"_s).toString();
    if (object.value(u"type"_s).toString() != u"registry.tree"
        || hive < 0 || hive > static_cast<int>(RegistryHive::Users)
        || view < 0 || view > static_cast<int>(RegistryView::Registry64)
        || key.isEmpty() || !object.value(u"existed"_s).isBool()
        || !object.value(u"nodes"_s).isArray()) {
        return std::nullopt;
    }

    RegistryTreeSnapshot result{
        .location = {
            .hive = static_cast<RegistryHive>(hive),
            .key = key,
            .view = static_cast<RegistryView>(view),
        },
        .existed = object.value(u"existed"_s).toBool(),
    };
    for (const auto& nodeValue : object.value(u"nodes"_s).toArray()) {
        if (!nodeValue.isObject()) return std::nullopt;
        const auto nodeObject = nodeValue.toObject();
        if (!nodeObject.value(u"values"_s).isArray()) return std::nullopt;
        RegistryTreeNode node{.relativePath = nodeObject.value(u"path"_s).toString()};
        for (const auto& valueValue : nodeObject.value(u"values"_s).toArray()) {
            if (!valueValue.isObject()) return std::nullopt;
            const auto valueObject = valueValue.toObject();
            const auto nativeType = valueObject.value(u"nativeType"_s).toInteger(-1);
            const auto raw = QByteArray::fromBase64Encoding(
                valueObject.value(u"rawBase64"_s).toString().toLatin1(),
                QByteArray::AbortOnBase64DecodingErrors);
            if (nativeType < 0 || nativeType > std::numeric_limits<quint32>::max()
                || !raw) {
                return std::nullopt;
            }
            node.values.append({
                .name = valueObject.value(u"name"_s).toString(),
                .value = {
                    .nativeType = static_cast<quint32>(nativeType),
                    .rawValue = raw.decoded,
                },
            });
        }
        result.nodes.append(std::move(node));
    }
    if (!result.existed && !result.nodes.isEmpty()) return std::nullopt;
    return result;
}

} // namespace tweakopedia::domain
