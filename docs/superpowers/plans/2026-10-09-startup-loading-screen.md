# Startup Loading Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Показывать отзывчивый Fluent-экран загрузки сразу после запуска portable EXE и выводить на нём только фактически выполняемые этапы запуска.

**Architecture:** Нативный Win32 bootstrap создаёт и анимирует единственное стартовое окно до любых длительных операций. Bootstrap и дочерний Qt-процесс используют общий тип `StartupStage`; дочерний процесс передаёт этапы и сигнал готовности через `WM_COPYDATA`, после чего основное QML-окно занимает ту же геометрию и заменяет стартовое без визуального разрыва.

**Tech Stack:** C++20, Win32, WIC/GDI, Qt 6.8.3, QML, CMake 3.30, Qt Test/CTest, MinGW 13.1.

**Spec:** `docs/superpowers/specs/2026-10-09-startup-loading-screen-design.md`

## Global Constraints

- Поддерживаются Windows 10/11 x64; ARM64 не добавляется.
- Поставка остаётся одним portable EXE с локальным версионированным runtime-кэшем.
- Окно создаётся до `inspectContainer()` и не зависит от загруженного Qt-runtime.
- Экран соответствует утверждённому варианту 1: крупный логотип, `Загрузка...`, круговой неопределённый индикатор и максимум три реальные операции.
- Проценты и искусственные задержки не используются.
- Логотип берётся из `assets/branding/tweakopedia-logo.png`, текст — системный `Segoe UI` с DPI-масштабированием.
- Сбор WMI/DXGI для страницы «Обзор» остаётся асинхронным и не удерживает стартовый экран.
- `--self-check` остаётся неинтерактивным и не создаёт стартовое окно.

## Review Focus

- Повреждённый контейнер: окно уже показано, затем закрывается перед существующим системным сообщением об ошибке — Task 4.
- Первый запуск без runtime-кэша: появляется этап извлечения, а индикатор продолжает анимацию — Tasks 2 и 4.
- Повторный запуск с пригодным кэшем: этап извлечения отсутствует, остальные этапы сохраняют порядок — Tasks 2 и 3.
- Поддельный или устаревший IPC-пакет: неизвестный идентификатор игнорируется, окно не закрывается — Tasks 1 и 5.
- Масштабы 100/125/150/200% и несколько мониторов: окно и его замена QML сохраняют размер и положение без скачка — Tasks 4, 5 и 6.

---

## Карта файлов

- `src/startup/StartupStage.*` — общий независимый от Qt контракт этапов и русские подписи.
- `src/startup/StartupIpc.*` — формат допустимых межпроцессных сообщений.
- `apps/bootstrap/StartupWindow.*` — нативное окно, рендеринг, DPI и анимация.
- `apps/bootstrap/BootstrapApplication.*` — фактическая последовательность bootstrap-этапов.
- `apps/tweakopedia/BootstrapProgressClient.*` — отправка этапов и готовности в bootstrap.
- `src/package/RuntimeCache.*` — события проверки и извлечения runtime.
- `src/app/AppController.*` — уведомления на границах существующих операций запуска.
- `apps/tweakopedia/main.cpp` — связывание статусов, геометрии и первого QML-кадра.
- `apps/tweakopedia/qml/Main.qml` — показ окна только после настройки геометрии из C++.

### Task 1: Общий контракт этапов и IPC

**Files:**
- Create: `src/startup/CMakeLists.txt`
- Create: `src/startup/StartupStage.h`
- Create: `src/startup/StartupStage.cpp`
- Create: `src/startup/StartupIpc.h`
- Create: `src/startup/StartupIpc.cpp`
- Create: `tests/startup/StartupStageTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class StartupStage : std::uint32_t` со значениями из спецификации и `Ready`.
- Produces: `std::wstring_view startupStageText(StartupStage) noexcept`.
- Produces: `std::vector<StartupStage> startupPipeline(bool includeRuntimeExtraction)`.
- Produces: `StartupIpcMessage encodeStartupMessage(StartupStage)` и `std::optional<StartupStage> decodeStartupMessage(const void*, std::size_t)` с magic/version.

- [ ] **Step 1: Написать падающий `StartupStageTest`**

Проверить точные русские подписи всех этапов, наличие `runtime_extract` только при `includeRuntimeExtraction=true`, порядок общего pipeline и отклонение IPC с неверными magic, version, size либо неизвестным stage.

- [ ] **Step 2: Запустить тест до реализации**

Run: `cmake --preset mingw-debug && cmake --build --preset mingw-debug --target StartupStageTest && ctest --test-dir build/mingw-debug -R "^StartupStage$" --output-on-failure`

