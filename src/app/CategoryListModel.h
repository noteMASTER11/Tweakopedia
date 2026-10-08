#pragma once

#include "content/CategoryCatalog.h"

#include <QAbstractListModel>

namespace tweakopedia::app {

class CategoryListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SubcategoriesRole,
    };
    Q_ENUM(Role)

    explicit CategoryListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(const content::CategoryCatalog& catalog);

private:
    QVector<content::CategoryDefinition> categories_;
};

} // namespace tweakopedia::app
