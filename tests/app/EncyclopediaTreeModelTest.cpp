#include "app/EncyclopediaTreeModel.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::TweakDefinition makeTweak(
    QStringView id,
    QString title,
    QString category,
    QString subcategory,
    QString summary,
    QString registryKey = {})
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(id);
    tweak.title = std::move(title);
    tweak.category = std::move(category);
    tweak.subcategory = std::move(subcategory);
    tweak.summary = std::move(summary);
    tweak.explanation = {
        .purpose = u"Назначение параметра."_s,
        .mechanism = u"Механизм Windows."_s,
        .effect = u"Практический эффект."_s,
        .tradeoffs = u"Ограничения."_s,
        .recommendation = u"Рекомендация."_s,
        .technicalDetails = u"Технические сведения."_s,
    };
    if (!registryKey.isEmpty()) {
        tweak.detection = domain::RegistryDwordDetection{
            .location = {
                .hive = domain::RegistryHive::LocalMachine,
                .key = std::move(registryKey),
                .valueName = u"AllowTelemetry"_s,
                .view = domain::RegistryView::Registry64,
            },
            .statesByValue = {{0, u"disabled"_s}},
            .missingState = u"disabled"_s,
        };
    }
    return tweak;
}

content::CategoryCatalog categories()
{
    return content::CategoryCatalog({
        {
            .id = u"privacy"_s,
            .title = u"Конфиденциальность"_s,
            .subcategories = {
                {.id = u"diagnostics"_s, .title = u"Диагностика и телеметрия"_s},
                {.id = u"advertising"_s, .title = u"Реклама"_s},
            },
        },
    });
}

content::TweakCatalog tweaks()
{
    return content::TweakCatalog({
        makeTweak(
            u"privacy.diagnostic-data",
            u"Диагностические данные"_s,
            u"privacy"_s,
            u"diagnostics"_s,
            u"Управляет объёмом сведений Windows."_s,
            u"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection"_s),
        makeTweak(
            u"privacy.telemetry-details",
            u"Подробности телеметрии"_s,
            u"privacy"_s,
            u"diagnostics"_s,
            u"Объясняет, как используются диагностические данные."_s),
        makeTweak(
            u"privacy.advertising-id",
            u"Рекламный идентификатор"_s,
            u"privacy"_s,
            u"advertising"_s,
            u"Управляет персонализацией рекламы."_s),
        makeTweak(
            u"orphan.hidden-setting",
            u"Неизвестный раздел"_s,
            u"missing-category"_s,
            u"missing-subcategory"_s,
            u"Материал без объявления категории."_s),
    });
}

QModelIndex childByTitle(
    const app::EncyclopediaTreeModel& model,
    QStringView title,
    const QModelIndex& parent = {})
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const auto index = model.index(row, 0, parent);
        if (model.data(index, app::EncyclopediaTreeModel::TitleRole).toString() == title) {
            return index;
        }
    }
    return {};
}

} // namespace

class EncyclopediaTreeModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsCategorySubcategoryArticleHierarchy()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();

        model.reset(catalog, categoryCatalog);

        QCOMPARE(model.rowCount(), 2);
        const auto privacy = childByTitle(model, u"Конфиденциальность");
        QVERIFY(privacy.isValid());
        QCOMPARE(model.data(privacy, app::EncyclopediaTreeModel::NodeTypeRole).toString(), u"category"_s);
        QCOMPARE(model.rowCount(privacy), 2);
        const auto diagnostics = childByTitle(model, u"Диагностика и телеметрия", privacy);
        QVERIFY(diagnostics.isValid());
        QCOMPARE(model.rowCount(diagnostics), 2);
        const auto article = childByTitle(model, u"Диагностические данные", diagnostics);
        QVERIFY(article.isValid());
        QCOMPARE(model.data(article, app::EncyclopediaTreeModel::IdRole).toString(),
                 u"privacy.diagnostic-data"_s);
        QCOMPARE(model.data(article, app::EncyclopediaTreeModel::PathRole).toString(),
                 u"Конфиденциальность › Диагностика и телеметрия"_s);
        QCOMPARE(model.parent(article), diagnostics);
    }

    void placesOrphanTweaksUnderOtherMaterials()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();

        model.reset(catalog, categoryCatalog);

        const auto other = childByTitle(model, u"Другие материалы");
        QVERIFY(other.isValid());
        QCOMPARE(model.rowCount(other), 1);
        const auto subsection = model.index(0, 0, other);
        QCOMPARE(model.data(subsection, app::EncyclopediaTreeModel::TitleRole).toString(),
                 u"Без раздела"_s);
        QCOMPARE(model.rowCount(subsection), 1);
        QCOMPARE(model.data(model.index(0, 0, subsection), app::EncyclopediaTreeModel::IdRole).toString(),
                 u"orphan.hidden-setting"_s);
    }

    void normalizesAndRanksSearchResults()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        model.reset(catalog, categoryCatalog);

        model.setQuery(u"  ДИАГНОСТИЧЕСКИЕ,   ДАННЫЕ  "_s);

        QCOMPARE(model.rowCount(), 1);
        const auto privacy = model.index(0, 0);
        QCOMPARE(model.rowCount(privacy), 1);
        const auto diagnostics = model.index(0, 0, privacy);
        QCOMPARE(model.rowCount(diagnostics), 2);
        const auto first = model.index(0, 0, diagnostics);
        const auto second = model.index(1, 0, diagnostics);
        QCOMPARE(model.data(first, app::EncyclopediaTreeModel::IdRole).toString(),
                 u"privacy.diagnostic-data"_s);
        QVERIFY(model.data(first, app::EncyclopediaTreeModel::MatchScoreRole).toInt()
                > model.data(second, app::EncyclopediaTreeModel::MatchScoreRole).toInt());
    }

    void searchesIdsAndTechnicalObjects()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        model.reset(catalog, categoryCatalog);

        model.setQuery(u"privacy.diagnostic-data"_s);
        QCOMPARE(model.articleCount(), 1);

        model.setQuery(u"DataCollection AllowTelemetry"_s);
        QCOMPARE(model.articleCount(), 1);
        const auto privacy = model.index(0, 0);
        const auto diagnostics = model.index(0, 0, privacy);
        QCOMPARE(model.data(model.index(0, 0, diagnostics), app::EncyclopediaTreeModel::IdRole).toString(),
                 u"privacy.diagnostic-data"_s);

        model.setQuery({});
        QCOMPARE(model.articleCount(), 4);
        QCOMPARE(model.rowCount(), 2);
    }

    void searchesLocalizedCategoryAndSubcategoryTitles()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        model.reset(catalog, categoryCatalog);

        model.setQuery(u"конфиденциальность"_s);
        QCOMPARE(model.articleCount(), 3);

        model.setQuery(u"диагностика телеметрия"_s);
        QCOMPARE(model.articleCount(), 2);
        const auto privacy = model.index(0, 0);
        const auto diagnostics = model.index(0, 0, privacy);
        QCOMPARE(model.data(diagnostics, app::EncyclopediaTreeModel::TitleRole).toString(),
                 u"Диагностика и телеметрия"_s);
    }

    void marksOnlySelectedVisibleArticle()
    {
        app::EncyclopediaTreeModel model;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        model.reset(catalog, categoryCatalog);

        model.setSelectedArticleId(u"privacy.telemetry-details"_s);

        const auto privacy = childByTitle(model, u"Конфиденциальность");
        const auto diagnostics = childByTitle(model, u"Диагностика и телеметрия", privacy);
        const auto first = model.index(0, 0, diagnostics);
        const auto second = model.index(1, 0, diagnostics);
        QVERIFY(!model.data(first, app::EncyclopediaTreeModel::SelectedRole).toBool());
        QVERIFY(model.data(second, app::EncyclopediaTreeModel::SelectedRole).toBool());
    }
};

QTEST_APPLESS_MAIN(EncyclopediaTreeModelTest)

#include "EncyclopediaTreeModelTest.moc"
