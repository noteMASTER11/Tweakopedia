#include "content/CategoryCatalogLoader.h"
#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <algorithm>

using namespace tweakopedia::content;
using namespace Qt::StringLiterals;

namespace {

QString fixture(QStringView name)
{
    return QDir(QStringLiteral(TWEAKOPEDIA_CATEGORY_FIXTURES)).filePath(name.toString());
}

bool hasError(const CategoryCatalogLoadResult& result, QStringView code)
{
    return std::any_of(result.errors.cbegin(), result.errors.cend(), [code](const CatalogError& error) {
        return error.code == code;
    });
}

CategoryCatalogLoadResult loadText(QByteArray yaml)
{
    QTemporaryDir directory;
    if (!directory.isValid()) return {};
    const auto path = QDir(directory.path()).filePath(u"categories.yaml"_s);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(yaml) != yaml.size()) return {};
    file.close();
    return CategoryCatalogLoader{}.loadFile(path);
}

} // namespace

class CategoryCatalogLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void preservesOrderAndRussianTitles()
    {
        const auto result = CategoryCatalogLoader{}.loadFile(fixture(u"valid.yaml"));

        QVERIFY(result.errors.isEmpty());
        QVERIFY(result.catalog.has_value());
        QCOMPARE(result.catalog->size(), 2);
        QCOMPARE(result.catalog->categories().at(0).id, u"filesystem"_s);
        QCOMPARE(result.catalog->categories().at(0).title,
                 u"Накопители, NTFS и файловая система"_s);
        QCOMPARE(result.catalog->categories().at(0).subcategories.at(0).id, u"paths"_s);
        QCOMPARE(result.catalog->categories().at(0).subcategories.at(1).id, u"storage"_s);
        QCOMPARE(result.catalog->categories().at(1).id, u"privacy"_s);
        QVERIFY(result.catalog->find(u"filesystem") != nullptr);
        QVERIFY(result.catalog->find(u"missing") == nullptr);
    }

    void rejectsDuplicateCategoryAndSubcategoryIds()
    {
        const auto result = CategoryCatalogLoader{}.loadFile(fixture(u"duplicate.yaml"));

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasError(result, u"category.id_duplicate"));
        QVERIFY(hasError(result, u"subcategory.id_duplicate"));
    }

    void rejectsUnknownFields()
    {
        const auto result = CategoryCatalogLoader{}.loadFile(fixture(u"unknown-field.yaml"));

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasError(result, u"field.unknown"));
    }

    void rejectsEmptyIdsAndTitles()
    {
        const auto result = loadText(R"(
categories:
  - id: ""
    title: ""
    subcategories:
      - id: ""
        title: ""
)");

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasError(result, u"type.string_required"));
    }

    void loadsRealCategoriesAndResolvesEveryTweakReference()
    {
        const auto categoriesPath = QDir(QStringLiteral(TWEAKOPEDIA_CONTENT_ROOT))
            .filePath(u"categories.yaml"_s);
        const auto categories = CategoryCatalogLoader{}.loadFile(categoriesPath);
        const auto tweaks = TweakCatalogLoader{}.loadDirectory(
            QDir(QStringLiteral(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"tweaks"_s));

        QVERIFY(categories.errors.isEmpty());
        QVERIFY(categories.catalog.has_value());
        QVERIFY(tweaks.errors.isEmpty());
        QVERIFY(tweaks.catalog.has_value());
        for (const auto& tweak : tweaks.catalog->tweaks()) {
            const auto* category = categories.catalog->find(tweak.category);
            QVERIFY2(category != nullptr, qPrintable(tweak.category));
            const auto subcategory = std::find_if(
                category->subcategories.cbegin(), category->subcategories.cend(),
                [&tweak](const SubcategoryDefinition& item) {
                    return item.id == tweak.subcategory;
                });
            QVERIFY2(subcategory != category->subcategories.cend(), qPrintable(tweak.subcategory));
        }
    }
};

QTEST_APPLESS_MAIN(CategoryCatalogLoaderTest)

#include "CategoryCatalogLoaderTest.moc"
