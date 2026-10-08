#pragma once

#include "content/TweakCatalog.h"
#include "persistence/TransactionRecord.h"

#include <QAbstractListModel>
#include <QVariantList>

namespace tweakopedia::app {

class HistoryListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int interruptedCount READ interruptedCount NOTIFY interruptedCountChanged)

public:
    enum Role { TransactionIdRole = Qt::UserRole + 1, PackageNameRole, StatusRole,
                UpdatedAtRole, ErrorRole, CanRollbackRole, OperationsRole,
                OperationCountRole, DetailsAvailableRole };
    Q_ENUM(Role)

    explicit HistoryListModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void reset(QVector<persistence::TransactionRecord> records);
    void reset(
        QVector<persistence::TransactionRecord> records,
        const content::TweakCatalog& catalog);
    [[nodiscard]] int interruptedCount() const noexcept;

signals:
    void interruptedCountChanged();

private:
    struct Entry {
        persistence::TransactionRecord record;
        QVariantList operations;
        bool detailsAvailable{};
    };

    QVector<Entry> entries_;
    int interruptedCount_{};
};

} // namespace tweakopedia::app
