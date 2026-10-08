#pragma once

#include "content/CatalogError.h"
#include "content/TweakCatalog.h"

#include <QVector>

#include <optional>

namespace tweakopedia::content {

struct CatalogLoadResult {
    std::optional<TweakCatalog> catalog;
    QVector<CatalogError> errors;
};

class TweakCatalogLoader final
{
public:
    [[nodiscard]] CatalogLoadResult loadDirectory(const QString& directory) const;
};

} // namespace tweakopedia::content
