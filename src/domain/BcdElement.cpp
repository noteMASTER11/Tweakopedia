#include "domain/BcdElement.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {
namespace {

QString kindName(BcdValueKind kind)
{
    switch (kind) {
    case BcdValueKind::Boolean: return u"boolean"_s;
    case BcdValueKind::Integer: return u"integer"_s;
    case BcdValueKind::String: return u"string"_s;
    }
    return {};
}

std::optional<BcdValueKind> parseKind(QStringView name)
{
    if (name == u"boolean") return BcdValueKind::Boolean;
    if (name == u"integer") return BcdValueKind::Integer;
    if (name == u"string") return BcdValueKind::String;
    return std::nullopt;
}

} // namespace

bool bcdValueMatchesKind(const BcdValue& value, BcdValueKind kind)
{
    return (kind == BcdValueKind::Boolean && std::holds_alternative<bool>(value))
        || (kind == BcdValueKind::Integer && std::holds_alternative<quint64>(value))
        || (kind == BcdValueKind::String && std::holds_alternative<QString>(value));
}

bool isWhitelistedBcdElement(const BcdElementSpec& spec)
{
    static const QHash<quint32, BcdValueKind> allowed{
        {0x260000A5, BcdValueKind::Boolean},
        {0x25000020, BcdValueKind::Integer},
        {0x250000C2, BcdValueKind::Integer},
        {0x25000004, BcdValueKind::Integer},
        {0x12000004, BcdValueKind::String},
    };
    const auto object = spec.objectId.toLower();
    return (object == u"{current}"_s || object == u"{bootmgr}"_s)
        && allowed.contains(spec.elementType)
        && allowed.value(spec.elementType) == spec.valueKind;
}

QJsonObject bcdValueToJson(const BcdValue& value)
{
    return std::visit([](const auto& typed) -> QJsonObject {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, bool>) {
            return {{u"kind"_s, u"boolean"_s}, {u"value"_s, typed}};
        } else if constexpr (std::is_same_v<T, quint64>) {
            return {{u"kind"_s, u"integer"_s},
                    {u"value"_s, QString::number(typed)}};
        } else {
            return {{u"kind"_s, u"string"_s}, {u"value"_s, typed}};
        }
    }, value);
}

std::optional<BcdValue> bcdValueFromJson(
    const QJsonObject& object, BcdValueKind expectedKind)
{
    const auto encodedKind = parseKind(object.value(u"kind"_s).toString());
    if (!encodedKind || *encodedKind != expectedKind) return std::nullopt;
    const auto encoded = object.value(u"value"_s);
    switch (expectedKind) {
    case BcdValueKind::Boolean:
        if (encoded.isBool()) return BcdValue{encoded.toBool()};
        break;
    case BcdValueKind::Integer: {
        bool ok{};
        const auto value = encoded.toString().toULongLong(&ok, 10);
        if (ok) return BcdValue{value};
        break;
    }
    case BcdValueKind::String:
        if (encoded.isString()) return BcdValue{encoded.toString()};
        break;
    }
    return std::nullopt;
}

QJsonObject BcdElementSnapshot::toJson() const
{
    QJsonObject result{
        {u"type"_s, u"bcd.element"_s},
        {u"object_id"_s, spec.objectId},
        {u"element_type"_s, QString::number(spec.elementType)},
        {u"value_kind"_s, kindName(spec.valueKind)},
        {u"existed"_s, existed},
    };
    if (value) result.insert(u"value"_s, bcdValueToJson(*value));
    return result;
}

std::optional<BcdElementSnapshot> BcdElementSnapshot::fromJson(const QJsonObject& object)
{
    if (object.value(u"type"_s).toString() != u"bcd.element"_s
        || !object.value(u"existed"_s).isBool()) return std::nullopt;
    bool typeOk{};
    const auto elementType = object.value(u"element_type"_s).toString().toUInt(&typeOk, 10);
    const auto kind = parseKind(object.value(u"value_kind"_s).toString());
    const auto objectId = object.value(u"object_id"_s).toString();
    if (!typeOk || !kind || objectId.isEmpty()) return std::nullopt;
    const auto existed = object.value(u"existed"_s).toBool();
    std::optional<BcdValue> value;
    if (existed) {
        if (!object.value(u"value"_s).isObject()) return std::nullopt;
        value = bcdValueFromJson(object.value(u"value"_s).toObject(), *kind);
        if (!value) return std::nullopt;
    }
    return BcdElementSnapshot{
        .spec = {.objectId = objectId, .elementType = elementType, .valueKind = *kind},
        .existed = existed,
        .value = std::move(value),
    };
}

} // namespace tweakopedia::domain
