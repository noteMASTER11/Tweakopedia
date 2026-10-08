#include "app/TweakFilterProxyModel.h"
#include "app/TweakListModel.h"
#include "content/TweakCatalogLoader.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class TweakFilterProxyModelTest final : public QObject
{
    Q_OBJECT

private:
    static content::TweakCatalog catalog()
    {
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        Q_ASSERT(loaded.catalog.has_value());
        const auto id = *domain::TweakId::parse(u"filesystem.win32-long-paths");
        const auto* longPaths = loaded.catalog->find(id);
        Q_ASSERT(longPaths != nullptr);
        auto first = *longPaths;
        auto second = first;
        second.id = *domain::TweakId::parse(u"privacy.diagnostics"_s);
        second.title = u"Диагностические данные"_s;
        second.summary = u"Управляет отправкой сведений"_s;
        second.category = u"privacy"_s;
        auto third = first;
        third.id = *domain::TweakId::parse(u"filesystem.trim"_s);
        third.title = u"Оптимизация накопителей"_s;
        third.summary = u"Управляет командой TRIM"_s;
        return content::TweakCatalog({first, second, third});
    }

    static void populate(app::TweakListModel& model)
    {
        const auto source = catalog();
        QHash<domain::TweakId, domain::DetectedState> states;
        QHash<domain::TweakId, bool> supported;
        for (const auto& tweak : source.tweaks()) {
            states.insert(tweak.id, {
                .status = domain::DetectionStatus::Named,
                .stateId = u"disabled"_s,
            });
            supported.insert(tweak.id, true);
        }
        model.reset(source, states, supported);
    }

private slots:
    void filtersByCaseInsensitiveTitleAndSummary()
    {
        app::TweakListModel source;
        populate(source);
        app::TweakFilterProxyModel proxy;
        proxy.setSourceModel(&source);

        proxy.setQuery(u"ДИАГНОСТИЧЕСКИЕ"_s);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.data(proxy.index(0, 0), app::TweakListModel::TitleRole).toString(),
                 u"Диагностические данные"_s);

        proxy.setQuery(u"trim"_s);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.data(proxy.index(0, 0), app::TweakListModel::TitleRole).toString(),
                 u"Оптимизация накопителей"_s);
    }

    void combinesCategoryAndQueryWithoutReordering()
    {
        app::TweakListModel source;
        populate(source);
        app::TweakFilterProxyModel proxy;
        proxy.setSourceModel(&source);

        QCOMPARE(proxy.rowCount(), 3);
        proxy.setCategoryId(u"filesystem"_s);
        QCOMPARE(proxy.rowCount(), 2);
        QCOMPARE(proxy.data(proxy.index(0, 0), app::TweakListModel::IdRole).toString(),
                 u"filesystem.win32-long-paths"_s);
        QCOMPARE(proxy.data(proxy.index(1, 0), app::TweakListModel::IdRole).toString(),
                 u"filesystem.trim"_s);

        proxy.setQuery(u"накопителей"_s);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.data(proxy.index(0, 0), app::TweakListModel::IdRole).toString(),
                 u"filesystem.trim"_s);

        proxy.setQuery(u"отсутствует"_s);
        QCOMPARE(proxy.rowCount(), 0);

        proxy.setQuery({});
        proxy.setCategoryId({});
        QCOMPARE(proxy.rowCount(), 3);
    }
};

QTEST_APPLESS_MAIN(TweakFilterProxyModelTest)

#include "TweakFilterProxyModelTest.moc"
