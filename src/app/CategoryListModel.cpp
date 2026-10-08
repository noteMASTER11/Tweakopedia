#include "app/CategoryListModel.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::app {

CategoryListModel::CategoryListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CategoryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(categories_.size());
}

QVariant CategoryListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= categories_.size()) return {};
    const auto& category = categories_.at(index.row());
    switch (role) {
    case IdRole: return category.id;
    case TitleRole: return category.title;
    case SubcategoriesRole: {
        QVariantList result;
        result.reserve(category.subcategories.size());
        for (const auto& subcategory : category.subcategories) {
            result.append(QVariantMap{
                {u"id"_s, subcategory.id},
                {u"title"_s, subcategory.title},
            });
        }
        return result;
    }
    default: return {};
    }
}

QHash<int, QByteArray> CategoryListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {SubcategoriesRole, "subcategories"},
    };
}

void CategoryListModel::reset(const content::CategoryCatalog& catalog)
{
    beginResetModel();
    categories_.clear();
    categories_.reserve(catalog.size() + 1);
    categories_.append({.id = {}, .title = u"Все категории"_s, .subcategories = {}});
    categories_ += catalog.categories();
    endResetModel();
}

} // namespace tweakopedia::app
