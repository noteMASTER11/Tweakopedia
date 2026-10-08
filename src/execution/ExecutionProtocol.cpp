#include "execution/ExecutionProtocol.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

using domain::CpuArchitecture;
using domain::RegistryHive;
using domain::RegistryLocation;
using domain::RegistryView;
using domain::RestartRequirement;
using domain::RemoveAppxPackageOperation;
using domain::SetRegistryDwordOperation;
using domain::SystemProfile;
using domain::TweakId;
using domain::WindowsFamily;
using planning::ExecutionPlan;
using planning::PlannedAppxRemoval;
using planning::PlannedOperation;
using planning::PlannedRegistryDwordChange;

QString familyName(WindowsFamily family)
{
    switch (family) {
    case WindowsFamily::Windows10: return u"windows10"_s;
    case WindowsFamily::Windows11: return u"windows11"_s;
    case WindowsFamily::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

QString architectureName(CpuArchitecture architecture)
{
    switch (architecture) {
    case CpuArchitecture::X64: return u"x64"_s;
    case CpuArchitecture::Arm64: return u"arm64"_s;
    case CpuArchitecture::X86: return u"x86"_s;
    case CpuArchitecture::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

QString restartName(RestartRequirement restart)
{
    switch (restart) {
    case RestartRequirement::None: return u"none"_s;
    case RestartRequirement::Explorer: return u"explorer"_s;
    case RestartRequirement::Service: return u"service"_s;
    case RestartRequirement::SignOut: return u"sign_out"_s;
    case RestartRequirement::Reboot: return u"reboot"_s;
    }
    return u"none"_s;
}

QString hiveName(RegistryHive hive)
{
    return hive == RegistryHive::LocalMachine ? u"HKLM"_s : u"HKCU"_s;
}

QString viewName(RegistryView view)
{
    switch (view) {
    case RegistryView::Default: return u"default"_s;
    case RegistryView::Registry32: return u"registry32"_s;
    case RegistryView::Registry64: return u"registry64"_s;
    }
    return u"default"_s;
}

QJsonObject profileObject(const SystemProfile& profile)
{
    return {
        {u"architecture"_s, architectureName(profile.architecture)},
        {u"build"_s, static_cast<qint64>(profile.build)},
        {u"edition"_s, profile.edition},
        {u"family"_s, familyName(profile.family)},
        {u"ubr"_s, static_cast<qint64>(profile.ubr)},
    };
}

QJsonObject registryObject(const RegistryLocation& location, quint32 value)
{
    return {
        {u"hive"_s, hiveName(location.hive)},
        {u"key"_s, location.key},
        {u"value"_s, static_cast<qint64>(value)},
        {u"value_name"_s, location.valueName},
        {u"view"_s, viewName(location.view)},
    };
}

QJsonObject operationObject(const PlannedRegistryDwordChange& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"registry"_s, registryObject(operation.change.location, operation.change.value)},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"registry.set_dword"_s},
    };
}

QJsonObject operationObject(const PlannedAppxRemoval& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"package_name"_s, operation.change.packageName},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"appx.remove"_s},
    };
}

void addError(ProtocolDecodeResult& result, QString code, QString message)
{
    result.errors.append({.code = std::move(code), .message = std::move(message)});
}

bool validateKeys(
    ProtocolDecodeResult& result,
    const QJsonObject& object,
    const QSet<QString>& allowed)
{
    bool valid = true;
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(result, u"field.unknown"_s, u"Неизвестное поле JSON: "_s + iterator.key());
            valid = false;
        }
    }
    return valid;
}

std::optional<WindowsFamily> parseFamily(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"windows10") return WindowsFamily::Windows10;
    if (value == u"windows11") return WindowsFamily::Windows11;
    addError(result, u"profile.family_unknown"_s, u"Неизвестное семейство Windows."_s);
    return std::nullopt;
}

std::optional<CpuArchitecture> parseArchitecture(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"x64") return CpuArchitecture::X64;
    addError(result, u"profile.architecture_unknown"_s, u"Executor принимает только x64."_s);
    return std::nullopt;
}

std::optional<RestartRequirement> parseRestart(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"none") return RestartRequirement::None;
    if (value == u"explorer") return RestartRequirement::Explorer;
    if (value == u"service") return RestartRequirement::Service;
    if (value == u"sign_out") return RestartRequirement::SignOut;
    if (value == u"reboot") return RestartRequirement::Reboot;
    addError(result, u"restart.unknown"_s, u"Неизвестное требование перезапуска."_s);
    return std::nullopt;
}

std::optional<RegistryHive> parseHive(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"HKLM") return RegistryHive::LocalMachine;
    if (value == u"HKCU") return RegistryHive::CurrentUser;
    addError(result, u"registry.hive_unknown"_s, u"Hive реестра не входит в whitelist."_s);
    return std::nullopt;
}

