#include "content/CategoryMembershipValidator.h"

#include <QHash>
#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::content {
namespace {

CatalogError error(QString code, QString message, QString filePath)
{
    return {
        .code = std::move(code),
        .message = std::move(message),
        .filePath = std::move(filePath),
        .line = 1,
        .column = 1,
    };
}

bool hasSubcategory(const CategoryDefinition& category, QStringView id)
{
    return std::any_of(
        category.subcategories.cbegin(), category.subcategories.cend(),
        [id](const auto& subcategory) { return subcategory.id == id; });
}

} // namespace

QVector<CatalogError> CategoryMembershipValidator::validate(
    const CategoryCatalog& categories,
    const TweakCatalog& tweaks) const
{
    QVector<CatalogError> errors;
    QHash<QString, QSet<QString>> usedSubcategories;

    for (const auto& tweak : tweaks.tweaks()) {
        const auto* category = categories.find(tweak.category);
        const auto source = u"tweak:"_s + tweak.id.toString();
        if (!category) {
            errors.append(error(
                u"category.reference_unknown"_s,
                u"Твик %1 ссылается на неизвестную категорию %2."_s
                    .arg(tweak.id.toString(), tweak.category),
                source));
            continue;
        }
        if (tweak.subcategory.isEmpty() || !hasSubcategory(*category, tweak.subcategory)) {
            errors.append(error(
                u"subcategory.reference_unknown"_s,
                u"Твик %1 ссылается на неизвестную подкатегорию %2/%3."_s
                    .arg(tweak.id.toString(), tweak.category, tweak.subcategory),
                source));
            continue;
        }
        usedSubcategories[tweak.category].insert(tweak.subcategory);
    }

    for (const auto& category : categories.categories()) {
        if (category.id == u"app-removal"_s) continue;
        const auto used = usedSubcategories.value(category.id);
        for (const auto& subcategory : category.subcategories) {
            if (used.contains(subcategory.id)) continue;
            errors.append(error(
                u"subcategory.empty"_s,
                u"Подкатегория %1/%2 не содержит твиков."_s
                    .arg(category.id, subcategory.id),
                u"categories:"_s + category.id));
        }
    }

    return errors;
}

} // namespace tweakopedia::content
