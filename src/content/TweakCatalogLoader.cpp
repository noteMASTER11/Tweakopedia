#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <limits>

using namespace Qt::StringLiterals;

namespace tweakopedia::content {
namespace {

using domain::BuildRange;
using domain::CpuArchitecture;
using domain::Impact;
using domain::OperationSpec;
using domain::RegistryDwordDetection;
using domain::RegistryHive;
using domain::RegistryKeyLocation;
using domain::RegistryLocation;
using domain::RegistryValueDetection;
using domain::RegistryValueSpec;
using domain::RegistryView;
using domain::RestartRequirement;
using domain::Reversibility;
using domain::SetRegistryDwordOperation;
using domain::SetRegistryValueOperation;
using domain::DeleteRegistryValueOperation;
using domain::CreateRegistryKeyOperation;
using domain::DeleteRegistryTreeOperation;
using domain::SetFeatureStateOperation;
using domain::FeatureEnabledState;
using domain::TweakDefinition;
using domain::TweakId;
using domain::TweakInputChoice;
using domain::TweakInputDefinition;
using domain::TweakInputType;
using domain::TweakKind;
using domain::TweakStateDefinition;
using domain::WindowsDefaultRule;
using domain::WindowsFamily;

struct ParseContext {
    QString filePath;
    QVector<CatalogError>* errors{};
};

QString nodeText(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        return {};
    }
    return QString::fromStdString(node.Scalar());
}

void addError(ParseContext& context, QString code, QString message, const YAML::Node& node)
{
    const auto mark = node.Mark();
    context.errors->append(CatalogError{
        .code = std::move(code),
        .message = std::move(message),
        .filePath = context.filePath,
        .line = mark.is_null() ? 1 : mark.line + 1,
        .column = mark.is_null() ? 1 : mark.column + 1,
    });
}

void validateKeys(
    ParseContext& context,
    const YAML::Node& map,
    const QSet<QString>& allowed)
{
    if (!map || !map.IsMap()) {
        addError(context, u"type.map_required"_s, u"Ожидался объект YAML."_s, map);
        return;
    }

    for (const auto& entry : map) {
        const auto key = nodeText(entry.first);
        if (!allowed.contains(key)) {
            addError(context, u"field.unknown"_s, u"Неизвестное поле: "_s + key, entry.first);
        }
    }
}

YAML::Node required(ParseContext& context, const YAML::Node& map, const char* key)
{
    const auto node = map[key];
    if (!node) {
        addError(
            context,
            u"field.required"_s,
            u"Отсутствует обязательное поле: "_s + QString::fromLatin1(key),
            map);
    }
    return node;
}

QString requiredString(ParseContext& context, const YAML::Node& map, const char* key)
{
    const auto node = required(context, map, key);
    if (!node) {
        return {};
    }
    if (!node.IsScalar() || nodeText(node).trimmed().isEmpty()) {
        addError(context, u"type.string_required"_s, u"Поле должно быть непустой строкой."_s, node);
        return {};
    }
    return nodeText(node).trimmed();
}

QString normalizedRegistryKey(QString key)
{
    key = key.trimmed();
    key.replace(u'/', u'\\');
    while (key.contains(u"\\\\")) {
        key.replace(u"\\\\"_s, u"\\"_s);
    }
    while (key.startsWith(u'\\')) {
        key.remove(0, 1);
    }
    while (key.endsWith(u'\\')) {
        key.chop(1);
    }
    return key;
}

std::optional<RegistryHive> parseHive(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"HKCU") return RegistryHive::CurrentUser;
    if (value == u"HKLM") return RegistryHive::LocalMachine;
    if (value == u"HKCR") return RegistryHive::ClassesRoot;
    if (value == u"HKU") return RegistryHive::Users;
    addError(context, u"registry.hive_unknown"_s, u"Неизвестный hive реестра."_s, node);
    return std::nullopt;
}

std::optional<RegistryView> parseView(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"default") return RegistryView::Default;
    if (value == u"registry32") return RegistryView::Registry32;
    if (value == u"registry64") return RegistryView::Registry64;
    addError(context, u"registry.view_unknown"_s, u"Неизвестное представление реестра."_s, node);
    return std::nullopt;
}

std::optional<RegistryLocation> parseRegistryLocation(ParseContext& context, const YAML::Node& map)
{
    const auto hiveNode = required(context, map, "hive");
    const auto key = requiredString(context, map, "key");
    const auto valueName = requiredString(context, map, "value_name");
    const auto viewNode = required(context, map, "view");
    if (!hiveNode || key.isEmpty() || valueName.isEmpty() || !viewNode) {
        return std::nullopt;
    }

    const auto hive = parseHive(context, hiveNode);
    const auto view = parseView(context, viewNode);
    if (!hive || !view) {
        return std::nullopt;
    }

    const auto normalizedKey = normalizedRegistryKey(key);
    if (normalizedKey.isEmpty()) {
        addError(context, u"registry.key_invalid"_s, u"Путь раздела реестра пуст."_s, map["key"]);
        return std::nullopt;
    }
    if (normalizedKey.contains(u"${"_s) || valueName.contains(u"${"_s)) {
        addError(context, u"registry.location_dynamic"_s,
                 u"Путь и имя значения реестра должны быть фиксированными."_s, map);
        return std::nullopt;
    }

    return RegistryLocation{
        .hive = *hive,
        .key = normalizedKey,
        .valueName = valueName,
        .view = *view,
    };
}

std::optional<RegistryKeyLocation> parseRegistryKeyLocation(
    ParseContext& context,
    const YAML::Node& map)
{
    const auto hiveNode = required(context, map, "hive");
    const auto key = requiredString(context, map, "key");
    const auto viewNode = required(context, map, "view");
    if (!hiveNode || key.isEmpty() || !viewNode) return std::nullopt;
    const auto hive = parseHive(context, hiveNode);
    const auto view = parseView(context, viewNode);
    const auto normalizedKey = normalizedRegistryKey(key);
    if (!hive || !view || normalizedKey.isEmpty()) {
        if (normalizedKey.isEmpty()) {
            addError(context, u"registry.key_invalid"_s,
                     u"Путь раздела реестра пуст."_s, map["key"]);
        }
        return std::nullopt;
    }
    if (normalizedKey.contains(u"${"_s)) {
        addError(context, u"registry.location_dynamic"_s,
                 u"Путь раздела реестра должен быть фиксированным."_s, map);
        return std::nullopt;
    }
    return RegistryKeyLocation{
        .hive = *hive,
        .key = normalizedKey,
        .view = *view,
    };
}