std::optional<RegistryView> parseView(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"default") return RegistryView::Default;
    if (value == u"registry32") return RegistryView::Registry32;
    if (value == u"registry64") return RegistryView::Registry64;
    addError(result, u"registry.view_unknown"_s, u"Неизвестное представление реестра."_s);
    return std::nullopt;
}

std::optional<SystemProfile> parseProfile(ProtocolDecodeResult& result, const QJsonValue& value)
{
    if (!value.isObject()) {
        addError(result, u"profile.invalid"_s, u"Профиль ОС должен быть объектом."_s);
        return std::nullopt;
    }
    const auto object = value.toObject();
    validateKeys(result, object, {u"architecture"_s, u"build"_s, u"edition"_s, u"family"_s, u"ubr"_s});
    const auto family = parseFamily(result, object.value(u"family"_s).toString());
    const auto architecture = parseArchitecture(result, object.value(u"architecture"_s).toString());
    if (!family || !architecture || !object.value(u"build"_s).isDouble() || !object.value(u"ubr"_s).isDouble()) {
        if (!object.value(u"build"_s).isDouble() || !object.value(u"ubr"_s).isDouble()) {
            addError(result, u"profile.invalid"_s, u"Build и UBR должны быть числами."_s);
        }
        return std::nullopt;
    }
    return SystemProfile{
        .family = *family,
        .build = static_cast<quint32>(object.value(u"build"_s).toInteger()),
        .ubr = static_cast<quint32>(object.value(u"ubr"_s).toInteger()),
        .edition = object.value(u"edition"_s).toString(),
        .architecture = *architecture,
    };
}

std::optional<PlannedOperation> parseOperation(ProtocolDecodeResult& result, const QJsonValue& value)
{
    if (!value.isObject()) {
        addError(result, u"operation.invalid"_s, u"Операция должна быть объектом."_s);
        return std::nullopt;
    }
    const auto object = value.toObject();
    const auto type = object.value(u"type"_s).toString();
    if (type != u"registry.set_dword" && type != u"appx.remove") {
        addError(result, u"operation.unknown"_s, u"Тип операции не входит в whitelist."_s);
        return std::nullopt;
    }
    const auto id = TweakId::parse(object.value(u"tweak_id"_s).toString());
    const auto restart = parseRestart(result, object.value(u"restart"_s).toString());
    const auto targetState = object.value(u"target_state"_s).toString();
    if (!id || !restart || targetState.isEmpty()) {
        if (!id) addError(result, u"id.invalid"_s, u"Некорректный ID твика."_s);
        if (targetState.isEmpty()) {
            addError(result, u"operation.invalid"_s, u"Целевое состояние отсутствует."_s);
        }
        return std::nullopt;
    }

    if (type == u"appx.remove") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"package_name"_s, u"restart"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s,
        });
        const auto packageName = object.value(u"package_name"_s).toString();
        static const QRegularExpression packageNamePattern(u"^[A-Za-z0-9][A-Za-z0-9.-]{0,199}$"_s);
        if (!packageNamePattern.match(packageName).hasMatch()) {
            addError(result, u"appx.package_name_invalid"_s,
                     u"Имя AppX-пакета содержит недопустимые символы."_s);
            return std::nullopt;
        }
        return PlannedAppxRemoval{
            .tweakId = *id,
            .targetState = targetState,
            .change = RemoveAppxPackageOperation{.packageName = packageName},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
        };
    }

    validateKeys(result, object, {
        u"before_fingerprint"_s, u"registry"_s, u"restart"_s,
        u"target_state"_s, u"tweak_id"_s, u"type"_s,
    });
    const auto registryValue = object.value(u"registry"_s);
    if (!registryValue.isObject()) {
        addError(result, u"registry.invalid"_s, u"Параметры реестра должны быть объектом."_s);
        return std::nullopt;
    }
    const auto registry = registryValue.toObject();
    validateKeys(result, registry, {u"hive"_s, u"key"_s, u"value"_s, u"value_name"_s, u"view"_s});
    const auto hive = parseHive(result, registry.value(u"hive"_s).toString());
    const auto view = parseView(result, registry.value(u"view"_s).toString());
    const auto rawValue = registry.value(u"value"_s).toInteger(-1);
    if (!hive || !view || rawValue < 0 || rawValue > std::numeric_limits<quint32>::max()) {
        if (rawValue < 0 || rawValue > std::numeric_limits<quint32>::max()) {
            addError(result, u"value.invalid_dword"_s, u"DWORD выходит за допустимый диапазон."_s);
        }
        return std::nullopt;
    }
    const auto key = registry.value(u"key"_s).toString();
    const auto valueName = registry.value(u"value_name"_s).toString();
    if (key.isEmpty() || valueName.isEmpty() || targetState.isEmpty()) {
        addError(result, u"operation.invalid"_s, u"Операция содержит пустое обязательное поле."_s);
        return std::nullopt;
    }
    return PlannedRegistryDwordChange{
        .tweakId = *id,
        .targetState = targetState,
        .change = SetRegistryDwordOperation{
            .location = {.hive = *hive, .key = key, .valueName = valueName, .view = *view},
            .value = static_cast<quint32>(rawValue),
        },
        .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
        .restart = *restart,
    };
}

} // namespace

