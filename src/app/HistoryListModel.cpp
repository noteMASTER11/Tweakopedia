#include "app/HistoryListModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

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

} // namespace

namespace tweakopedia::app {

HistoryListModel::HistoryListModel(QObject* parent) : QAbstractListModel(parent) {}

int HistoryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(records_.size());
}

QVariant HistoryListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= records_.size()) return {};
    const auto& record = records_.at(index.row());
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
    default: return {};
    }
}

QHash<int, QByteArray> HistoryListModel::roleNames() const
{
    return {{TransactionIdRole, "transactionId"}, {PackageNameRole, "packageName"},
            {StatusRole, "status"}, {UpdatedAtRole, "updatedAt"}, {ErrorRole, "error"},
            {CanRollbackRole, "canRollback"}};
}

void HistoryListModel::reset(QVector<persistence::TransactionRecord> records)
{
    const auto previousInterrupted = interruptedCount_;
    beginResetModel();
    records_ = std::move(records);
    interruptedCount_ = 0;
    for (const auto& record : records_) {
        if (record.status == persistence::TransactionStatus::Interrupted) ++interruptedCount_;
    }
    endResetModel();
    if (previousInterrupted != interruptedCount_) emit interruptedCountChanged();
}

int HistoryListModel::interruptedCount() const noexcept { return interruptedCount_; }

} // namespace tweakopedia::app
