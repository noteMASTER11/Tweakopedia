#include "app/TweakGroupListModel.h"

#include <QSignalSpy>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

domain::TweakDefinition tweak(QStringView id, QString category, QString subcategory)
{
    domain::TweakDefinition result;
    result.id = *domain::TweakId::parse(id);
    result.category = std::move(category);
    result.subcategory = std::move(subcategory);
    return result;
}

content::CategoryCatalog categories()
{
    return content::CategoryCatalog({
        {u"behavior"_s, u"Поведение"_s,
         {{u"windows"_s, u"Окна"_s},
          {u"input"_s, u"Ввод"_s},
          {u"media"_s, u"Носители"_s}}},
        {u"single"_s, u"Одна группа"_s,
         {{u"only"_s, u"Единственная"_s}}},
    });
}

content::TweakCatalog tweaks()
{
    return content::TweakCatalog({
        tweak(u"test.windows-one", u"behavior"_s, u"windows"_s),
        tweak(u"test.windows-two", u"behavior"_s, u"windows"_s),
        tweak(u"test.input", u"behavior"_s, u"input"_s),
        tweak(u"test.media", u"behavior"_s, u"media"_s),
        tweak(u"test.only", u"single"_s, u"only"_s),
    });
}

QHash<domain::TweakId, bool> support(bool mediaSupported = false)
{
    QHash<domain::TweakId, bool> result;
    result.insert(*domain::TweakId::parse(u"test.windows-one"), true);
    result.insert(*domain::TweakId::parse(u"test.windows-two"), true);
    result.insert(*domain::TweakId::parse(u"test.input"), true);
    result.insert(*domain::TweakId::parse(u"test.media"), mediaSupported);
    result.insert(*domain::TweakId::parse(u"test.only"), true);
    return result;
}

QString idAt(const app::TweakGroupListModel& model, int row)
{
    return model.data(model.index(row), app::TweakGroupListModel::IdRole).toString();
}

} // namespace

class TweakGroupListModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void preservesYamlOrderAndCounts()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), u"behavior"_s, false);

        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(idAt(model, 0), QString{});
        QCOMPARE(model.data(model.index(0), app::TweakGroupListModel::TitleRole).toString(),
                 u"Все"_s);
        QCOMPARE(model.data(model.index(0), app::TweakGroupListModel::CountRole).toInt(), 4);
        QCOMPARE(idAt(model, 1), u"windows"_s);
        QCOMPARE(model.data(model.index(1), app::TweakGroupListModel::CountRole).toInt(), 2);
        QCOMPARE(idAt(model, 2), u"input"_s);
        QCOMPARE(idAt(model, 3), u"media"_s);
        QVERIFY(model.data(model.index(0), app::TweakGroupListModel::SelectedRole).toBool());
        QVERIFY(model.data(model.index(1), app::TweakGroupListModel::EnabledRole).toBool());
    }

    void hidesUnsupportedOnlyGroupOnlyWhenRequested()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), u"behavior"_s, false);
        QCOMPARE(model.rowCount(), 4);

        model.reset(categories(), tweaks(), support(), u"behavior"_s, true);

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0), app::TweakGroupListModel::CountRole).toInt(), 3);
        QCOMPARE(idAt(model, 1), u"windows"_s);
        QCOMPARE(idAt(model, 2), u"input"_s);
    }

    void preservesSelectionAndRepairsItWhenGroupDisappears()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), u"behavior"_s, false);
        QVERIFY(model.select(u"media"));
        QCOMPARE(model.selectedId(), u"media"_s);
        QSignalSpy selectionChanged(&model, &app::TweakGroupListModel::selectedIdChanged);

        model.reset(categories(), tweaks(), support(), u"behavior"_s, false);
        QCOMPARE(model.selectedId(), u"media"_s);
        QCOMPARE(selectionChanged.count(), 0);

        model.reset(categories(), tweaks(), support(), u"behavior"_s, true);
        QCOMPARE(model.selectedId(), QString{});
        QCOMPARE(selectionChanged.count(), 1);
        QVERIFY(model.data(model.index(0), app::TweakGroupListModel::SelectedRole).toBool());
    }

    void keepsAllAndSingleNonEmptyGroup()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), u"single"_s, true);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(idAt(model, 0), QString{});
        QCOMPARE(idAt(model, 1), u"only"_s);
    }

    void adjacentSelectionStopsAtBoundaries()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), u"behavior"_s, true);

        QCOMPARE(model.adjacentId(-1), QString{});
        QCOMPARE(model.adjacentId(1), u"windows"_s);
        QVERIFY(model.select(u"windows"));
        QCOMPARE(model.adjacentId(-1), QString{});
        QCOMPARE(model.adjacentId(1), u"input"_s);
        QVERIFY(model.select(u"input"));
        QCOMPARE(model.adjacentId(1), u"input"_s);
        QVERIFY(!model.select(u"missing"));
        QCOMPARE(model.selectedId(), u"input"_s);
    }

    void emptyOrUnknownCategoryHasNoRows()
    {
        app::TweakGroupListModel model;
        model.reset(categories(), tweaks(), support(), {}, false);
        QCOMPARE(model.rowCount(), 0);
        model.reset(categories(), tweaks(), support(), u"missing"_s, false);
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_APPLESS_MAIN(TweakGroupListModelTest)

#include "TweakGroupListModelTest.moc"