std::optional<quint32> parseDword(ParseContext& context, const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        addError(context, u"value.invalid_dword"_s, u"DWORD должен быть целым числом."_s, node);
        return std::nullopt;
    }

    bool ok = false;
    const auto value = nodeText(node).toULongLong(&ok, 0);
    if (!ok || value > std::numeric_limits<quint32>::max()) {
        addError(context, u"value.invalid_dword"_s, u"DWORD выходит за диапазон 0..4294967295."_s, node);
        return std::nullopt;
    }
    return static_cast<quint32>(value);
}

std::optional<quint64> parseQword(ParseContext& context, const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        addError(context, u"value.invalid_qword"_s, u"QWORD должен быть целым числом."_s, node);
        return std::nullopt;
    }
    bool ok = false;
    const auto value = nodeText(node).toULongLong(&ok, 0);
    if (!ok) {
        addError(context, u"value.invalid_qword"_s, u"QWORD выходит за допустимый диапазон."_s, node);
        return std::nullopt;
    }
    return value;
}

std::optional<domain::BcdValueKind> parseBcdValueKind(
    ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"boolean") return domain::BcdValueKind::Boolean;
    if (value == u"integer") return domain::BcdValueKind::Integer;
    if (value == u"string") return domain::BcdValueKind::String;
    addError(context, u"bcd.value_kind_invalid"_s,
             u"Тип BCD-значения должен быть boolean, integer или string."_s, node);
    return std::nullopt;
}

std::optional<domain::BcdValue> parseBcdValue(
    ParseContext& context, const YAML::Node& node, domain::BcdValueKind kind)
{
    if (!node || !node.IsScalar()) {
        addError(context, u"bcd.value_invalid"_s, u"BCD-значение должно быть скаляром."_s, node);
        return std::nullopt;
    }
    if (kind == domain::BcdValueKind::Boolean) {
        try {
            return domain::BcdValue{node.as<bool>()};
        } catch (const YAML::Exception&) {
            addError(context, u"bcd.value_invalid"_s,
                     u"Boolean BCD-значение должно быть true либо false."_s, node);
            return std::nullopt;
        }
    }
    if (kind == domain::BcdValueKind::Integer) {
        bool ok{};
        const auto value = nodeText(node).toULongLong(&ok, 0);
        if (ok) return domain::BcdValue{value};
        addError(context, u"bcd.value_invalid"_s,
                 u"Integer BCD-значение выходит за допустимый диапазон."_s, node);
        return std::nullopt;
    }
    return domain::BcdValue{nodeText(node)};
}

std::optional<domain::BcdElementSpec> parseBcdSpec(
    ParseContext& context, const YAML::Node& node)
{
    const auto objectId = requiredString(context, node, "object_id");
    const auto typeNode = required(context, node, "element_type");
    const auto kindNode = required(context, node, "value_kind");
    const auto elementType = typeNode ? parseDword(context, typeNode) : std::nullopt;
    const auto kind = kindNode ? parseBcdValueKind(context, kindNode) : std::nullopt;
    if (objectId.contains(u"${"_s) || !elementType || !kind) return std::nullopt;
    const domain::BcdElementSpec spec{
        .objectId = objectId, .elementType = *elementType, .valueKind = *kind};
    if (!domain::isWhitelistedBcdElement(spec)) {
        addError(context, u"bcd.element_not_whitelisted"_s,
                 u"BCD-объект или element type не входит в whitelist."_s, node);
        return std::nullopt;
    }
    return spec;
}

std::optional<domain::PowerSettingLocation> parsePowerLocation(
    ParseContext& context, const YAML::Node& node)
{
    const auto scheme = requiredString(context, node, "scheme").toLower();
    const auto subgroup = requiredString(context, node, "subgroup").toLower();
    const auto setting = requiredString(context, node, "setting").toLower();
    const auto sourceName = requiredString(context, node, "source");
    const auto source = sourceName == u"ac" ? std::optional{domain::PowerSource::Ac}
        : sourceName == u"dc" ? std::optional{domain::PowerSource::Dc} : std::nullopt;
    if (!source) addError(context, u"power.source_invalid"_s,
                          u"Источник питания должен быть ac или dc."_s, node["source"]);
    if (!source) return std::nullopt;
    domain::PowerSettingLocation location{scheme, subgroup, setting, *source};
    if (!domain::isValidPowerLocation(location)) {
        addError(context, u"power.location_invalid"_s,
                 u"GUID параметра питания недопустим."_s, node);
        return std::nullopt;
    }
    return location;
}

std::optional<domain::WindowsComponentTarget> parseWindowsComponentTarget(
    ParseContext& context, const YAML::Node& node,
    std::optional<domain::WindowsComponentKind> impliedKind = std::nullopt)
{
    auto kind = impliedKind;
    if (!kind) kind = domain::parseWindowsComponentKind(requiredString(context, node, "component_kind"));
    const auto name = requiredString(context, node, "name");
    if (!kind) {
        addError(context, u"windows_component.kind_invalid"_s,
                 u"Тип компонента должен быть feature или capability."_s, node);
        return std::nullopt;
    }
    domain::WindowsComponentTarget target{*kind, name};
    if (!domain::isValidWindowsComponentTarget(target)) {
        addError(context, u"windows_component.name_invalid"_s,
                 u"Имя компонента Windows недопустимо."_s, node);
        return std::nullopt;
    }
    return target;
}

