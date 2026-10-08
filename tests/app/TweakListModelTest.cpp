#include "app/TweakListModel.h"
#include "content/TweakCatalogLoader.h"

#include <QSignalSpy>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class TweakListModelTest final : public QObject
{
    Q_OBJECT

private:
    content::TweakCatalog catalog() const
    {
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        Q_ASSERT(loaded.catalog.has_value());
        return *loaded.catalog;
    }

private slots:
    void exposesStableDocumentedRoles()
    {
        app::TweakListModel model;
        const auto source = catalog();
        const auto id = source.tweaks().first().id;
        model.reset(source, {{id, {
            .status = domain::DetectionStatus::Named,
            .stateId = u"disabled"_s,
            .details = u"Фактическое состояние"_s,
        }}}, {{id, true}});

        QCOMPARE(model.rowCount(), 1);
        const auto index = model.index(0);
        QCOMPARE(model.data(index, app::TweakListModel::IdRole).toString(),
                 u"filesystem.win32-long-paths"_s);
        QCOMPARE(model.data(index, app::TweakListModel::TitleRole).toString(),
                 u"Поддержка длинных путей Win32"_s);
        QVERIFY(!model.data(index, app::TweakListModel::SummaryRole).toString().isEmpty());
        QCOMPARE(model.data(index, app::TweakListModel::CurrentStateRole).toString(), u"disabled"_s);
        QCOMPARE(model.data(index, app::TweakListModel::TargetStateRole).toString(), QString{});
        QVERIFY(model.data(index, app::TweakListModel::SupportedRole).toBool());
        QCOMPARE(model.data(index, app::TweakListModel::ImpactRole).toString(), u"low"_s);
        QCOMPARE(model.data(index, app::TweakListModel::RestartRole).toString(), u"none"_s);

        const auto names = model.roleNames();
        QCOMPARE(names.value(app::TweakListModel::IdRole), QByteArray("id"));
        QCOMPARE(names.value(app::TweakListModel::TitleRole), QByteArray("title"));
        QCOMPARE(names.value(app::TweakListModel::SummaryRole), QByteArray("summary"));
        QCOMPARE(names.value(app::TweakListModel::CurrentStateRole), QByteArray("currentState"));
        QCOMPARE(names.value(app::TweakListModel::TargetStateRole), QByteArray("targetState"));
        QCOMPARE(names.value(app::TweakListModel::SupportedRole), QByteArray("supported"));
        QCOMPARE(names.value(app::TweakListModel::ImpactRole), QByteArray("impact"));
        QCOMPARE(names.value(app::TweakListModel::RestartRole), QByteArray("restart"));
    }

    void targetUpdateChangesOnlyTargetRole()
    {
        app::TweakListModel model;
        const auto source = catalog();
        const auto id = source.tweaks().first().id;
        model.reset(source, {{id, {
            .status = domain::DetectionStatus::Named,
            .stateId = u"disabled"_s,
        }}}, {{id, true}});
        const auto originalTitle = model.data(model.index(0), app::TweakListModel::TitleRole).toString();
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(model.setTargetState(id, u"enabled"_s));

        QCOMPARE(changed.size(), 1);
        const auto roles = changed.first().at(2).value<QList<int>>();
        QCOMPARE(roles, QList<int>{app::TweakListModel::TargetStateRole});
        QCOMPARE(model.data(model.index(0), app::TweakListModel::TitleRole).toString(), originalTitle);
        QCOMPARE(model.data(model.index(0), app::TweakListModel::TargetStateRole).toString(), u"enabled"_s);
    }
};

QTEST_APPLESS_MAIN(TweakListModelTest)

#include "TweakListModelTest.moc"
