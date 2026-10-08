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
using domain::RegistryLocation;
using domain::RegistryView;
using domain::RestartRequirement;
using domain::Reversibility;
using domain::SetRegistryDwordOperation;
using domain::TweakDefinition;
using domain::TweakId;
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

    return RegistryLocation{
        .hive = *hive,
        .key = normalizedKey,
        .valueName = valueName,
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

std::optional<OperationSpec> parseOperation(ParseContext& context, const YAML::Node& node)
{
    validateKeys(context, node, {
        u"type"_s, u"hive"_s, u"key"_s, u"value_name"_s, u"view"_s, u"value"_s,
    });
    const auto typeNode = required(context, node, "type");
    if (!typeNode || nodeText(typeNode) != u"registry.set_dword") {
        addError(context, u"operation.unknown"_s, u"Разрешена только типизированная операция registry.set_dword."_s, typeNode ? typeNode : node);
        return std::nullopt;
    }

    const auto location = parseRegistryLocation(context, node);
    const auto valueNode = required(context, node, "value");
    const auto value = valueNode ? parseDword(context, valueNode) : std::nullopt;
    if (!location || !value) {
        return std::nullopt;
    }
    return SetRegistryDwordOperation{.location = *location, .value = *value};
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

std::optional<RegistryDwordDetection> parseDetection(ParseContext& context, const YAML::Node& node)
{
    validateKeys(context, node, {
        u"type"_s, u"hive"_s, u"key"_s, u"value_name"_s, u"view"_s, u"states"_s, u"missing_state"_s,
    });
    const auto type = requiredString(context, node, "type");
    if (type != u"registry.dword") {
        addError(context, u"detection.unknown"_s, u"Разрешено только обнаружение registry.dword."_s, node["type"]);
        return std::nullopt;
    }
    const auto location = parseRegistryLocation(context, node);
    const auto statesNode = required(context, node, "states");
    const auto missingState = requiredString(context, node, "missing_state");
    if (!location || !statesNode || !statesNode.IsMap() || missingState.isEmpty()) {
        if (statesNode && !statesNode.IsMap()) {
            addError(context, u"type.map_required"_s, u"detect.states должен быть объектом."_s, statesNode);
        }
        return std::nullopt;
    }

    RegistryDwordDetection detection{.location = *location, .missingState = missingState};
    for (const auto& entry : statesNode) {
        const auto value = parseDword(context, entry.first);
        const auto state = nodeText(entry.second);
        if (value && !state.isEmpty()) {
            detection.statesByValue.insert(*value, state);
        }
    }
    return detection;
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

std::optional<TweakDefinition> parseDefinition(ParseContext& context, const YAML::Node& root)
{
    const auto errorCountBefore = context.errors->size();
    validateKeys(context, root, {
        u"id"_s, u"title"_s, u"category"_s, u"subcategory"_s, u"kind"_s, u"summary"_s,
        u"explanation"_s, u"compatibility"_s, u"states"_s, u"windows_defaults"_s, u"detect"_s,
        u"dependencies"_s, u"conflicts"_s, u"restart"_s, u"impact"_s, u"reversibility"_s,
        u"requires_network"_s,
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
    definition.states = parseStates(context, required(context, root, "states"));
    definition.windowsDefaults = parseWindowsDefaults(context, required(context, root, "windows_defaults"));
    definition.detection = parseDetection(context, required(context, root, "detect"));

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

    for (const auto* relation : {"dependencies", "conflicts"}) {
        const auto relationNode = required(context, root, relation);
        if (relationNode && !relationNode.IsSequence()) {
            addError(context, u"type.sequence_required"_s, u"Связи должны быть массивом."_s, relationNode);
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

    if (result.errors.isEmpty()) {
        result.catalog.emplace(std::move(definitions));
    }
    return result;
}

} // namespace tweakopedia::content