std::optional<RegistryValueSpec> parseRegistryValue(
    ParseContext& context,
    const YAML::Node& map)
{
    validateKeys(context, map, {u"value_type"_s, u"value"_s});
    const auto typeNode = required(context, map, "value_type");
    const auto valueNode = required(context, map, "value");
    if (!typeNode || !valueNode) return std::nullopt;

    const auto type = nodeText(typeNode);
    if (type == u"dword") {
        if (const auto value = parseDword(context, valueNode)) return RegistryValueSpec::dword(*value);
        return std::nullopt;
    }
    if (type == u"qword") {
        if (const auto value = parseQword(context, valueNode)) return RegistryValueSpec::qword(*value);
        return std::nullopt;
    }
    if (type == u"string" || type == u"expand_string") {
        if (!valueNode.IsScalar()) {
            addError(context, u"type.string_required"_s, u"Значение должно быть строкой."_s, valueNode);
            return std::nullopt;
        }
        return type == u"string"
            ? RegistryValueSpec::string(nodeText(valueNode))
            : RegistryValueSpec::expandString(nodeText(valueNode));
    }
    if (type == u"multi_string") {
        if (!valueNode.IsSequence()) {
            addError(context, u"type.sequence_required"_s, u"MULTI_SZ должен быть массивом строк."_s, valueNode);
            return std::nullopt;
        }
        QStringList values;
        for (const auto& entry : valueNode) {
            if (!entry.IsScalar()) {
                addError(context, u"type.string_required"_s, u"Элемент MULTI_SZ должен быть строкой."_s, entry);
                return std::nullopt;
            }
            values.append(nodeText(entry));
        }
        return RegistryValueSpec::multiString(values);
    }
    if (type == u"binary") {
        const auto text = nodeText(valueNode).trimmed();
        static const QRegularExpression hexPattern(u"^(?:[0-9a-fA-F]{2})*$"_s);
        if (!valueNode.IsScalar() || !hexPattern.match(text).hasMatch()) {
            addError(context, u"value.invalid_binary"_s, u"REG_BINARY должен быть строкой из пар hex-цифр."_s, valueNode);
            return std::nullopt;
        }
        return RegistryValueSpec::binary(QByteArray::fromHex(text.toLatin1()));
    }

    addError(context, u"registry.value_type_unknown"_s,
             u"Неизвестный тип значения реестра."_s, typeNode);
    return std::nullopt;
}

std::optional<QString> parseInputReference(ParseContext& context, const YAML::Node& node)
{
    if (!node || !node.IsScalar()) return std::nullopt;
    const auto text = nodeText(node);
    static const QRegularExpression exact(u"^\\$\\{input\\.([a-z][a-z0-9_-]*)\\}$"_s);
    const auto match = exact.match(text);
    if (match.hasMatch()) return match.captured(1);
    if (text.contains(u"${input."_s)) {
        addError(context, u"input.reference_invalid"_s,
                 u"Ссылка на поле должна занимать значение операции целиком."_s, node);
    }
    return std::nullopt;
}

std::optional<RegistryValueSpec> emptyRegistryValueForType(
    ParseContext& context, const YAML::Node& typeNode)
{
    const auto type = nodeText(typeNode);
    if (type == u"dword") return RegistryValueSpec::dword(0);
    if (type == u"qword") return RegistryValueSpec::qword(0);
    if (type == u"string") return RegistryValueSpec::string({});
    if (type == u"expand_string") return RegistryValueSpec::expandString({});
    if (type == u"binary") return RegistryValueSpec::binary({});
    addError(context, u"input.payload_type_unsupported"_s,
             u"Параметризованное значение не поддерживает этот тип реестра."_s, typeNode);
    return std::nullopt;
}

