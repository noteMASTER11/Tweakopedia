#include "content/CategoryCatalog.h"

namespace tweakopedia::content {

CategoryCatalog::CategoryCatalog(QVector<CategoryDefinition> categories)
    : categories_(std::move(categories))
{
}

qsizetype CategoryCatalog::size() const noexcept
{
    return categories_.size();
}

bool CategoryCatalog::isEmpty() const noexcept
{
    return categories_.isEmpty();
}

const QVector<CategoryDefinition>& CategoryCatalog::categories() const noexcept
{
    return categories_;
}

const CategoryDefinition* CategoryCatalog::find(QStringView id) const noexcept
{
    for (const auto& category : categories_) {
        if (category.id == id) return &category;
    }
    return nullptr;
}

} // namespace tweakopedia::content
