#include "startup/StartupIpc.h"
#include "startup/StartupStage.h"

#include <QtTest>

#include <array>
#include <cstdint>
#include <vector>

using namespace tweakopedia;

class StartupStageTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesExactRussianLabels()
    {
        using startup::StartupStage;
        const std::array expected{
            std::pair{StartupStage::ContainerIntegrity, L"Проверка целостности приложения"},
            std::pair{StartupStage::LocalData, L"Подготовка локальных данных"},
            std::pair{StartupStage::RuntimeCache, L"Проверка локального кэша"},
            std::pair{StartupStage::RuntimeExtract, L"Извлечение встроенных компонентов"},
            std::pair{StartupStage::AppLaunch, L"Запуск компонентов приложения"},
            std::pair{StartupStage::DataStore, L"Открытие локальных данных"},
            std::pair{StartupStage::CatalogLoad, L"Загрузка каталога твиков"},
            std::pair{StartupStage::CatalogValidate, L"Проверка структуры каталога"},
            std::pair{StartupStage::WindowsProfile, L"Определение версии Windows"},
            std::pair{StartupStage::StateDetection, L"Определение текущих значений параметров"},
            std::pair{StartupStage::Models, L"Подготовка разделов приложения"},
            std::pair{StartupStage::Interface, L"Создание интерфейса"},
        };

        for (const auto& [stage, label] : expected) {
            QCOMPARE(startup::startupStageText(stage), std::wstring_view(label));
        }
        QCOMPARE(startup::startupStageText(StartupStage::Ready), std::wstring_view{});
    }

    void buildsConditionalPipeline()
    {
        using startup::StartupStage;
        const std::vector withoutExtraction{
            StartupStage::ContainerIntegrity, StartupStage::LocalData,
            StartupStage::RuntimeCache, StartupStage::AppLaunch,
            StartupStage::DataStore, StartupStage::CatalogLoad,
            StartupStage::CatalogValidate, StartupStage::WindowsProfile,
            StartupStage::StateDetection, StartupStage::Models,
            StartupStage::Interface,
        };
        auto withExtraction = withoutExtraction;
        withExtraction.insert(withExtraction.begin() + 3, StartupStage::RuntimeExtract);

        QCOMPARE(startup::startupPipeline(false), withoutExtraction);
        QCOMPARE(startup::startupPipeline(true), withExtraction);
    }

    void roundTripsKnownMessage()
    {
        const auto encoded = startup::encodeStartupMessage(startup::StartupStage::CatalogLoad);
        const auto decoded = startup::decodeStartupMessage(&encoded, sizeof(encoded));
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, startup::StartupStage::CatalogLoad);
    }

    void rejectsMalformedMessages()
    {
        auto encoded = startup::encodeStartupMessage(startup::StartupStage::CatalogLoad);
        QVERIFY(!startup::decodeStartupMessage(nullptr, sizeof(encoded)).has_value());
        QVERIFY(!startup::decodeStartupMessage(&encoded, sizeof(encoded) - 1).has_value());

        encoded.magic ^= 1U;
        QVERIFY(!startup::decodeStartupMessage(&encoded, sizeof(encoded)).has_value());
        encoded = startup::encodeStartupMessage(startup::StartupStage::CatalogLoad);
        ++encoded.version;
        QVERIFY(!startup::decodeStartupMessage(&encoded, sizeof(encoded)).has_value());
        encoded = startup::encodeStartupMessage(startup::StartupStage::CatalogLoad);
        encoded.stage = 0xFFFFFFFFU;
        QVERIFY(!startup::decodeStartupMessage(&encoded, sizeof(encoded)).has_value());
    }
};

QTEST_GUILESS_MAIN(StartupStageTest)

#include "StartupStageTest.moc"