QJsonObject ExecutionProtocol::bodyObject(const planning::ExecutionPlan& plan)
{
    QJsonArray operations;
    for (const auto& operation : plan.operations) {
        operations.append(std::visit([](const auto& value) { return operationObject(value); }, operation));
    }
    return {
        {u"created_at_utc"_s, plan.createdAtUtc.toUTC().toString(Qt::ISODateWithMs)},
        {u"operations"_s, operations},
        {u"profile"_s, profileObject(plan.profile)},
        {u"restart"_s, restartName(plan.restart)},
        {u"schema_version"_s, plan.schemaVersion},
        {u"summary"_s, plan.summary},
        {u"transaction_id"_s, plan.transactionId.toString(QUuid::WithoutBraces)},
    };
}

QByteArray ExecutionProtocol::canonicalHash(const QJsonObject& body)
{
    return QCryptographicHash::hash(
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

QByteArray ExecutionProtocol::bodyHash(const planning::ExecutionPlan& plan)
{
    return canonicalHash(bodyObject(plan));
}

QByteArray ExecutionProtocol::encode(const planning::ExecutionPlan& plan)
{
    const auto body = bodyObject(plan);
    const QJsonObject envelope{
        {u"body"_s, body},
        {u"sha256"_s, QString::fromLatin1(canonicalHash(body))},
    };
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

ProtocolDecodeResult ExecutionProtocol::decode(const QByteArray& json)
{
    ProtocolDecodeResult result;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        addError(result, u"json.invalid"_s, u"Некорректный JSON плана."_s);
        return result;
    }
    const auto envelope = document.object();
    validateKeys(result, envelope, {u"body"_s, u"sha256"_s});
    if (!envelope.value(u"body"_s).isObject()) {
        addError(result, u"body.invalid"_s, u"Тело плана отсутствует."_s);
        return result;
    }
    const auto body = envelope.value(u"body"_s).toObject();
    result.bodyHash = canonicalHash(body);
    if (envelope.value(u"sha256"_s).toString().toLatin1() != result.bodyHash) {
        addError(result, u"hash.mismatch"_s, u"SHA-256 тела плана не совпадает."_s);
        return result;
    }
    validateKeys(result, body, {
        u"created_at_utc"_s, u"operations"_s, u"profile"_s, u"restart"_s,
        u"schema_version"_s, u"summary"_s, u"transaction_id"_s,
    });
    const auto schemaVersion = body.value(u"schema_version"_s).toInt(-1);
    if (schemaVersion != planning::executionPlanSchemaVersion) {
        addError(result, u"schema.unsupported"_s, u"Версия протокола плана не поддерживается."_s);
    }
    const auto profile = parseProfile(result, body.value(u"profile"_s));
    const auto restart = parseRestart(result, body.value(u"restart"_s).toString());
    const auto transactionId = QUuid(body.value(u"transaction_id"_s).toString());
    const auto createdAt = QDateTime::fromString(body.value(u"created_at_utc"_s).toString(), Qt::ISODateWithMs);
    if (transactionId.isNull()) addError(result, u"transaction.invalid_id"_s, u"Некорректный UUID транзакции."_s);
    if (!createdAt.isValid()) addError(result, u"plan.invalid_time"_s, u"Некорректное время создания плана."_s);

    QVector<PlannedOperation> operations;
    const auto operationsValue = body.value(u"operations"_s);
    if (!operationsValue.isArray()) {
        addError(result, u"operations.invalid"_s, u"operations должен быть массивом."_s);
    } else {
        for (const auto& operationValue : operationsValue.toArray()) {
            if (const auto operation = parseOperation(result, operationValue)) {
                operations.append(*operation);
            }
        }
    }
    if (!result.errors.isEmpty() || !profile || !restart) {
        return result;
    }
    result.plan = ExecutionPlan{
        .schemaVersion = schemaVersion,
        .transactionId = transactionId,
        .profile = *profile,
        .createdAtUtc = createdAt.toUTC(),
        .operations = std::move(operations),
        .summary = body.value(u"summary"_s).toString(),
        .restart = *restart,
    };
    return result;
}

} // namespace tweakopedia::execution
