#include "content/CategoryCatalogLoader.h"
#include "content/CategoryMembershipValidator.h"
#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QtTest/QTest>

#include <algorithm>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::TweakDefinition tweak(QStringView id, QString category, QString subcategory)
{
    domain::TweakDefinition definition;
    definition.id = *domain::TweakId::parse(id);
    definition.category = std::move(category);
    definition.subcategory = std::move(subcategory);
    return definition;
}

bool hasError(const QVector<content::CatalogError>& errors, QStringView code)
{
    return std::any_of(errors.cbegin(), errors.cend(), [code](const auto& error) {
        return error.code == code;
    });
}

} // namespace

class CategoryMembershipValidatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnknownCategory()
    {
        const content::CategoryCatalog categories({
            {u"known"_s, u"Известная"_s, {{u"shared"_s, u"Общая"_s}}},
        });
        const content::TweakCatalog tweaks({
            tweak(u"test.unknown-category", u"missing"_s, u"shared"_s),
        });

        const auto errors = content::CategoryMembershipValidator{}.validate(categories, tweaks);

        QVERIFY(hasError(errors, u"category.reference_unknown"));
    }

    void rejectsMissingAndUnknownSubcategory()
    {
        const content::CategoryCatalog categories({
            {u"known"_s, u"Известная"_s, {{u"shared"_s, u"Общая"_s}}},
        });
        const content::TweakCatalog tweaks({
            tweak(u"test.missing-subcategory", u"known"_s, {}),
            tweak(u"test.unknown-subcategory", u"known"_s, u"missing"_s),
            tweak(u"test.valid", u"known"_s, u"shared"_s),
        });

        const auto errors = content::CategoryMembershipValidator{}.validate(categories, tweaks);

        QCOMPARE(std::count_if(errors.cbegin(), errors.cend(), [](const auto& error) {
            return error.code == u"subcategory.reference_unknown";
        }), 2);
    }

    void rejectsConfiguredSubcategoryWithoutTweaks()
    {
        const content::CategoryCatalog categories({
            {u"known"_s, u"Известная"_s,
             {{u"used"_s, u"Используется"_s}, {u"empty"_s, u"Пустая"_s}}},
        });
        const content::TweakCatalog tweaks({
            tweak(u"test.used", u"known"_s, u"used"_s),
        });

        const auto errors = content::CategoryMembershipValidator{}.validate(categories, tweaks);

        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().code, u"subcategory.empty"_s);
        QVERIFY(errors.first().message.contains(u"known/empty"_s));
    }

    void scopesDuplicateSubcategoryIdsByCategory()
    {
        const content::CategoryCatalog categories({
            {u"first"_s, u"Первая"_s, {{u"shared"_s, u"Общая 1"_s}}},
            {u"second"_s, u"Вторая"_s, {{u"shared"_s, u"Общая 2"_s}}},
        });
        const content::TweakCatalog tweaks({
            tweak(u"test.first", u"first"_s, u"shared"_s),
            tweak(u"test.second", u"second"_s, u"shared"_s),
        });

        const auto errors = content::CategoryMembershipValidator{}.validate(categories, tweaks);

        QVERIFY(errors.isEmpty());
    }

    void acceptsCompleteRealCatalog()
    {
        const QDir contentRoot(QStringLiteral(TWEAKOPEDIA_CONTENT_ROOT));
        const auto categories = content::CategoryCatalogLoader{}.loadFile(
            contentRoot.filePath(u"categories.yaml"_s));
        const auto tweaks = content::TweakCatalogLoader{}.loadDirectory(
            contentRoot.filePath(u"tweaks"_s));

        QVERIFY(categories.catalog.has_value());
        QVERIFY2(categories.errors.isEmpty(), qPrintable(categories.errors.value(0).message));
        QVERIFY(tweaks.catalog.has_value());
        QVERIFY2(tweaks.errors.isEmpty(), qPrintable(tweaks.errors.value(0).message));
        QCOMPARE(tweaks.catalog->size(), 1109);

        const auto errors = content::CategoryMembershipValidator{}.validate(
            *categories.catalog, *tweaks.catalog);

        QVERIFY2(errors.isEmpty(), qPrintable(errors.value(0).message));
    }
};

QTEST_APPLESS_MAIN(CategoryMembershipValidatorTest)

#include "CategoryMembershipValidatorTest.moc"