std::optional<OperationSpec> parseOperation(ParseContext& context, const YAML::Node& node)
{
    validateKeys(context, node, {
        u"type"_s, u"hive"_s, u"key"_s, u"value_name"_s, u"view"_s, u"value"_s,
        u"value_type"_s, u"feature_id"_s, u"state"_s,
        u"input"_s, u"destination"_s, u"folder"_s, u"name"_s, u"enabled"_s,
        u"object_id"_s, u"element_type"_s, u"value_kind"_s,
        u"scheme"_s, u"subgroup"_s, u"setting"_s, u"source"_s, u"index"_s,
        u"component_kind"_s,
    });
    const auto typeNode = required(context, node, "type");
    if (!typeNode) {
        return std::nullopt;
    }

    const auto type = nodeText(typeNode);
    if (type == u"windows_feature.set_state" || type == u"windows_capability.set_state") {
        const auto kind = type == u"windows_feature.set_state"
            ? domain::WindowsComponentKind::Feature : domain::WindowsComponentKind::Capability;
        const auto target = parseWindowsComponentTarget(context, node, kind);
        const auto state = domain::parseWindowsComponentState(requiredString(context, node, "state"));
        if (!state || *state == domain::WindowsComponentState::Unsupported
            || (kind == domain::WindowsComponentKind::Capability
                && *state == domain::WindowsComponentState::Disabled)) {
            addError(context, u"windows_component.state_invalid"_s,
                     u"Целевое состояние компонента недопустимо."_s, node["state"]);
            return std::nullopt;
        }
        if (target) return domain::SetWindowsComponentStateOperation{*target, *state};
        return std::nullopt;
    }
    if (type == u"power.set_index") {
        const auto location = parsePowerLocation(context, node);
        const auto indexNode = required(context, node, "index");
        const auto index = indexNode ? parseDword(context, indexNode) : std::nullopt;
        if (location && index) return domain::SetPowerSettingOperation{*location, *index};
        return std::nullopt;
    }
    if (type == u"bcd.set_element" || type == u"bcd.delete_element") {
        const auto spec = parseBcdSpec(context, node);
        if (!spec) return std::nullopt;
        std::optional<domain::BcdValue> value;
        if (type == u"bcd.set_element") {
            value = parseBcdValue(context, required(context, node, "value"), spec->valueKind);
            if (!value) return std::nullopt;
        }
        return domain::SetBcdElementOperation{.spec = *spec, .value = std::move(value)};
    }
    if (type == u"scheduled_task.set_enabled") {
        const auto folder = requiredString(context, node, "folder");
        const auto name = requiredString(context, node, "name");
        const auto enabledNode = required(context, node, "enabled");
        if (folder.contains(u"${"_s) || !folder.startsWith(u'\\')
            || folder.contains(u".."_s) || name.contains(u"${"_s)
            || name.trimmed().isEmpty() || name.contains(u'\\') || name.contains(u'/')) {
            addError(context, u"scheduled_task.location_invalid"_s,
                     u"Папка и имя задачи должны задавать фиксированную задачу планировщика."_s,
                     node);
            return std::nullopt;
        }
        bool enabled{};
        try {
            enabled = enabledNode.as<bool>();
        } catch (const YAML::Exception&) {
            addError(context, u"type.bool_required"_s,
                     u"enabled должен быть true либо false."_s, enabledNode);
            return std::nullopt;
        }
        return domain::SetScheduledTaskEnabledOperation{
            .location = {.folder = folder, .name = name}, .enabled = enabled};
    }
    if (type == u"file.copy" || type == u"file.replace" || type == u"file.delete") {
        const auto destination = requiredString(context, node, "destination");
        if (destination.contains(u"${"_s) || !QDir::isAbsolutePath(destination)
            || QDir::cleanPath(destination) != destination) {
            addError(context, u"file.destination_invalid"_s,
                     u"Путь назначения файла должен быть фиксированным абсолютным путём."_s,
                     node["destination"]);
            return std::nullopt;
        }
        QString inputId;
        if (type != u"file.delete") inputId = requiredString(context, node, "input");
        if (inputId.contains(u"${"_s)) {
            const auto reference = parseInputReference(context, node["input"]);
            if (!reference) return std::nullopt;
            inputId = *reference;
        }
        if (type != u"file.delete" && inputId.isEmpty()) return std::nullopt;
        return domain::FileOperationDefinition{
            .kind = type == u"file.copy" ? domain::FileOperationKind::Copy
                : type == u"file.replace" ? domain::FileOperationKind::Replace
                                           : domain::FileOperationKind::Delete,
            .inputId = inputId,
            .destination = destination,
        };
    }
    if (type == u"registry.create_key" || type == u"registry.delete_key") {
        if (const auto location = parseRegistryKeyLocation(context, node)) {
            return type == u"registry.create_key"
                ? OperationSpec{CreateRegistryKeyOperation{.location = *location}}
                : OperationSpec{DeleteRegistryTreeOperation{.location = *location}};
        }
        return std::nullopt;
    }
    if (type == u"feature.set_state" || type == u"feature.reset") {
        const auto featureIdNode = required(context, node, "feature_id");
        const auto featureId = featureIdNode ? parseDword(context, featureIdNode) : std::nullopt;
        if (!featureId || *featureId == 0) {
            if (featureId && *featureId == 0) {
                addError(context, u"feature.id_invalid"_s, u"Feature ID должен быть больше нуля."_s, featureIdNode);
            }
            return std::nullopt;
        }
        if (type == u"feature.reset") {
            return SetFeatureStateOperation{.featureId = *featureId, .state = FeatureEnabledState::Default};
        }
        const auto stateNode = required(context, node, "state");
        const auto state = nodeText(stateNode);
        if (state == u"enabled") {
            return SetFeatureStateOperation{.featureId = *featureId, .state = FeatureEnabledState::Enabled};
        }
        if (state == u"disabled") {
            return SetFeatureStateOperation{.featureId = *featureId, .state = FeatureEnabledState::Disabled};
        }
        addError(context, u"feature.state_invalid"_s, u"Состояние функции должно быть enabled или disabled."_s, stateNode);
        return std::nullopt;
    }

    if (type == u"registry.delete_value") {
        if (const auto location = parseRegistryLocation(context, node)) {
            return DeleteRegistryValueOperation{.location = *location};
        }
        return std::nullopt;
    }

    if (type == u"registry.set_value") {
        const auto location = parseRegistryLocation(context, node);
        const auto reference = parseInputReference(context, node["value"]);
        if (reference) {
            const auto value = emptyRegistryValueForType(context, required(context, node, "value_type"));
            if (location && value) {
                return SetRegistryValueOperation{
                    .location = *location, .value = *value, .valueInput = *reference};
            }
            return std::nullopt;
        }
        YAML::Node valueMap(YAML::NodeType::Map);
        valueMap["value_type"] = node["value_type"];
        valueMap["value"] = node["value"];
        const auto value = parseRegistryValue(context, valueMap);
        if (location && value) {
            return SetRegistryValueOperation{.location = *location, .value = *value};
        }
        return std::nullopt;
    }

    if (type != u"registry.set_dword") {
        addError(context, u"operation.unknown"_s, u"Тип операции не поддерживается."_s, typeNode);
        return std::nullopt;
    }

    const auto location = parseRegistryLocation(context, node);
    const auto valueNode = required(context, node, "value");
    const auto reference = parseInputReference(context, valueNode);
    if (location && reference) {
        return SetRegistryDwordOperation{
            .location = *location, .value = 0, .valueInput = *reference};
    }
    const auto value = valueNode ? parseDword(context, valueNode) : std::nullopt;
    if (!location || !value) {
        return std::nullopt;
    }
    return SetRegistryDwordOperation{.location = *location, .value = *value};
}

std::optional<qint64> optionalSignedInteger(
    ParseContext& context, const YAML::Node& map, const char* key)
{
    const auto node = map[key];
    if (!node) return std::nullopt;
    try {
        return node.as<qint64>();
    } catch (const YAML::Exception&) {
        addError(context, u"type.integer_required"_s, u"Поле должно быть целым числом."_s, node);
        return std::nullopt;
    }
}

