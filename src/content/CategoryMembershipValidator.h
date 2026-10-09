#pragma once

#include "content/CatalogError.h"
#include "content/CategoryCatalog.h"
#include "content/TweakCatalog.h"

#include <QVector>

namespace tweakopedia::content {

class CategoryMembershipValidator final
{
public:
    [[nodiscard]] QVector<CatalogError> validate(
        const CategoryCatalog& categories,
        const TweakCatalog& tweaks) const;
};

} // namespace tweakopedia::content