Expected: FAIL при конфигурации или сборке из-за отсутствующих типов.

- [ ] **Step 3: Реализовать статическую библиотеку `TweakopediaStartup`**

Контракт не использует Qt и Win32-типы. IPC содержит только фиксированные целые поля; `decodeStartupMessage()` возвращает значение только для полного известного пакета.

- [ ] **Step 4: Запустить тест после реализации**

Run: `cmake --build --preset mingw-debug --target StartupStageTest && ctest --test-dir build/mingw-debug -R "^StartupStage$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add CMakeLists.txt src/startup tests/CMakeLists.txt tests/startup/StartupStageTest.cpp
git commit -m "feat: define startup progress protocol"
```

### Task 2: Фактические события runtime-кэша

**Files:**
- Modify: `src/package/RuntimeCache.h`
- Modify: `src/package/RuntimeCache.cpp`
- Modify: `tests/package/RuntimeCacheTest.cpp`

**Interfaces:**
- Produces: `enum class RuntimeCacheEvent { ValidationStarted, ExtractionStarted }`.
- Produces: `using RuntimeCacheProgress = std::function<void(RuntimeCacheEvent)>`.
- Produces: `RuntimeCache::prepare(path, packageInfo, RuntimeCacheProgress progress = {})`.

- [ ] **Step 1: Добавить падающие тесты событий кэша**

В существующем `RuntimeCacheTest` проверить:

- первый запуск сообщает `ValidationStarted`, затем `ExtractionStarted`;
- повторный запуск сообщает только `ValidationStarted`;
- ошибка до распаковки не сообщает `ExtractionStarted`, а ошибка распаковки не
  добавляет посторонних событий.

- [ ] **Step 2: Запустить `RuntimeCacheTest` до реализации**

Run: `cmake --build --preset mingw-debug --target RuntimeCacheTest && ctest --test-dir build/mingw-debug -R "^RuntimeCache$" --output-on-failure`

Expected: FAIL из-за отсутствующего callback.

- [ ] **Step 3: Добавить callback в `RuntimeCache::prepare()`**

Вызывать `ValidationStarted` непосредственно перед `validateRuntime()`, а `ExtractionStarted` непосредственно перед `ZipPayload::extract()`. Не сообщать извлечение для пригодного кэша.

- [ ] **Step 4: Запустить тест после реализации**

Run: `cmake --build --preset mingw-debug --target RuntimeCacheTest && ctest --test-dir build/mingw-debug -R "^RuntimeCache$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/package/RuntimeCache.h src/package/RuntimeCache.cpp tests/package/RuntimeCacheTest.cpp
git commit -m "feat: report runtime cache preparation stages"
```

### Task 3: Этапы BootstrapApplication

**Files:**
- Modify: `apps/bootstrap/BootstrapApplication.h`
- Modify: `apps/bootstrap/BootstrapApplication.cpp`
- Modify: `tests/package/BootstrapApplicationTest.cpp`

**Interfaces:**
- Consumes: `StartupStage`, `RuntimeCacheProgress` из Tasks 1–2.
- Produces: `using StartupProgress = std::function<void(startup::StartupStage)>`.
- Produces: `BootstrapApplication::run(self, arguments, StartupProgress, std::optional<std::uintptr_t> startupWindowHandle = std::nullopt)`.

- [ ] **Step 1: Добавить тест точного порядка bootstrap-этапов**

Расширить fake platform и проверить, что `ContainerIntegrity` отправляется до чтения контейнера, затем `LocalData`, `RuntimeCache`, условный `RuntimeExtract` и `AppLaunch`. При переданном handle дочерние аргументы содержат `--startup-window` и десятичное значение; без handle аргумента нет.

- [ ] **Step 2: Запустить `BootstrapApplicationTest` до реализации**

Run: `cmake --build --preset mingw-debug --target BootstrapApplicationTest && ctest --test-dir build/mingw-debug -R "^BootstrapApplication$" --output-on-failure`

Expected: FAIL по новым ожиданиям.

- [ ] **Step 3: Добавить отправку реальных этапов**

Сопоставить `RuntimeCacheEvent::ValidationStarted` с `RuntimeCache`, а `ExtractionStarted` с `RuntimeExtract`. Не менять существующие коды ошибок и очистку старого кэша.

- [ ] **Step 4: Запустить тест после реализации**

Run: `cmake --build --preset mingw-debug --target BootstrapApplicationTest && ctest --test-dir build/mingw-debug -R "^BootstrapApplication$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add apps/bootstrap/BootstrapApplication.* tests/package/BootstrapApplicationTest.cpp
git commit -m "feat: expose real bootstrap startup stages"
```