QVector<TweakInputDefinition> parseInputs(ParseContext& context, const YAML::Node& node)
{
    QVector<TweakInputDefinition> inputs;
    if (!node) return inputs;
    if (!node.IsMap()) {
        addError(context, u"type.map_required"_s, u"inputs должен быть объектом."_s, node);
        return inputs;
    }
    for (const auto& entry : node) {
        const auto id = nodeText(entry.first).trimmed();
        const auto value = entry.second;
        validateKeys(context, value, {
            u"type"_s, u"label"_s, u"required"_s, u"min_length"_s, u"max_length"_s,
            u"min"_s, u"max"_s, u"extensions"_s, u"max_size"_s, u"choices"_s,
        });
        TweakInputDefinition input{.id = id, .label = requiredString(context, value, "label")};
        const auto typeNode = required(context, value, "type");
        const auto type = nodeText(typeNode);
        if (type == u"text") input.type = TweakInputType::Text;
        else if (type == u"integer") input.type = TweakInputType::Integer;
        else if (type == u"file") input.type = TweakInputType::File;
        else if (type == u"choice") input.type = TweakInputType::Choice;
        else if (type == u"boolean") input.type = TweakInputType::Boolean;
        else addError(context, u"input.type_unknown"_s, u"Неизвестный тип поля ввода."_s, typeNode);

        if (const auto requiredNode = value["required"]) {
            try { input.required = requiredNode.as<bool>(); }
            catch (const YAML::Exception&) {
                addError(context, u"type.bool_required"_s,
                         u"required должен быть true либо false."_s, requiredNode);
            }
        }
        if (const auto parsed = optionalSignedInteger(context, value, "min_length")) {
            if (*parsed < 0) addError(context, u"input.constraint_invalid"_s,
                                      u"min_length не может быть отрицательным."_s, value["min_length"]);
            else input.minimumLength = static_cast<qsizetype>(*parsed);
        }
        if (const auto parsed = optionalSignedInteger(context, value, "max_length")) {
            if (*parsed < 0) addError(context, u"input.constraint_invalid"_s,
                                      u"max_length не может быть отрицательным."_s, value["max_length"]);
            else input.maximumLength = static_cast<qsizetype>(*parsed);
        }
        input.minimum = optionalSignedInteger(context, value, "min");
        input.maximum = optionalSignedInteger(context, value, "max");
        if (const auto maxSize = optionalSignedInteger(context, value, "max_size")) {
            if (*maxSize < 0) addError(context, u"input.constraint_invalid"_s,
                                       u"max_size не может быть отрицательным."_s, value["max_size"]);
            else input.maximumFileSize = static_cast<quint64>(*maxSize);
        }
        if (const auto extensions = value["extensions"]) {
            if (!extensions.IsSequence()) {
                addError(context, u"type.sequence_required"_s,
                         u"extensions должен быть массивом."_s, extensions);
            } else {
                for (const auto& extension : extensions) {
                    const auto text = nodeText(extension).trimmed().toLower();
                    if (text.isEmpty()) addError(context, u"type.string_required"_s,
                                                 u"Расширение должно быть строкой."_s, extension);
                    else input.allowedExtensions.append(text.startsWith(u'.') ? text.mid(1) : text);
                }
            }
        }
        if (const auto choices = value["choices"]) {
            if (!choices.IsMap()) {
                addError(context, u"type.map_required"_s, u"choices должен быть объектом."_s, choices);
            } else {
                for (const auto& choice : choices) {
                    input.choices.append(TweakInputChoice{
                        .value = nodeText(choice.first).trimmed(),
                        .label = nodeText(choice.second).trimmed(),
                    });
                }
            }
        }
        inputs.append(std::move(input));
    }
    return inputs;
}

QVector<TweakStateDefinition> parseStates(ParseContext& context, const YAML::Node& node)
{
    QVector<TweakStateDefinition> states;
    if (!node || !node.IsMap()) {
        addError(context, u"type.map_required"_s, u"states должен быть объектом."_s, node);
        return states;
    }

    for (const auto& entry : node) {
        const auto stateId = nodeText(entry.first);
        const auto stateNode = entry.second;
        validateKeys(context, stateNode, {u"title"_s, u"operations"_s});
        TweakStateDefinition state;
        state.id = stateId;
        state.title = requiredString(context, stateNode, "title");
        const auto operations = required(context, stateNode, "operations");
        if (operations && operations.IsSequence()) {
            for (const auto& operationNode : operations) {
                if (const auto operation = parseOperation(context, operationNode)) {
                    state.operations.append(*operation);
                }
            }
        } else if (operations) {
            addError(context, u"type.sequence_required"_s, u"operations должен быть массивом."_s, operations);
        }
        states.append(std::move(state));
    }
    return states;
}

std::optional<WindowsFamily> parseWindowsFamily(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"windows10") return WindowsFamily::Windows10;
    if (value == u"windows11") return WindowsFamily::Windows11;
    addError(context, u"compatibility.os_unknown"_s, u"Неизвестная версия Windows."_s, node);
    return std::nullopt;
}

void parseCompatibility(ParseContext& context, const YAML::Node& node, TweakDefinition& definition)
{
    validateKeys(context, node, {
        u"architectures"_s, u"os"_s, u"min_build"_s, u"max_build"_s, u"required_components"_s,
    });
    const auto architectures = required(context, node, "architectures");
    if (architectures && architectures.IsSequence()) {
        for (const auto& architecture : architectures) {
            if (nodeText(architecture) == u"x64") {
                definition.compatibility.architectures.append(CpuArchitecture::X64);
            } else {
                addError(context, u"compatibility.architecture_unknown"_s, u"Поддерживается только x64."_s, architecture);
            }
        }
    } else if (architectures) {
        addError(context, u"type.sequence_required"_s, u"architectures должен быть массивом."_s, architectures);
    }

    const auto systems = required(context, node, "os");
    if (systems && systems.IsSequence()) {
        for (const auto& system : systems) {
            if (const auto family = parseWindowsFamily(context, system)) {
                definition.compatibility.operatingSystems.append(*family);
            }
        }
    } else if (systems) {
        addError(context, u"type.sequence_required"_s, u"os должен быть массивом."_s, systems);
    }

    const auto minimumBuild = required(context, node, "min_build");
    if (minimumBuild) {
        const auto value = parseDword(context, minimumBuild);
        if (value) definition.compatibility.minimumBuild = *value;
    }
    if (const auto maximumBuild = node["max_build"]) {
        const auto value = parseDword(context, maximumBuild);
        if (value) definition.compatibility.maximumBuild = *value;
    }
    if (const auto components = node["required_components"]) {
        if (components.IsSequence()) {
            for (const auto& component : components) {
                definition.compatibility.requiredComponents.append(nodeText(component));
            }
        } else {
            addError(context, u"type.sequence_required"_s, u"required_components должен быть массивом."_s, components);
        }
    }
}

QVector<WindowsDefaultRule> parseWindowsDefaults(ParseContext& context, const YAML::Node& node)
{
    QVector<WindowsDefaultRule> rules;
    if (!node || !node.IsSequence()) {
        addError(context, u"type.sequence_required"_s, u"windows_defaults должен быть массивом."_s, node);
        return rules;
    }

    for (const auto& ruleNode : node) {
        validateKeys(context, ruleNode, {u"os"_s, u"builds"_s, u"state"_s});
        const auto osNode = required(context, ruleNode, "os");
        const auto buildsNode = required(context, ruleNode, "builds");
        const auto state = requiredString(context, ruleNode, "state");
        const auto family = osNode ? parseWindowsFamily(context, osNode) : std::nullopt;
        if (!buildsNode || !buildsNode.IsSequence() || buildsNode.size() != 2) {
            addError(context, u"build_range.invalid"_s, u"builds должен содержать две границы."_s, buildsNode ? buildsNode : ruleNode);
            continue;
        }
        const auto minimum = parseDword(context, buildsNode[0]);
        const auto maximum = parseDword(context, buildsNode[1]);
        if (family && minimum && maximum && !state.isEmpty()) {
            rules.append(WindowsDefaultRule{
                .operatingSystem = *family,
                .builds = BuildRange{.minimum = *minimum, .maximum = *maximum},
                .stateId = state,
            });
        }
    }
    return rules;
}

