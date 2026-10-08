#include "app/EncyclopediaController.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::TweakDefinition makeTweak(
    QStringView id, QString title, QString subcategory, QString summary)
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(id);
    tweak.title = std::move(title);
    tweak.category = u"privacy"_s;
    tweak.subcategory = std::move(subcategory);
    tweak.summary = std::move(summary);
    tweak.explanation = {
        .purpose = u"Назначение."_s,
        .mechanism = u"Механизм."_s,
        .effect = u"Эффект."_s,
        .tradeoffs = u"Ограничения."_s,
        .recommendation = u"Рекомендация."_s,
        .technicalDetails = u"Технические сведения."_s,
    };
    return tweak;
}

content::TweakCatalog tweaks()
{
    return content::TweakCatalog({
        makeTweak(u"privacy.diagnostic-data", u"Диагностические данные"_s,
                  u"diagnostics"_s, u"Сбор диагностических данных."_s),
        makeTweak(u"privacy.telemetry-details", u"Подробности телеметрии"_s,
                  u"diagnostics"_s, u"Описание телеметрии."_s),
        makeTweak(u"privacy.advertising-id", u"Рекламный идентификатор"_s,
                  u"advertising"_s, u"Идентификатор для приложений."_s),
    });
}

content::CategoryCatalog categories()
{
    return content::CategoryCatalog({{
        .id = u"privacy"_s,
        .title = u"Конфиденциальность"_s,
        .subcategories = {
            {.id = u"diagnostics"_s, .title = u"Диагностика и телеметрия"_s},
            {.id = u"advertising"_s, .title = u"Реклама"_s},
        },
    }});
}

int selectedCount(const app::EncyclopediaTreeModel& model, const QModelIndex& parent = {})
{
    int count = 0;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const auto index = model.index(row, 0, parent);
        if (model.data(index, app::EncyclopediaTreeModel::SelectedRole).toBool()) ++count;
        count += selectedCount(model, index);
    }
    return count;
}

} // namespace

class EncyclopediaControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void opensArticleAndSynchronizesTreeSelection()
    {
        app::EncyclopediaController controller;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        controller.reset(catalog, categoryCatalog);

        QVERIFY(controller.openArticle(u"privacy.diagnostic-data"_s));

        QVERIFY(controller.article()->hasArticle());
        QCOMPARE(controller.article()->selectedArticleId(), u"privacy.diagnostic-data"_s);
        QCOMPARE(selectedCount(*controller.tree()), 1);
        QVERIFY(!controller.canGoBack());
        QVERIFY(!controller.canGoForward());
        QVERIFY(!controller.loading());
    }

    void navigatesBackForwardWithoutDuplicateEntries()
    {
        app::EncyclopediaController controller;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        controller.reset(catalog, categoryCatalog);
        QVERIFY(controller.openArticle(u"privacy.diagnostic-data"_s));
        QVERIFY(controller.openArticle(u"privacy.telemetry-details"_s));
        QVERIFY(controller.openArticle(u"privacy.telemetry-details"_s));

        QVERIFY(controller.canGoBack());
        QVERIFY(controller.goBack());
        QCOMPARE(controller.article()->selectedArticleId(), u"privacy.diagnostic-data"_s);
        QVERIFY(!controller.canGoBack());
        QVERIFY(controller.canGoForward());
        QVERIFY(controller.goForward());
        QCOMPARE(controller.article()->selectedArticleId(), u"privacy.telemetry-details"_s);

        QVERIFY(controller.goBack());
        QVERIFY(controller.openArticle(u"privacy.advertising-id"_s));
        QVERIFY(!controller.canGoForward());
    }

    void searchCanHideSelectionWithoutClosingArticle()
    {
        app::EncyclopediaController controller;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        controller.reset(catalog, categoryCatalog);
        QVERIFY(controller.openArticle(u"privacy.diagnostic-data"_s));

        controller.setQuery(u"рекламный идентификатор"_s);

        QVERIFY(controller.article()->hasArticle());
        QCOMPARE(controller.article()->selectedArticleId(), u"privacy.diagnostic-data"_s);
        QCOMPARE(controller.tree()->articleCount(), 1);
        QCOMPARE(selectedCount(*controller.tree()), 0);
        controller.clearSearch();
        QCOMPARE(controller.tree()->articleCount(), 3);
        QCOMPARE(selectedCount(*controller.tree()), 1);
    }

    void unknownArticleReturnsToInitialState()
    {
        app::EncyclopediaController controller;
        const auto catalog = tweaks();
        const auto categoryCatalog = categories();
        controller.reset(catalog, categoryCatalog);
        QVERIFY(controller.openArticle(u"privacy.diagnostic-data"_s));

        QVERIFY(!controller.openArticle(u"privacy.missing"_s));

        QVERIFY(!controller.article()->hasArticle());
        QCOMPARE(selectedCount(*controller.tree()), 0);
    }
};

QTEST_APPLESS_MAIN(EncyclopediaControllerTest)

#include "EncyclopediaControllerTest.moc"
