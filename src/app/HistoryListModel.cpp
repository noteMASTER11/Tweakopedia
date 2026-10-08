#include "app/HistoryListModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <optional>

namespace {

bool hasCompleteSnapshot(const QString& directory)
{
    QFile file(QDir(directory).filePath(QStringLiteral("before.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto operations = document.object().value(QStringLiteral("operations"));
    if (!operations.isArray() || operations.toArray().isEmpty()) return false;
    for (const auto& operation : operations.toArray()) {
        if (operation.toObject().value(QStringLiteral("type")).toString()
            == QStringLiteral("appx.packages")) {
            return false;
        }
    }
    return true;
}

std::optional<QJsonObject> readObject(const QString& directory, const QString& fileName)
{
    QFile file(QDir(directory).filePath(fileName));
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

QString restartTitle(const QString& value)
{
    if (value == QStringLiteral("explorer")) return QStringLiteral("Перезапуск Проводника");
    if (value == QStringLiteral("service")) return QStringLiteral("Перезапуск службы");
    if (value == QStringLiteral("sign_out")) return QStringLiteral("Выход из системы");
    if (value == QStringLiteral("reboot")) return QStringLiteral("Перезагрузка ПК");
    return QStringLiteral("Не требуется");
}

QString featureStateTitle(qint64 value)
{
    switch (value) {
    case 0: return QStringLiteral("По умолчанию");
    case 1: return QStringLiteral("Выключено");
    case 2: return QStringLiteral("Включено");
    default: return QStringLiteral("Неизвестно");
    }
}

QString stateTitle(
    const tweakopedia::domain::TweakDefinition* tweak,
    const QString& stateId)
{
    if (tweak) {
        for (const auto& state : tweak->states) {
            if (state.id == stateId) return state.title;
        }
    }
    return stateId.isEmpty() ? QStringLiteral("Не указано") : stateId;
}

std::optional<quint32> dwordValue(const QJsonObject& snapshot)
{
    if (snapshot.value(QStringLiteral("presence")).toInt(-1) != 1
        || snapshot.value(QStringLiteral("type")).toInt(-1) != 1) {
        return std::nullopt;
    }
    const auto bytes = QByteArray::fromBase64(
        snapshot.value(QStringLiteral("rawBase64")).toString().toLatin1());
    if (bytes.size() != 4) return std::nullopt;
    return static_cast<quint32>(static_cast<quint8>(bytes[0]))
        | (static_cast<quint32>(static_cast<quint8>(bytes[1])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(bytes[2])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(bytes[3])) << 24);
}

QString registryBefore(
    const QJsonObject& snapshot,
    const tweakopedia::domain::TweakDefinition* tweak)
{
    const auto presence = snapshot.value(QStringLiteral("presence")).toInt(-1);
    if (presence == 0) {
        const auto state = tweak && tweak->detection
            ? stateTitle(tweak, tweak->detection->missingState) : QString{};
        return state.isEmpty()
            ? QStringLiteral("Значение отсутствовало")
            : state + QStringLiteral(" · значение отсутствовало");
    }
    const auto value = dwordValue(snapshot);
    if (value) {
        QString state;
        if (tweak && tweak->detection) {
            state = stateTitle(tweak, tweak->detection->statesByValue.value(*value));
        }
        const auto technical = QStringLiteral("DWORD %1").arg(*value);
        return state.isEmpty() || state == QStringLiteral("Не указано")
            ? technical : state + QStringLiteral(" · ") + technical;
    }
    if (presence == 1) {
        const auto raw = QByteArray::fromBase64(
            snapshot.value(QStringLiteral("rawBase64")).toString().toLatin1());
        return QStringLiteral("Исходное значение · %1 байт").arg(raw.size());
    }
    return QStringLiteral("Исходное значение не зафиксировано");
}

QString registryObject(const QJsonObject& registry)
{
    return registry.value(QStringLiteral("hive")).toString()
        + QStringLiteral("\\") + registry.value(QStringLiteral("key")).toString()
        + QStringLiteral("\\") + registry.value(QStringLiteral("value_name")).toString();
}

QVariantMap operationDetails(
    const QJsonObject& operation,
    const QJsonObject& snapshot,
    const tweakopedia::content::TweakCatalog& catalog)
{
    const auto idText = operation.value(QStringLiteral("tweak_id")).toString();
    const auto id = tweakopedia::domain::TweakId::parse(idText);
    const auto* tweak = id ? catalog.find(*id) : nullptr;
    const auto targetState = operation.value(QStringLiteral("target_state")).toString();
    QVariantMap result{
        {QStringLiteral("title"), tweak ? tweak->title : idText},
        {QStringLiteral("restart"), restartTitle(
             operation.value(QStringLiteral("restart")).toString())},
    };

    const auto type = operation.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("registry.set_dword")) {
        const auto registry = operation.value(QStringLiteral("registry")).toObject();
        result.insert(QStringLiteral("kind"), QStringLiteral("Реестр"));
        result.insert(QStringLiteral("object"), registryObject(registry));
        result.insert(QStringLiteral("before"), snapshot.isEmpty()
            ? QStringLiteral("Исходное значение не зафиксировано")
            : registryBefore(snapshot, tweak));
        result.insert(QStringLiteral("after"), stateTitle(tweak, targetState)
            + QStringLiteral(" · DWORD %1").arg(
                registry.value(QStringLiteral("value")).toInteger()));
        return result;
    }

    if (type == QStringLiteral("appx.remove")) {
        const auto packages = snapshot.value(QStringLiteral("packages")).toArray();
        result.insert(QStringLiteral("kind"), QStringLiteral("Приложение"));
        result.insert(QStringLiteral("object"),
                      operation.value(QStringLiteral("package_name")).toString());
        result.insert(QStringLiteral("before"), snapshot.isEmpty()
            ? QStringLiteral("Исходное состояние не зафиксировано")
            : packages.isEmpty()
                ? QStringLiteral("Не установлено")
                : QStringLiteral("Установлено · пакетов: %1").arg(packages.size()));
        result.insert(QStringLiteral("after"), stateTitle(tweak, targetState));
        return result;
    }

    if (type == QStringLiteral("feature.set_state")) {
        const auto configuration = snapshot.value(QStringLiteral("configuration")).toObject();
        const auto featureId = operation.value(QStringLiteral("feature_id")).toInteger();
        result.insert(QStringLiteral("kind"), QStringLiteral("Feature Store"));
        result.insert(QStringLiteral("object"), QStringLiteral("Feature ID %1").arg(featureId));
        result.insert(QStringLiteral("before"), snapshot.isEmpty()
            ? QStringLiteral("Исходное состояние не зафиксировано")
            : featureStateTitle(configuration.value(QStringLiteral("state")).toInteger(-1)));
        result.insert(QStringLiteral("after"), stateTitle(tweak, targetState)
            + QStringLiteral(" · состояние %1").arg(
                operation.value(QStringLiteral("state")).toInteger()));
        return result;
    }

    result.insert(QStringLiteral("kind"), QStringLiteral("Операция Windows"));
    result.insert(QStringLiteral("object"), type);
    result.insert(QStringLiteral("before"), QStringLiteral("Исходное состояние не определено"));
    result.insert(QStringLiteral("after"), stateTitle(tweak, targetState));
    return result;
}

QVariantList loadOperations(
    const tweakopedia::persistence::TransactionRecord& record,
    const tweakopedia::content::TweakCatalog& catalog,
    bool& detailsAvailable)
{
    detailsAvailable = false;
    const auto planDocument = readObject(record.directory, QStringLiteral("plan.json"));
    if (!planDocument) return {};
    const auto body = planDocument->value(QStringLiteral("body")).toObject();
    const auto operationsValue = body.value(QStringLiteral("operations"));
    if (!operationsValue.isArray()) return {};
    detailsAvailable = true;

    QJsonArray snapshots;
    if (const auto before = readObject(record.directory, QStringLiteral("before.json"))) {
        snapshots = before->value(QStringLiteral("operations")).toArray();
    }

    QVariantList result;
    const auto operations = operationsValue.toArray();
    result.reserve(operations.size());
    for (qsizetype index = 0; index < operations.size(); ++index) {
        const auto snapshot = index < snapshots.size()
            ? snapshots.at(index).toObject() : QJsonObject{};
        result.append(operationDetails(operations.at(index).toObject(), snapshot, catalog));
    }
    return result;
}

} // namespace

namespace tweakopedia::app {

HistoryListModel::HistoryListModel(QObject* parent) : QAbstractListModel(parent) {}

int HistoryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant HistoryListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};
    const auto& entry = entries_.at(index.row());
    const auto& record = entry.record;
    switch (role) {
    case TransactionIdRole: return record.id.toString(QUuid::WithoutBraces);
    case PackageNameRole: return record.packageName;
    case StatusRole: return persistence::transactionStatusName(record.status);
    case UpdatedAtRole: return record.updatedAtUtc;
    case ErrorRole: return record.error;
    case CanRollbackRole: {
        const auto recoverable = record.status == persistence::TransactionStatus::Succeeded
            || record.status == persistence::TransactionStatus::Failed
            || record.status == persistence::TransactionStatus::Interrupted;
        return recoverable && hasCompleteSnapshot(record.directory);
    }
    case OperationsRole: return entry.operations;
    case OperationCountRole: return entry.operations.size();
    case DetailsAvailableRole: return entry.detailsAvailable;
    default: return {};
    }
}

QHash<int, QByteArray> HistoryListModel::roleNames() const
{
    return {{TransactionIdRole, "transactionId"}, {PackageNameRole, "packageName"},
            {StatusRole, "status"}, {UpdatedAtRole, "updatedAt"}, {ErrorRole, "error"},
            {CanRollbackRole, "canRollback"}, {OperationsRole, "operations"},
            {OperationCountRole, "operationCount"},
            {DetailsAvailableRole, "detailsAvailable"}};
}

void HistoryListModel::reset(QVector<persistence::TransactionRecord> records)
{
    reset(std::move(records), content::TweakCatalog{});
}

void HistoryListModel::reset(
    QVector<persistence::TransactionRecord> records,
    const content::TweakCatalog& catalog)
{
    const auto previousInterrupted = interruptedCount_;
    beginResetModel();
    entries_.clear();
    entries_.reserve(records.size());
    interruptedCount_ = 0;
    for (auto& record : records) {
        if (record.status == persistence::TransactionStatus::Interrupted) ++interruptedCount_;
        bool detailsAvailable{};
        auto operations = loadOperations(record, catalog, detailsAvailable);
        entries_.append(Entry{
            .record = std::move(record),
            .operations = std::move(operations),
            .detailsAvailable = detailsAvailable,
        });
    }
    endResetModel();
    if (previousInterrupted != interruptedCount_) emit interruptedCountChanged();
}

int HistoryListModel::interruptedCount() const noexcept { return interruptedCount_; }

} // namespace tweakopedia::app