void parseDetection(ParseContext& context, const YAML::Node& node, TweakDefinition& definition)
{
    validateKeys(context, node, {
        u"type"_s, u"hive"_s, u"key"_s, u"value_name"_s, u"view"_s, u"states"_s,
        u"missing_state"_s, u"present_state"_s, u"feature_id"_s,
        u"folder"_s, u"name"_s, u"enabled_state"_s, u"disabled_state"_s,
        u"object_id"_s, u"element_type"_s, u"value_kind"_s,
        u"scheme"_s, u"subgroup"_s, u"setting"_s, u"source"_s,
        u"component_kind"_s,
    });
    const auto type = requiredString(context, node, "type");
    if (type == u"windows_component") {
        const auto target = parseWindowsComponentTarget(context, node);
        const auto statesNode = required(context, node, "states");
        if (!target || !statesNode || !statesNode.IsMap()) {
            if (statesNode && !statesNode.IsMap()) addError(context, u"type.map_required"_s,
                u"detect.states должен быть объектом."_s, statesNode);
            return;
        }
        domain::WindowsComponentDetection detection{.target = *target};
        for (const auto& entry : statesNode) {
            const auto state = domain::parseWindowsComponentState(nodeText(entry.first));
            const auto stateId = nodeText(entry.second);
            if (!state || *state == domain::WindowsComponentState::Unsupported) {
                addError(context, u"windows_component.state_invalid"_s,
                         u"Состояние определения компонента недопустимо."_s, entry.first);
            } else if (!stateId.isEmpty()) {
                detection.states.insert(*state, stateId);
            }
        }
        definition.windowsComponentDetection = std::move(detection);
        return;
    }
    if (type == u"power.setting") {
        const auto location = parsePowerLocation(context, node);
        const auto statesNode = required(context, node, "states");
        if (!location || !statesNode || !statesNode.IsMap()) {
            if (statesNode && !statesNode.IsMap()) addError(context, u"type.map_required"_s,
                u"detect.states должен быть объектом."_s, statesNode);
            return;
        }
        domain::PowerSettingDetection detection{.location = *location};
        for (const auto& entry : statesNode) {
            const auto index = parseDword(context, entry.first);
            const auto state = nodeText(entry.second);
            if (index && !state.isEmpty()) detection.statesByIndex.insert(*index, state);
        }
        definition.powerDetection = std::move(detection);
        return;
    }
    if (type == u"bcd.element") {
        const auto spec = parseBcdSpec(context, node);
        const auto statesNode = required(context, node, "states");
        const auto missingState = requiredString(context, node, "missing_state");
        if (!spec || !statesNode || !statesNode.IsMap() || missingState.isEmpty()) {
            if (statesNode && !statesNode.IsMap()) {
                addError(context, u"type.map_required"_s,
                         u"detect.states должен быть объектом."_s, statesNode);
            }
            return;
        }
        domain::BcdElementDetection detection{.spec = *spec, .missingState = missingState};
        for (const auto& entry : statesNode) {
            const auto state = nodeText(entry.first);
            if (const auto value = parseBcdValue(context, entry.second, spec->valueKind)) {
                detection.valuesByState.insert(state, *value);
            }
        }
        definition.bcdDetection = std::move(detection);
        return;
    }
    if (type == u"scheduled_task") {
        const auto folder = requiredString(context, node, "folder");
        const auto name = requiredString(context, node, "name");
        const auto enabledState = requiredString(context, node, "enabled_state");
        const auto disabledState = requiredString(context, node, "disabled_state");
        if (folder.contains(u"${"_s) || !folder.startsWith(u'\\')
            || folder.contains(u".."_s) || name.contains(u"${"_s)
            || name.trimmed().isEmpty() || name.contains(u'\\') || name.contains(u'/')) {
            addError(context, u"scheduled_task.location_invalid"_s,
                     u"Папка и имя задачи должны задавать фиксированную задачу планировщика."_s,
                     node);
            return;
        }
        definition.scheduledTaskDetection = domain::ScheduledTaskDetection{
            .location = {.folder = folder, .name = name},
            .enabledState = enabledState,
            .disabledState = disabledState,
        };
        return;
    }
    if (type == u"feature.state") {
        const auto featureIdNode = required(context, node, "feature_id");
        const auto featureId = featureIdNode ? parseDword(context, featureIdNode) : std::nullopt;
        if (featureId && *featureId > 0) {
            definition.featureDetection = domain::FeatureStateDetection{.featureId = *featureId};
        } else if (featureId && *featureId == 0) {
            addError(context, u"feature.id_invalid"_s, u"Feature ID должен быть больше нуля."_s, featureIdNode);
        }
        return;
    }
    if (type == u"registry.value") {
        const auto location = parseRegistryLocation(context, node);
        const auto statesNode = required(context, node, "states");
        const auto missingState = requiredString(context, node, "missing_state");
        if (!location || !statesNode || !statesNode.IsMap() || missingState.isEmpty()) {
            if (statesNode && !statesNode.IsMap()) {
                addError(context, u"type.map_required"_s, u"detect.states должен быть объектом."_s, statesNode);
            }
            return;
        }
        RegistryValueDetection detection{.location = *location, .missingState = missingState};
        for (const auto& entry : statesNode) {
            const auto state = nodeText(entry.first);
            if (const auto value = parseRegistryValue(context, entry.second)) {
                detection.valuesByState.insert(state, *value);
            }
        }
        definition.valueDetection = std::move(detection);
        return;
    }
    if (type == u"registry.tree") {
        const auto location = parseRegistryKeyLocation(context, node);
        const auto presentState = requiredString(context, node, "present_state");
        const auto missingState = requiredString(context, node, "missing_state");
        if (location && !presentState.isEmpty() && !missingState.isEmpty()) {
            definition.treeDetection = domain::RegistryTreeDetection{
                .location = *location,
                .presentState = presentState,
                .missingState = missingState,
            };
        }
        return;
    }
    if (type != u"registry.dword") {
        addError(context, u"detection.unknown"_s, u"Тип обнаружения не поддерживается."_s, node["type"]);
        return;
    }
    const auto location = parseRegistryLocation(context, node);
    const auto statesNode = required(context, node, "states");
    const auto missingState = requiredString(context, node, "missing_state");
    if (!location || !statesNode || !statesNode.IsMap() || missingState.isEmpty()) {
        if (statesNode && !statesNode.IsMap()) {
            addError(context, u"type.map_required"_s, u"detect.states должен быть объектом."_s, statesNode);
        }
        return;
    }

    RegistryDwordDetection detection{.location = *location, .missingState = missingState};
    for (const auto& entry : statesNode) {
        const auto value = parseDword(context, entry.first);
        const auto state = nodeText(entry.second);
        if (value && !state.isEmpty()) {
            detection.statesByValue.insert(*value, state);
        }
    }
    definition.detection = std::move(detection);
}

