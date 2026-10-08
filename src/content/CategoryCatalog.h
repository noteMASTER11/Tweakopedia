#pragma once

#include <QString>
#include <QVector>

namespace tweakopedia::content {

struct SubcategoryDefinition {
    QString id;
    QString title;
};

struct CategoryDefinition {
    QString id;
    QString title;
    QVector<SubcategoryDefinition> subcategories;
};

class CategoryCatalog final
{
public:
    CategoryCatalog() = default;
    explicit CategoryCatalog(QVector<CategoryDefinition> categories);

    [[nodiscard]] qsizetype size() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] const QVector<CategoryDefinition>& categories() const noexcept;
    [[nodiscard]] const CategoryDefinition* find(QStringView id) const noexcept;

private:
    QVector<CategoryDefinition> categories_;
};

} // namespace tweakopedia::content
