#pragma once

#include "content/CatalogError.h"
#include "content/CategoryCatalog.h"

#include <QVector>

#include <optional>

namespace tweakopedia::content {

struct CategoryCatalogLoadResult {
    std::optional<CategoryCatalog> catalog;
    QVector<CatalogError> errors;
};

class CategoryCatalogLoader final
{
public:
    [[nodiscard]] CategoryCatalogLoadResult loadFile(const QString& filePath) const;
};

} // namespace tweakopedia::content