### Task 4: Нативное Fluent-окно bootstrap

**Files:**
- Create: `apps/bootstrap/StartupWindow.h`
- Create: `apps/bootstrap/StartupWindow.cpp`
- Create: `tests/package/StartupWindowModelTest.cpp`
- Modify: `apps/bootstrap/main.cpp`
- Modify: `apps/bootstrap/CMakeLists.txt`
- Modify: `assets/branding/tweakopedia.rc`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `StartupStage`, `startupStageText()`, `decodeStartupMessage()`.
- Produces: `StartupWindow::create(HINSTANCE)`, `report(StartupStage)`, `nativeHandle()`, `hide()`, `messageLoopUntil(HANDLE worker)`.
- Produces: чистую модель `StartupWindowModel::advance(StartupStage)` и `visibleStages()` для тестирования трёх строк.

- [ ] **Step 1: Написать падающий тест модели окна**

Проверить максимум три строки, текущий этап в первой строке, корректное исключение условного `RuntimeExtract`, игнорирование неизвестного IPC и сохранение окна открытым до `Ready`.

- [ ] **Step 2: Запустить тест до реализации**

Run: `cmake --build --preset mingw-debug --target StartupWindowModelTest && ctest --test-dir build/mingw-debug -R "^StartupWindowModel$" --output-on-failure`

Expected: FAIL из-за отсутствующих файлов.

- [ ] **Step 3: Реализовать Win32-окно и worker bootstrap**

Создать окно перед `BootstrapApplication::run()`. Рисовать фон, PNG-логотип из `RCDATA`, `Загрузка...`, вращающийся индикатор и строки через системные WIC/GDI. Применить per-monitor DPI, caption color и минимальный размер. Запустить `BootstrapApplication::run()` в рабочем потоке; главный поток обслуживает таймер, `WM_COPYDATA` и завершение worker.

Для `--self-check` выполнить прежний синхронный путь без окна. Перед показом системной ошибки скрыть splash.

- [ ] **Step 4: Запустить тесты bootstrap**

Run: `cmake --build --preset mingw-debug --target StartupWindowModelTest BootstrapApplicationTest TweakopediaBootstrap && ctest --test-dir build/mingw-debug -R "^(StartupWindowModel|BootstrapApplication|BuildSmoke)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add apps/bootstrap assets/branding/tweakopedia.rc tests/CMakeLists.txt tests/package/StartupWindowModelTest.cpp
git commit -m "feat: show native startup window immediately"
```