std::optional<TweakKind> parseKind(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"setting") return TweakKind::Setting;
    if (value == u"action") return TweakKind::Action;
    if (value == u"diagnostic") return TweakKind::Diagnostic;
    addError(context, u"kind.unknown"_s, u"Неизвестный тип элемента."_s, node);
    return std::nullopt;
}

std::optional<Impact> parseImpact(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"low") return Impact::Low;
    if (value == u"medium") return Impact::Medium;
    if (value == u"high") return Impact::High;
    if (value == u"critical") return Impact::Critical;
    addError(context, u"impact.unknown"_s, u"Неизвестный уровень вмешательства."_s, node);
    return std::nullopt;
}

std::optional<Reversibility> parseReversibility(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"reversible") return Reversibility::Reversible;
    if (value == u"conditional") return Reversibility::Conditional;
    if (value == u"irreversible") return Reversibility::Irreversible;
    addError(context, u"reversibility.unknown"_s, u"Неизвестный класс обратимости."_s, node);
    return std::nullopt;
}

std::optional<RestartRequirement> parseRestart(ParseContext& context, const YAML::Node& node)
{
    const auto value = nodeText(node);
    if (value == u"none") return RestartRequirement::None;
    if (value == u"explorer") return RestartRequirement::Explorer;
    if (value == u"service") return RestartRequirement::Service;
    if (value == u"sign_out") return RestartRequirement::SignOut;
    if (value == u"reboot") return RestartRequirement::Reboot;
    addError(context, u"restart.unknown"_s, u"Неизвестное требование перезапуска."_s, node);
    return std::nullopt;
}

QVector<TweakId> parseRelations(
    ParseContext& context, const YAML::Node& root, const char* field)
{
    QVector<TweakId> relations;
    const auto node = required(context, root, field);
    if (!node) return relations;
    if (!node.IsSequence()) {
        addError(context, u"type.sequence_required"_s, u"Связи должны быть массивом."_s, node);
        return relations;
    }
    for (const auto& entry : node) {
        if (!entry.IsScalar()) {
            addError(context, u"type.string_required"_s, u"ID связи должен быть строкой."_s, entry);
            continue;
        }
        const auto parsed = TweakId::parse(nodeText(entry));
        if (!parsed) {
            addError(context, u"relation.id_invalid"_s, u"Некорректный ID связи."_s, entry);
            continue;
        }
        relations.append(*parsed);
    }
    return relations;
}

