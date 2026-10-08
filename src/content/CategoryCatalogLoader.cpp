#include "content/CategoryCatalogLoader.h"

#include <QFile>
#include <QSet>

#include <yaml-cpp/yaml.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::content {
namespace {

struct ParseContext {
    QString filePath;
    QVector<CatalogError>* errors{};
};

QString nodeText(const YAML::Node& node)
{
    return node && node.IsScalar() ? QString::fromStdString(node.Scalar()) : QString{};
}

void addError(ParseContext& context, QString code, QString message, const YAML::Node& node)
{
    const auto mark = node.Mark();
    context.errors->append({
        .code = std::move(code),
        .message = std::move(message),
        .filePath = context.filePath,
        .line = mark.is_null() ? 1 : mark.line + 1,
        .column = mark.is_null() ? 1 : mark.column + 1,
    });
}

void validateKeys(ParseContext& context, const YAML::Node& map, const QSet<QString>& allowed)
{
    if (!map || !map.IsMap()) {
        addError(context, u"type.map_required"_s, u"Ожидался объект YAML."_s, map);
        return;
    }
    for (const auto& entry : map) {
        const auto key = nodeText(entry.first);
        if (!allowed.contains(key)) {
            addError(context, u"field.unknown"_s, u"Неизвестное поле: "_s + key, entry.first);
        }
    }
}

YAML::Node required(ParseContext& context, const YAML::Node& map, const char* key)
{
    const auto node = map[key];
    if (!node) {
        addError(context, u"field.required"_s,
                 u"Отсутствует обязательное поле: "_s + QString::fromLatin1(key), map);
    }
    return node;
}

QString requiredString(ParseContext& context, const YAML::Node& map, const char* key)
{
    const auto node = required(context, map, key);
    if (!node) return {};
    const auto value = nodeText(node).trimmed();
    if (!node.IsScalar() || value.isEmpty()) {
        addError(context, u"type.string_required"_s,
                 u"Поле должно быть непустой строкой."_s, node);
        return {};
    }
    return value;
}

QVector<SubcategoryDefinition> parseSubcategories(
    ParseContext& context,
    const YAML::Node& node)
{
    QVector<SubcategoryDefinition> result;
    if (!node || !node.IsSequence()) {
        addError(context, u"type.sequence_required"_s,
                 u"subcategories должен быть массивом."_s, node);
        return result;
    }

    QSet<QString> ids;
    for (const auto& item : node) {
        validateKeys(context, item, {u"id"_s, u"title"_s});
        SubcategoryDefinition subcategory{
            .id = requiredString(context, item, "id"),
            .title = requiredString(context, item, "title"),
        };
        if (!subcategory.id.isEmpty()) {
            if (ids.contains(subcategory.id)) {
                addError(context, u"subcategory.id_duplicate"_s,
                         u"ID подкатегории уже объявлен: "_s + subcategory.id, item["id"]);
            } else {
                ids.insert(subcategory.id);
            }
        }
        result.append(std::move(subcategory));
    }
    return result;
}

QVector<CategoryDefinition> parseCategories(ParseContext& context, const YAML::Node& node)
{
    QVector<CategoryDefinition> result;
    if (!node || !node.IsSequence()) {
        addError(context, u"type.sequence_required"_s,
                 u"categories должен быть массивом."_s, node);
        return result;
    }

    QSet<QString> ids;
    for (const auto& item : node) {
        validateKeys(context, item, {u"id"_s, u"title"_s, u"subcategories"_s});
        CategoryDefinition category{
            .id = requiredString(context, item, "id"),
            .title = requiredString(context, item, "title"),
        };
        const auto subcategories = required(context, item, "subcategories");
        if (subcategories) category.subcategories = parseSubcategories(context, subcategories);
        if (!category.id.isEmpty()) {
            if (ids.contains(category.id)) {
                addError(context, u"category.id_duplicate"_s,
                         u"ID категории уже объявлен: "_s + category.id, item["id"]);
            } else {
                ids.insert(category.id);
            }
        }
        result.append(std::move(category));
    }
    return result;
}

} // namespace

CategoryCatalogLoadResult CategoryCatalogLoader::loadFile(const QString& filePath) const
{
    CategoryCatalogLoadResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errors.append({
            .code = u"file.read_failed"_s,
            .message = file.errorString(),
            .filePath = filePath,
            .line = 1,
            .column = 1,
        });
        return result;
    }

    ParseContext context{.filePath = filePath, .errors = &result.errors};
    try {
        const auto root = YAML::Load(file.readAll().toStdString());
        validateKeys(context, root, {u"categories"_s});
        const auto categories = parseCategories(context, required(context, root, "categories"));
        if (result.errors.isEmpty()) result.catalog.emplace(categories);
    } catch (const YAML::Exception& exception) {
        const auto mark = exception.mark;
        result.errors.append({
            .code = u"yaml.invalid"_s,
            .message = QString::fromUtf8(exception.what()),
            .filePath = filePath,
            .line = mark.is_null() ? 1 : mark.line + 1,
            .column = mark.is_null() ? 1 : mark.column + 1,
        });
    }
    return result;
}

} // namespace tweakopedia::content