### Task 5: Реальные этапы основного приложения

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/app/AppControllerTest.cpp`

**Interfaces:**
- Consumes: `StartupStage` из Task 1.
- Produces: `using StartupProgress = std::function<void(startup::StartupStage)>`.
- Produces: `bool AppController::startup(const StartupProgress& progress = {})`.

- [ ] **Step 1: Добавить тест порядка этапов AppController**

Fake services записывает вызовы. Тест требует порядок `CatalogLoad`, `CatalogValidate`, `WindowsProfile`, `StateDetection`, `Models`; `systemOverview()` не входит в синхронный порядок и запускается существующим watcher после моделей. При ошибке каталога дальнейшие этапы не отправляются.

- [ ] **Step 2: Запустить `AppControllerTest` до реализации**

Run: `cmake --build --preset mingw-debug --target AppControllerTest && ctest --test-dir build/mingw-debug -R "^AppController$" --output-on-failure`

Expected: FAIL по новым ожиданиям.

- [ ] **Step 3: Вставить callback перед существующими операциями**

Не менять алгоритмы загрузки, detection и построения моделей. Отправлять этап непосредственно перед соответствующим вызовом; этап `Models` охватывает модели твиков, категорий, групп, истории и Твикопедии.

- [ ] **Step 4: Запустить тест после реализации**

Run: `cmake --build --preset mingw-debug --target AppControllerTest && ctest --test-dir build/mingw-debug -R "^AppController$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/app/AppController.* tests/app/AppControllerTest.cpp
git commit -m "feat: report application initialization stages"
```

### Task 6: IPC, геометрия и бесшовная передача QML

**Files:**
- Create: `apps/tweakopedia/BootstrapProgressClient.h`
- Create: `apps/tweakopedia/BootstrapProgressClient.cpp`
- Create: `tests/app/BootstrapProgressClientTest.cpp`
- Modify: `apps/tweakopedia/main.cpp`
- Modify: `apps/tweakopedia/CMakeLists.txt`
- Modify: `apps/tweakopedia/qml/Main.qml`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `StartupStage`, `encodeStartupMessage()`.
- Produces: `BootstrapProgressClient::fromArgument(QStringView)`, `report(StartupStage)`, `splashGeometry()`, `ready()`.

- [ ] **Step 1: Написать падающий тест клиента**

Проверить пустой/нечисловой/нулевой/устаревший HWND, точное кодирование известных этапов, отказ отправлять неизвестный этап и получение геометрии только для существующего окна. Win32-вызовы завернуть в внедряемый `IBootstrapWindowChannel` для unit-теста.

- [ ] **Step 2: Запустить тест до реализации**

Run: `cmake --build --preset mingw-debug --target BootstrapProgressClientTest && ctest --test-dir build/mingw-debug -R "^BootstrapProgressClient$" --output-on-failure`

Expected: FAIL из-за отсутствующих типов.

- [ ] **Step 3: Связать main, AppController и первое отображение QML**

В `main.cpp` сообщить `DataStore` перед созданием `DesktopServices`, передать callback в `controller.startup()`, сообщить `Interface` перед `engine.load()`. `Main.qml` создаётся невидимым; C++ применяет прямоугольник splash, показывает окно и вызывает `ready()` после первого `frameSwapped`. Без `--startup-window` приложение показывается обычным способом. При `--self-check` IPC и окно отключены.

- [ ] **Step 4: Запустить целевые тесты**

Run: `cmake --build --preset mingw-debug --target BootstrapProgressClientTest AppControllerTest BuildSmokeTest && ctest --test-dir build/mingw-debug -R "^(BootstrapProgressClient|AppController|BuildSmoke)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add apps/tweakopedia src/app tests/app/BootstrapProgressClientTest.cpp tests/CMakeLists.txt
git commit -m "feat: hand startup progress to the Qt application"
```

### Task 7: Визуальная проверка и portable-сборка

**Files:**
- Create: `tests/visual/StartupWindowCapture.ps1`
- Modify: `tools/capture-fluent-ui.ps1`
- Modify: `README.md`
- Modify: `docs/superpowers/specs/2026-10-09-startup-loading-screen-design.md` only if implementation details differ materially

**Interfaces:**
- Consumes: завершённый native/Qt startup flow.
- Produces: воспроизводимые снимки стартового окна на основных DPI и проверенный single-EXE.

- [ ] **Step 1: Добавить управляемый capture-режим**

Добавить только тестовый аргумент, который останавливает pipeline на выбранном реальном этапе без включения его в обычный запуск. Скрипт снимает окно на 100%, 125%, 150% и 200% либо явно сообщает, какие масштабы недоступны в текущем сеансе.

- [ ] **Step 2: Собрать и проверить визуальные снимки**

Run: `pwsh .\tools\capture-fluent-ui.ps1 -Scenario startup-loading -OutputRoot D:\ChatGPT\Projects\Tweakopedia\artifacts\fluent-ui\startup-loading`

Expected: логотип, центральный блок и три строки не обрезаны; геометрия стабильна; индикатор и текст читаются на каждом доступном DPI.

- [ ] **Step 3: Запустить полный набор тестов**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug`

Expected: 100% tests passed.

- [ ] **Step 4: Собрать и проверить единый EXE**

Run: `pwsh .\tools\package.ps1 -Preset mingw-release`

Expected: `PASS: single EXE package created at D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`.

Проверить холодный запуск после очистки только каталога runtime Tweakopedia и повторный запуск с уже подготовленным кэшем. На холодном запуске должен появиться этап извлечения; на повторном — нет.

- [ ] **Step 5: Обновить README и закоммитить**

```powershell
git add README.md tools/capture-fluent-ui.ps1 tests/visual/StartupWindowCapture.ps1 docs/superpowers/specs/2026-10-09-startup-loading-screen-design.md
git commit -m "docs: document immediate startup feedback"
```

### Task 8: Финальная проверка ветки

**Files:**
- Verify only; исправления оформляются отдельным коммитом.

**Interfaces:**
- Consumes: все предыдущие задачи.
- Produces: чистая ветка с проверенным portable EXE.

- [ ] **Step 1: Проверить diff и историю**

Run: `git diff --check origin/main...HEAD; git status --short; git log --oneline origin/main..HEAD`

Expected: нет ошибок whitespace, рабочее дерево чистое, каждый самостоятельный этап имеет отдельный коммит.

- [ ] **Step 2: Повторить критические сценарии**

Проверить: повреждённый контейнер, холодный кэш, тёплый кэш, неверный IPC, `--self-check`, запуск без bootstrap из build-каталога и закрытие приложения во время старта.

- [ ] **Step 3: Зафиксировать итоговые данные**

Сохранить количество CTest-сценариев, размер и SHA-256 финального EXE, а также пути к визуальным снимкам в итоговом отчёте пользователю.