std::optional<TweakDefinition> parseDefinition(ParseContext& context, const YAML::Node& root)
{
    const auto errorCountBefore = context.errors->size();
    validateKeys(context, root, {
        u"id"_s, u"title"_s, u"category"_s, u"subcategory"_s, u"kind"_s, u"summary"_s,
        u"explanation"_s, u"compatibility"_s, u"states"_s, u"windows_defaults"_s, u"detect"_s,
        u"dependencies"_s, u"conflicts"_s, u"restart"_s, u"impact"_s, u"reversibility"_s,
        u"requires_network"_s,
        u"inputs"_s,
    });

    TweakDefinition definition;
    const auto idNode = required(context, root, "id");
    if (idNode) {
        const auto parsedId = TweakId::parse(nodeText(idNode));
        if (parsedId) definition.id = *parsedId;
        else addError(context, u"id.invalid"_s, u"Некорректный постоянный ID."_s, idNode);
    }
    definition.title = requiredString(context, root, "title");
    definition.category = requiredString(context, root, "category");
    definition.subcategory = requiredString(context, root, "subcategory");
    definition.summary = requiredString(context, root, "summary");

    const auto kindNode = required(context, root, "kind");
    if (kindNode) {
        if (const auto kind = parseKind(context, kindNode)) definition.kind = *kind;
    }

    const auto explanation = required(context, root, "explanation");
    if (explanation) {
        validateKeys(context, explanation, {
            u"purpose"_s, u"mechanism"_s, u"effect"_s, u"tradeoffs"_s, u"recommendation"_s, u"technical_details"_s,
        });
        definition.explanation = {
            .purpose = requiredString(context, explanation, "purpose"),
            .mechanism = requiredString(context, explanation, "mechanism"),
            .effect = requiredString(context, explanation, "effect"),
            .tradeoffs = requiredString(context, explanation, "tradeoffs"),
            .recommendation = requiredString(context, explanation, "recommendation"),
            .technicalDetails = requiredString(context, explanation, "technical_details"),
        };
    }

    const auto compatibility = required(context, root, "compatibility");
    if (compatibility) parseCompatibility(context, compatibility, definition);
    definition.inputs = parseInputs(context, root["inputs"]);
    definition.states = parseStates(context, required(context, root, "states"));
    definition.windowsDefaults = parseWindowsDefaults(context, required(context, root, "windows_defaults"));
    parseDetection(context, required(context, root, "detect"), definition);
    definition.dependencies = parseRelations(context, root, "dependencies");
    definition.conflicts = parseRelations(context, root, "conflicts");

    const auto restartNode = required(context, root, "restart");
    if (restartNode) {
        if (const auto restart = parseRestart(context, restartNode)) definition.restart = *restart;
    }
    const auto impactNode = required(context, root, "impact");
    if (impactNode) {
        if (const auto impact = parseImpact(context, impactNode)) definition.impact = *impact;
    }
    const auto reversibilityNode = required(context, root, "reversibility");
    if (reversibilityNode) {
        if (const auto reversibility = parseReversibility(context, reversibilityNode)) definition.reversibility = *reversibility;
    }
    const auto requiresNetwork = required(context, root, "requires_network");
    if (requiresNetwork) {
        try {
            definition.requiresNetwork = requiresNetwork.as<bool>();
        } catch (const YAML::Exception&) {
            addError(context, u"type.bool_required"_s, u"requires_network должен быть true либо false."_s, requiresNetwork);
        }
    }

    for (const auto& rule : definition.windowsDefaults) {
        if (!definition.supportsTargetState(rule.stateId)) {
            addError(context, u"state.unknown"_s, u"Штатное значение ссылается на неизвестное состояние."_s, root["windows_defaults"]);
        }
    }
    if (definition.detection) {
        if (!definition.supportsTargetState(definition.detection->missingState)) {
            addError(context, u"state.unknown"_s, u"missing_state ссылается на неизвестное состояние."_s, root["detect"]);
        }
        for (const auto& state : definition.detection->statesByValue) {
            if (!definition.supportsTargetState(state)) {
                addError(context, u"state.unknown"_s, u"detect.states ссылается на неизвестное состояние."_s, root["detect"]["states"]);
            }
        }
    }
    if (definition.valueDetection) {
        if (!definition.supportsTargetState(definition.valueDetection->missingState)) {
            addError(context, u"state.unknown"_s, u"missing_state ссылается на неизвестное состояние."_s, root["detect"]);
        }
        for (auto iterator = definition.valueDetection->valuesByState.cbegin();
             iterator != definition.valueDetection->valuesByState.cend(); ++iterator) {
            if (!definition.supportsTargetState(iterator.key())) {
                addError(context, u"state.unknown"_s, u"detect.states ссылается на неизвестное состояние."_s, root["detect"]["states"]);
            }
        }
    }
    if (definition.scheduledTaskDetection) {
        if (!definition.supportsTargetState(definition.scheduledTaskDetection->enabledState)
            || !definition.supportsTargetState(definition.scheduledTaskDetection->disabledState)) {
            addError(context, u"state.unknown"_s,
                     u"Состояние задачи планировщика ссылается на неизвестное состояние."_s,
                     root["detect"]);
        }
    }
    if (definition.bcdDetection) {
        if (!definition.supportsTargetState(definition.bcdDetection->missingState)) {
            addError(context, u"state.unknown"_s,
                     u"missing_state BCD ссылается на неизвестное состояние."_s,
                     root["detect"]);
        }
        for (auto iterator = definition.bcdDetection->valuesByState.cbegin();
             iterator != definition.bcdDetection->valuesByState.cend(); ++iterator) {
            if (!definition.supportsTargetState(iterator.key())) {
                addError(context, u"state.unknown"_s,
                         u"detect.states BCD ссылается на неизвестное состояние."_s,
                         root["detect"]["states"]);
            }
        }
    }
    if (definition.powerDetection) {
        for (const auto& state : definition.powerDetection->statesByIndex) {
            if (!definition.supportsTargetState(state)) addError(context, u"state.unknown"_s,
                u"detect.states питания ссылается на неизвестное состояние."_s,
                root["detect"]["states"]);
        }
    }
    if (definition.windowsComponentDetection) {
        for (const auto& state : definition.windowsComponentDetection->states) {
            if (!definition.supportsTargetState(state)) addError(context, u"state.unknown"_s,
                u"detect.states компонента Windows ссылается на неизвестное состояние."_s,
                root["detect"]["states"]);
        }
    }

    for (const auto& error : definition.validationErrors()) {
        if (error == u"windows_default.unknown_state") continue;
        addError(context, u"definition.invalid"_s, u"Нарушен инвариант определения: "_s + error, root);
    }

    if (context.errors->size() != errorCountBefore) {
        return std::nullopt;
    }
    return definition;
}

} // namespace

CatalogLoadResult TweakCatalogLoader::loadDirectory(const QString& directory) const
{
    CatalogLoadResult result;
    QVector<QString> filePaths;
    QDirIterator iterator(directory, {u"*.yaml"_s, u"*.yml"_s}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        filePaths.append(QFileInfo(iterator.next()).canonicalFilePath());
    }
    std::sort(filePaths.begin(), filePaths.end());

    if (filePaths.isEmpty()) {
        result.errors.append(CatalogError{
            .code = u"catalog.empty"_s,
            .message = u"В каталоге нет YAML-определений."_s,
            .filePath = QDir(directory).absolutePath(),
            .line = 1,
            .column = 1,
        });
        return result;
    }

    QVector<TweakDefinition> definitions;
    QHash<TweakId, QString> sourceById;
    for (const auto& filePath : filePaths) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            result.errors.append(CatalogError{
                .code = u"file.read_failed"_s,
                .message = file.errorString(),
                .filePath = filePath,
                .line = 1,
                .column = 1,
            });
            continue;
        }

        ParseContext context{.filePath = filePath, .errors = &result.errors};
        try {
            const auto root = YAML::Load(file.readAll().toStdString());
            const auto definition = parseDefinition(context, root);
            if (!definition) {
                continue;
            }
            if (sourceById.contains(definition->id)) {
                addError(context, u"id.duplicate"_s, u"ID уже объявлен в другом файле: "_s + sourceById.value(definition->id), root["id"]);
                continue;
            }
            sourceById.insert(definition->id, filePath);
            definitions.append(*definition);
        } catch (const YAML::Exception& exception) {
            const auto mark = exception.mark;
            result.errors.append(CatalogError{
                .code = u"yaml.invalid"_s,
                .message = QString::fromUtf8(exception.what()),
                .filePath = filePath,
                .line = mark.is_null() ? 1 : mark.line + 1,
                .column = mark.is_null() ? 1 : mark.column + 1,
            });
        }
    }

    for (const auto& definition : definitions) {
        const auto verify = [&](const QVector<TweakId>& relations) {
            for (const auto& relation : relations) {
                if (sourceById.contains(relation)) continue;
                result.errors.append(CatalogError{
                    .code = u"relation.unknown"_s,
                    .message = u"Связь ссылается на отсутствующий ID: "_s + relation.toString(),
                    .filePath = sourceById.value(definition.id),
                    .line = 1,
                    .column = 1,
                });
            }
        };
        verify(definition.dependencies);
        verify(definition.conflicts);
    }

    if (result.errors.isEmpty()) {
        result.catalog.emplace(std::move(definitions));
    }
    return result;
}

} // namespace tweakopedia::content
