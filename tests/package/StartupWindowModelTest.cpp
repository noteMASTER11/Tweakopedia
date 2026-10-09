#include "bootstrap/StartupWindow.h"

#include <QtTest>

#include <vector>

using namespace tweakopedia;

class StartupWindowModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesCurrentAndTwoUpcomingRealStages()
    {
        bootstrap::StartupWindowModel model;
        model.advance(startup::StartupStage::ContainerIntegrity);
        QCOMPARE(model.visibleStages(), std::vector({
            startup::StartupStage::ContainerIntegrity,
            startup::StartupStage::LocalData,
            startup::StartupStage::RuntimeCache,
        }));

        model.advance(startup::StartupStage::RuntimeCache);
        QCOMPARE(model.visibleStages(), std::vector({
            startup::StartupStage::RuntimeCache,
            startup::StartupStage::AppLaunch,
            startup::StartupStage::DataStore,
        }));
    }

    void insertsExtractionOnlyWhenItActuallyStarts()
    {
        bootstrap::StartupWindowModel model;
        model.advance(startup::StartupStage::RuntimeExtract);
        QCOMPARE(model.visibleStages(), std::vector({
            startup::StartupStage::RuntimeExtract,
            startup::StartupStage::AppLaunch,
            startup::StartupStage::DataStore,
        }));
    }

    void ignoresUnknownAndStopsOnlyOnReady()
    {
        bootstrap::StartupWindowModel model;
        model.advance(startup::StartupStage::LocalData);
        const auto before = model.visibleStages();
        QVERIFY(!model.complete());

        model.advance(static_cast<startup::StartupStage>(9999));
        QCOMPARE(model.visibleStages(), before);
        QVERIFY(!model.complete());

        model.advance(startup::StartupStage::Ready);
        QVERIFY(model.complete());
        QVERIFY(model.visibleStages().empty());
    }
};

QTEST_GUILESS_MAIN(StartupWindowModelTest)

#include "StartupWindowModelTest.moc"
