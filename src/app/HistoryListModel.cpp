#include "app/HistoryListModel.h"

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
    case CanRollbackRole: return record.status == persistence::TransactionStatus::Succeeded;
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
    beginResetModel();
    records_ = std::move(records);
    endResetModel();
}

} // namespace tweakopedia::app
