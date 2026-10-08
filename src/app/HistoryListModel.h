#pragma once

#include "persistence/TransactionRecord.h"

#include <QAbstractListModel>

namespace tweakopedia::app {

class HistoryListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role { TransactionIdRole = Qt::UserRole + 1, PackageNameRole, StatusRole,
                UpdatedAtRole, ErrorRole, CanRollbackRole };
    Q_ENUM(Role)

    explicit HistoryListModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void reset(QVector<persistence::TransactionRecord> records);

private:
    QVector<persistence::TransactionRecord> records_;
};

} // namespace tweakopedia::app
