#include "startup/StartupStage.h"

namespace tweakopedia::startup {

bool isKnownStartupStage(StartupStage stage) noexcept
{
    const auto value = static_cast<std::uint32_t>(stage);
    return value >= static_cast<std::uint32_t>(StartupStage::ContainerIntegrity)
        && value <= static_cast<std::uint32_t>(StartupStage::Ready);
}

std::wstring_view startupStageText(StartupStage stage) noexcept
{
    switch (stage) {
    case StartupStage::ContainerIntegrity: return L"Проверка целостности приложения";
    case StartupStage::LocalData: return L"Подготовка локальных данных";
    case StartupStage::RuntimeCache: return L"Проверка локального кэша";
    case StartupStage::RuntimeExtract: return L"Извлечение встроенных компонентов";
    case StartupStage::AppLaunch: return L"Запуск компонентов приложения";
    case StartupStage::DataStore: return L"Открытие локальных данных";
    case StartupStage::CatalogLoad: return L"Загрузка каталога твиков";
    case StartupStage::CatalogValidate: return L"Проверка структуры каталога";
    case StartupStage::WindowsProfile: return L"Определение версии Windows";
    case StartupStage::StateDetection: return L"Определение текущих значений параметров";
    case StartupStage::Models: return L"Подготовка разделов приложения";
    case StartupStage::Interface: return L"Создание интерфейса";
    case StartupStage::Ready: return {};
    }
    return {};
}

std::vector<StartupStage> startupPipeline(bool includeRuntimeExtraction)
{
    std::vector result{
        StartupStage::ContainerIntegrity,
        StartupStage::LocalData,
        StartupStage::RuntimeCache,
        StartupStage::AppLaunch,
        StartupStage::DataStore,
        StartupStage::CatalogLoad,
        StartupStage::CatalogValidate,
        StartupStage::WindowsProfile,
        StartupStage::StateDetection,
        StartupStage::Models,
        StartupStage::Interface,
    };
    if (includeRuntimeExtraction) {
        result.insert(result.begin() + 3, StartupStage::RuntimeExtract);
    }
    return result;
}

} // namespace tweakopedia::startup
