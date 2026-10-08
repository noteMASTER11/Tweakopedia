#include "app/EncyclopediaArticleModel.h"

#include <QSet>
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
    QString registryValue = {})
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(id);
    tweak.title = std::move(title);
    tweak.category = std::move(category);
    tweak.subcategory = std::move(subcategory);
    tweak.summary = std::move(summary);
    tweak.explanation = {
        .purpose = u"Для чего нужен параметр."_s,
        .mechanism = u"Как Windows обрабатывает параметр."_s,
        .effect = u"Что изменится после применения."_s,
        .tradeoffs = u"Какие функции будут затронуты."_s,
        .recommendation = u"Когда параметр уместен."_s,
        .technicalDetails = u"Техническое описание параметра."_s,
    };
    tweak.compatibility = {
        .architectures = {domain::CpuArchitecture::X64},
        .operatingSystems = {domain::WindowsFamily::Windows10, domain::WindowsFamily::Windows11},
        .minimumBuild = 19041,
        .maximumBuild = 26200,
    };
    tweak.restart = domain::RestartRequirement::Reboot;
    if (!registryValue.isEmpty()) {
        tweak.detection = domain::RegistryDwordDetection{
            .location = {
                .hive = domain::RegistryHive::LocalMachine,
                .key = u"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection"_s,
                .valueName = std::move(registryValue),
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
        {
            .id = u"system"_s,
            .title = u"Система"_s,
            .subcategories = {{.id = u"diagnostics"_s, .title = u"Диагностика"_s}},
        },
    });
}

QVariantMap relatedById(const QVariantList& related, QStringView id)
{
    for (const auto& value : related) {
        const auto map = value.toMap();
        if (map.value(u"id"_s).toString() == id) return map;
    }
    return {};
}

} // namespace

class EncyclopediaArticleModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsOrderedArticleSectionsAndMetadata()
    {
        auto primary = makeTweak(
            u"privacy.diagnostic-data",
            u"Диагностические данные"_s,
            u"privacy"_s,
            u"diagnostics"_s,
            u"Управляет объёмом диагностических сведений."_s,
            u"AllowTelemetry"_s);
        primary.explanation.tradeoffs.clear();
        const content::TweakCatalog catalog({primary});
        const auto categoryCatalog = categories();
        app::EncyclopediaArticleModel model;
        model.reset(catalog, categoryCatalog);

        QVERIFY(model.selectArticle(u"privacy.diagnostic-data"_s));

        QVERIFY(model.hasArticle());
        const auto article = model.article();
        QCOMPARE(article.value(u"id"_s).toString(), u"privacy.diagnostic-data"_s);
        QCOMPARE(article.value(u"title"_s).toString(), u"Диагностические данные"_s);
        QCOMPARE(article.value(u"breadcrumbs"_s).toStringList(),
                 QStringList({u"Твикопедия"_s, u"Конфиденциальность"_s,
                              u"Диагностика и телеметрия"_s}));
        const auto sections = article.value(u"sections"_s).toList();
        QCOMPARE(sections.size(), 5);
        QCOMPARE(sections.at(0).toMap().value(u"id"_s).toString(), u"purpose"_s);
        QCOMPARE(sections.at(1).toMap().value(u"id"_s).toString(), u"mechanism"_s);
        QCOMPARE(sections.at(2).toMap().value(u"id"_s).toString(), u"effect"_s);
        QCOMPARE(sections.at(3).toMap().value(u"id"_s).toString(), u"recommendation"_s);
        QCOMPARE(sections.at(4).toMap().value(u"id"_s).toString(), u"technical"_s);
        const auto technical = article.value(u"technicalObjects"_s).toStringList();
        QCOMPARE(technical, QStringList({
            u"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection\\AllowTelemetry"_s,
        }));
        const auto compatibility = article.value(u"compatibility"_s).toMap();
        QCOMPARE(compatibility.value(u"operatingSystems"_s).toStringList(),
                 QStringList({u"Windows 10"_s, u"Windows 11"_s}));
        QCOMPARE(compatibility.value(u"minimumBuild"_s).toUInt(), 19041U);
        QCOMPARE(compatibility.value(u"maximumBuild"_s).toUInt(), 26200U);
        QCOMPARE(article.value(u"restart"_s).toMap().value(u"title"_s).toString(),
                 u"Перезагрузка ПК"_s);
    }

    void rejectsUnknownArticleAndClearsSelection()
    {
        const content::TweakCatalog catalog({makeTweak(
            u"privacy.diagnostic-data", u"Диагностические данные"_s,
            u"privacy"_s, u"diagnostics"_s, u"Описание."_s)});
        const auto categoryCatalog = categories();
        app::EncyclopediaArticleModel model;
        model.reset(catalog, categoryCatalog);
        QVERIFY(model.selectArticle(u"privacy.diagnostic-data"_s));

        QVERIFY(!model.selectArticle(u"privacy.does-not-exist"_s));
        QVERIFY(!model.hasArticle());
        QVERIFY(model.article().isEmpty());
    }

    void ranksAndDeduplicatesRelatedArticles()
    {
        auto primary = makeTweak(
            u"privacy.diagnostic-data", u"Диагностические данные"_s,
            u"privacy"_s, u"diagnostics"_s, u"Уровень диагностических данных."_s,
            u"AllowTelemetry"_s);
        auto explicitRelation = makeTweak(
            u"system.explicit-relation", u"Явно связанный параметр"_s,
            u"system"_s, u"diagnostics"_s, u"Другая тема."_s);
        primary.dependencies.append(explicitRelation.id);
        explicitRelation.conflicts.append(primary.id);
        auto sameSubcategory = makeTweak(
            u"privacy.telemetry-details", u"Подробности телеметрии"_s,
            u"privacy"_s, u"diagnostics"_s, u"Диагностические данные Windows."_s);
        auto sameCategory = makeTweak(
            u"privacy.advertising-id", u"Рекламный идентификатор"_s,
            u"privacy"_s, u"advertising"_s, u"Персонализация приложений."_s);
        auto sharedObject = makeTweak(
            u"system.shared-object", u"Общий объект политики"_s,
            u"system"_s, u"diagnostics"_s, u"Системная политика."_s,
            u"AllowTelemetry"_s);
        QVector<domain::TweakDefinition> definitions{
            primary, explicitRelation, sameSubcategory, sameCategory, sharedObject,
        };
        for (int index = 0; index < 5; ++index) {
            definitions.append(makeTweak(
                u"system.extra-%1"_s.arg(index),
                u"Диагностический материал %1"_s.arg(index),
                u"system"_s,
                u"diagnostics"_s,
                u"Дополнительный материал о диагностических данных."_s));
        }
        const content::TweakCatalog catalog(std::move(definitions));
        const auto categoryCatalog = categories();
        app::EncyclopediaArticleModel model;
        model.reset(catalog, categoryCatalog);

        QVERIFY(model.selectArticle(primary.id.toString()));

        const auto related = model.article().value(u"relatedArticles"_s).toList();
        QCOMPARE(related.size(), 6);
        QCOMPARE(related.first().toMap().value(u"id"_s).toString(),
                 u"system.explicit-relation"_s);
        QVERIFY(!relatedById(related, u"privacy.telemetry-details").isEmpty());
        QVERIFY(!relatedById(related, u"privacy.advertising-id").isEmpty());
        QVERIFY(!relatedById(related, u"system.shared-object").isEmpty());
        QSet<QString> ids;
        for (const auto& value : related) ids.insert(value.toMap().value(u"id"_s).toString());
        QCOMPARE(ids.size(), related.size());
        QVERIFY(!ids.contains(primary.id.toString()));
    }
};

QTEST_APPLESS_MAIN(EncyclopediaArticleModelTest)

#include "EncyclopediaArticleModelTest.moc"
