# Tweakopedia First Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Собрать первый сквозной portable-срез Tweakopedia: один реальный YAML-твик реестра проходит путь от загрузки и определения состояния до очереди, предварительного плана, повышенного Executor, снимка, применения, проверки, истории и полного отката.

**Architecture:** Обычный Qt Quick-процесс отвечает за контент, чтение состояния, очередь и интерфейс. Отдельный `Tweakopedia.Executor.exe` получает только типизированный план через локальный IPC с одноразовым nonce и SHA-256 канонического тела, повторно сверяет исходное состояние, создаёт снимок и выполняет операции. Доменные и исполнительные компоненты не зависят от QML и тестируются через фиктивный backend.

**Tech Stack:** C++20, Qt 6.8.3 (Core, Gui, Qml, Quick, QuickControls2, Sql, Test, Network), MinGW 13.1.0 x64, CMake 3.30.5, Ninja 1.12.1, yaml-cpp 0.8.0, SQLite, WinAPI.

**Spec:** [Утверждённая архитектурная спецификация](../specs/2026-10-08-tweakopedia-design.md)

## Global Constraints

- Поддерживаются только Windows 10/11 x64. ARM64 и x86 не входят в проект.
- Поставка только portable; установщик и служба не создаются.
- Все инструменты, загрузки, сборки, кэши и временные файлы находятся под `D:\ChatGPT`.
- Скрипты задают `TEMP`, `TMP`, кэши CMake и FetchContent только для собственного процесса.
- Основное приложение всегда запускается без повышения прав; повышение запрашивается только для Executor.
- Контент не содержит произвольных CMD/PowerShell-команд, поля версии, библиографии и ссылки на отдельную статью.
- Приложение не делает сетевых запросов во время работы.
- Любое изменение проходит через очередь, предварительный план и снимок фактического состояния.
- Все пользовательские строки первого среза пишутся по-русски.
- Первый реальный твик: `filesystem.win32-long-paths`, параметр `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled` в 64-битном представлении реестра.
- Состояния твика: `enabled` = DWORD `1`; `disabled` = DWORD `0` или отсутствующее значение. Снимок обязан различать `0` и отсутствие значения.
- Автоматические тесты не меняют рабочий параметр Windows: используются фиктивный backend и отдельная тестовая ветвь HKCU. Реальный HKLM-параметр затрагивается только явной ручной сквозной проверкой с немедленным откатом.
- Каждый шаг реализации следует циклу: падающий тест → минимальная реализация → проходящий тест → отдельный коммит.

## Review Focus

Проверяющий должен отдельно искать следующие классы дефектов:

1. Некорректный YAML, неизвестные поля и дубли ID принимаются без понятной ошибки.
2. Отсутствующее значение реестра смешивается с DWORD `0`, из-за чего откат создаёт значение вместо удаления.
3. Изменение системы между предварительным анализом и запуском Executor не обнаруживается.
4. Отмена UAC, разрыв IPC или аварийное завершение оставляют транзакцию в ложном состоянии `succeeded`.
5. Главное приложение либо контент могут передать Executor произвольную команду вместо разрешённой типизированной операции.

---

## Task 1: Воспроизводимая инструментальная среда и каркас CMake

**Files:**

- Create: `tools/bootstrap-toolchain.ps1`
- Create: `tools/enter-build-env.ps1`
- Create: `tools/verify-toolchain.ps1`
- Create: `tools/toolchain-lock.json`
- Create: `tools/build.ps1`
- Create: `tools/test.ps1`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `cmake/Dependencies.cmake`
- Create: `.gitignore`
- Create: `tests/CMakeLists.txt`
- Create: `tests/smoke/BuildSmokeTest.cpp`

- [ ] **Step 1: Написать проверку отсутствующей среды**

  `verify-toolchain.ps1` проверяет конкретные пути Qt 6.8.3, MinGW 13.1, CMake и Ninja под `D:\ChatGPT\Tools\Tweakopedia`; при отсутствии любого компонента возвращает ненулевой код.

- [ ] **Step 2: Запустить проверку и зафиксировать ожидаемое падение**

  Run: `pwsh -NoProfile -File tools/verify-toolchain.ps1`

  Expected: `FAIL` с перечнем отсутствующих компонентов.

- [ ] **Step 3: Реализовать bootstrap без глобальных изменений Windows**

  Скрипт создаёт Python venv под `D:\ChatGPT\Tools\Tweakopedia\bootstrap`, устанавливает `aqtinstall==3.3.0` и выполняет эквивалент следующих команд:

  ```powershell
  python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O D:\ChatGPT\Tools\Tweakopedia\Qt
  python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O D:\ChatGPT\Tools\Tweakopedia\Qt
  python -m aqt install-tool windows desktop tools_cmake qt.tools.cmake -O D:\ChatGPT\Tools\Tweakopedia\Qt
  python -m aqt install-tool windows desktop tools_ninja qt.tools.ninja -O D:\ChatGPT\Tools\Tweakopedia\Qt
  ```

  `toolchain-lock.json` фиксирует Qt 6.8.3, MinGW 13.1.0, CMake 3.30.5, Ninja 1.12.1, варианты пакетов и ожидаемые хеши архивов. Все архивы и временные данные направляются в `D:\ChatGPT\Cache\Tweakopedia` и `D:\ChatGPT\Temp\Tweakopedia`.

- [ ] **Step 4: Добавить процесс-локальное окружение сборки**

  `enter-build-env.ps1` добавляет конкретные каталоги Qt/MinGW/CMake/Ninja в `PATH`, задаёт `TEMP`, `TMP`, `CMAKE_GENERATOR=Ninja` и `FETCHCONTENT_BASE_DIR=D:\ChatGPT\Cache\Tweakopedia\fetchcontent`, не меняя системные переменные.

- [ ] **Step 5: Добавить минимальный проект и падающий smoke-тест конфигурации**

  Корневой CMake включает C++20, `AUTOMOC/AUTORCC/AUTOUIC`, Qt Test и pinned `yaml-cpp` через `FetchContent`. До bootstrap команда конфигурации должна завершаться ошибкой отсутствия Qt.

- [ ] **Step 6: Установить среду и выполнить smoke-тест**

  Run: `pwsh -NoProfile -File tools/bootstrap-toolchain.ps1`

  Run: `pwsh -NoProfile -File tools/build.ps1 -Preset mingw-debug`

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex BuildSmoke`

  Expected: один пройденный тест.

- [ ] **Step 7: Commit**

  ```powershell
  git add .gitignore CMakeLists.txt CMakePresets.json cmake tools tests
  git commit -m "build: bootstrap Qt toolchain and CMake project"
  ```

## Task 2: Доменные типы одного декларативного твика

**Files:**

- Create: `src/domain/CMakeLists.txt`
- Create: `src/domain/TweakId.h`
- Create: `src/domain/TweakDefinition.h`
- Create: `src/domain/TweakDefinition.cpp`
- Create: `src/domain/RegistryTypes.h`
- Create: `src/domain/SystemProfile.h`
- Create: `src/domain/DetectedState.h`
- Create: `tests/domain/TweakDefinitionTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты инвариантов**

  Проверить: допустимый ID `filesystem.win32-long-paths`; запрет пустого ID, пробелов, заглавных букв и повторяющихся точек; допустимость только объявленного target state; обязательность русского заголовка и всех секций объяснения.

- [ ] **Step 2: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex DomainTweakDefinition`

  Expected: сборка либо тест падает из-за отсутствующих типов.

- [ ] **Step 3: Реализовать минимальную модель**

  Добавить:

  - `TweakId::parse(QStringView) -> std::optional<TweakId>`;
  - `enum class TweakKind { Setting, Action, Diagnostic }`;
  - `enum class Impact { Low, Medium, High, Critical }`;
  - `enum class Reversibility { Reversible, Conditional, Irreversible }`;
  - `enum class RestartRequirement { None, Explorer, Service, SignOut, Reboot }`;
  - `RegistryLocation { hive, key, valueName, view }`;
  - `RegistryDwordDetection { location, statesByValue, missingState }`;
  - `SetRegistryDwordOperation { location, value }`;
  - `TweakStateDefinition { id, title, operations }`;
  - `TweakDefinition` с описанием, совместимостью, состояниями и штатными значениями.

- [ ] **Step 4: Запустить тесты и проверить проход**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex DomainTweakDefinition`

  Expected: PASS.

- [ ] **Step 5: Commit**

  ```powershell
  git add src/domain tests/domain tests/CMakeLists.txt
  git commit -m "feat: add declarative tweak domain model"
  ```

## Task 3: Строгий YAML-загрузчик и первый реальный твик

**Files:**

- Create: `src/content/CMakeLists.txt`
- Create: `src/content/CatalogError.h`
- Create: `src/content/TweakCatalog.h`
- Create: `src/content/TweakCatalog.cpp`
- Create: `src/content/TweakCatalogLoader.h`
- Create: `src/content/TweakCatalogLoader.cpp`
- Create: `content/categories.yaml`
- Create: `content/tweaks/filesystem/win32-long-paths.yaml`
- Create: `tests/content/TweakCatalogLoaderTest.cpp`
- Create: `tests/fixtures/content/valid/win32-long-paths.yaml`
- Create: `tests/fixtures/content/invalid/duplicate-id-a.yaml`
- Create: `tests/fixtures/content/invalid/duplicate-id-b.yaml`
- Create: `tests/fixtures/content/invalid/unknown-operation.yaml`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты загрузчика**

  Покрыть: корректный файл; отсутствующее обязательное поле; неизвестное поле; неверный тип DWORD; неизвестное состояние; дубли ID между файлами; попытка указать shell-команду.

- [ ] **Step 2: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex TweakCatalogLoader`

  Expected: FAIL.

- [ ] **Step 3: Реализовать строгий загрузчик**

  Публичный контракт:

  ```cpp
  struct CatalogLoadResult {
      std::optional<TweakCatalog> catalog;
      QVector<CatalogError> errors;
  };

  class TweakCatalogLoader {
  public:
      CatalogLoadResult loadDirectory(const QString& directory) const;
  };
  ```

  Загрузчик принимает только известные ключи, нормализует пути реестра, собирает все ошибки с путём файла и YAML-позицией и не возвращает частично пригодный каталог.

- [ ] **Step 4: Добавить реальное определение `filesystem.win32-long-paths`**

  YAML содержит встроенное русское объяснение, Win10/11 x64, `min_build: 14393`, 64-битный HKLM DWORD, состояния `enabled`/`disabled`, `missing_state: disabled`, `restart: none`, `impact: low`, `reversibility: reversible` и штатное состояние `disabled`.

- [ ] **Step 5: Запустить модульные тесты и проверку реального каталога**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "TweakCatalogLoader|ContentCatalog"`

  Expected: PASS и один загруженный реальный твик.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/content content tests/content tests/fixtures tests/CMakeLists.txt
  git commit -m "feat: load strict YAML tweak catalog"
  ```

## Task 4: Профиль Windows и оценка совместимости

**Files:**

- Create: `src/platform/CMakeLists.txt`
- Create: `src/platform/ISystemProfileProvider.h`
- Create: `src/platform/WindowsSystemProfileProvider.h`
- Create: `src/platform/WindowsSystemProfileProvider.cpp`
- Create: `src/detection/CMakeLists.txt`
- Create: `src/detection/CompatibilityEvaluator.h`
- Create: `src/detection/CompatibilityEvaluator.cpp`
- Create: `tests/detection/CompatibilityEvaluatorTest.cpp`
- Create: `tests/platform/WindowsSystemProfileProviderTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты матрицы совместимости**

  Проверить Windows 10 build 19045 x64, Windows 11 build 26200 x64, слишком старую сборку, ARM64 и неизвестную семью ОС. Неподдерживаемые варианты должны возвращать код причины, а не `false` без пояснения.

- [ ] **Step 2: Написать Windows-интеграционный тест профиля**

  Проверить, что провайдер получает build/UBR из реестра и версию через `RtlGetVersion`, не полагаясь на manifest version lie.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "CompatibilityEvaluator|WindowsSystemProfile"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать профиль и evaluator**

  Контракты:

  ```cpp
  class ISystemProfileProvider {
  public:
      virtual ~ISystemProfileProvider() = default;
      virtual SystemProfile current() const = 0;
  };

  CompatibilityResult evaluate(
      const TweakDefinition& tweak,
      const SystemProfile& profile);
  ```

  `CompatibilityResult` содержит `supported`, машинный `reasonCode` и русское пояснение.

- [ ] **Step 5: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "CompatibilityEvaluator|WindowsSystemProfile"`

  Expected: PASS на текущей Windows x64.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/platform src/detection tests/platform tests/detection tests/CMakeLists.txt
  git commit -m "feat: detect Windows profile and tweak compatibility"
  ```

## Task 5: Абстракция реестра и определение текущего состояния

**Files:**

- Create: `src/platform/IRegistryBackend.h`
- Create: `src/platform/WindowsRegistryBackend.h`
- Create: `src/platform/WindowsRegistryBackend.cpp`
- Create: `tests/fakes/FakeRegistryBackend.h`
- Create: `tests/fakes/FakeRegistryBackend.cpp`
- Create: `src/detection/RegistryDwordStateDetector.h`
- Create: `src/detection/RegistryDwordStateDetector.cpp`
- Create: `tests/detection/RegistryDwordStateDetectorTest.cpp`
- Create: `tests/platform/WindowsRegistryBackendTest.cpp`
- Modify: `src/platform/CMakeLists.txt`
- Modify: `src/detection/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты чтения и состояния**

  На fake backend проверить DWORD `1`, DWORD `0`, отсутствие значения, неверный тип, отказ чтения и неизвестное число. Ожидания: `enabled`, `disabled`, `disabled`, `unknown`, `unknown`, `custom` соответственно.

- [ ] **Step 2: Написать изолированный Windows-тест backend**

  Использовать только `HKCU\Software\Tweakopedia\Tests\<uuid>`; проверить различение отсутствия, DWORD и строкового типа, а также 64-битное представление.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "RegistryDwordStateDetector|WindowsRegistryBackend"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать backend и detector**

  Контракт backend:

  ```cpp
  struct RegistryReadResult {
      RegistryPresence presence;
      RegistryValueType type;
      QByteArray rawValue;
      std::error_code error;
  };

  class IRegistryBackend {
  public:
      virtual RegistryReadResult read(const RegistryLocation&) const = 0;
      virtual RegistryWriteResult writeDword(const RegistryLocation&, quint32) = 0;
      virtual RegistryWriteResult deleteValue(const RegistryLocation&) = 0;
  };
  ```

  Windows-реализация использует `RegOpenKeyExW`/`RegQueryValueExW`, явно применяет `KEY_WOW64_64KEY` и не использует `QSettings`.

- [ ] **Step 5: Запустить тесты и проверить очистку тестовой ветви**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "RegistryDwordStateDetector|WindowsRegistryBackend"`

  Expected: PASS; тестовая ветвь HKCU удалена самим fixture teardown.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/platform src/detection tests/fakes tests/platform tests/detection tests/CMakeLists.txt
  git commit -m "feat: detect registry DWORD tweak state"
  ```

## Task 6: Очередь и детерминированный предварительный план

**Files:**

- Create: `src/planning/CMakeLists.txt`
- Create: `src/planning/TweakQueue.h`
- Create: `src/planning/TweakQueue.cpp`
- Create: `src/planning/ExecutionPlan.h`
- Create: `src/planning/ExecutionPlan.cpp`
- Create: `src/planning/PlanBuilder.h`
- Create: `src/planning/PlanBuilder.cpp`
- Create: `tests/planning/TweakQueueTest.cpp`
- Create: `tests/planning/PlanBuilderTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты очереди**

  Проверить замену target state для одного ID, удаление пункта, запрет неизвестного состояния и неизменность системы при работе с очередью.

- [ ] **Step 2: Написать падающие тесты PlanBuilder**

  Проверить no-op при совпадении текущего и выбранного состояния; блокировку unsupported/unknown; одну типизированную запись DWORD; стабильный порядок; русское резюме; fingerprint исходного значения.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "TweakQueue|PlanBuilder"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать очередь и план**

  Основной контракт:

  ```cpp
  PlanBuildResult PlanBuilder::build(
      const TweakCatalog& catalog,
      const TweakQueue& queue,
      const QHash<TweakId, DetectedState>& detected,
      const SystemProfile& profile) const;
  ```

  `ExecutionPlan` содержит schema version протокола, UUID транзакции, профиль ОС, время создания, типизированные операции, ожидаемые before-fingerprint и требования перезапуска. Это версия протокола, не версия твика.

- [ ] **Step 5: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "TweakQueue|PlanBuilder"`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/planning tests/planning tests/CMakeLists.txt
  git commit -m "feat: build typed execution plans from queue"
  ```

## Task 7: Постоянные данные, SQLite и файловый журнал транзакции

**Files:**

- Create: `src/persistence/CMakeLists.txt`
- Create: `src/persistence/AppPaths.h`
- Create: `src/persistence/AppPaths.cpp`
- Create: `src/persistence/Database.h`
- Create: `src/persistence/Database.cpp`
- Create: `src/persistence/migrations/001_initial.sql`
- Create: `src/persistence/TransactionRecord.h`
- Create: `src/persistence/TransactionRepository.h`
- Create: `src/persistence/TransactionRepository.cpp`
- Create: `src/persistence/TransactionFiles.h`
- Create: `src/persistence/TransactionFiles.cpp`
- Create: `tests/persistence/AppPathsTest.cpp`
- Create: `tests/persistence/TransactionRepositoryTest.cpp`
- Create: `tests/persistence/TransactionFilesTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты путей**

  Проверить неизменяемый `applicationDir/content`, пользовательский `%LocalAppData%\Tweakopedia`, возможность подмены корня в тестах и отсутствие записи рядом с EXE.

- [ ] **Step 2: Написать падающие тесты БД и файлов транзакции**

  Проверить миграцию пустой БД, состояния `pending/running/succeeded/failed/rolled_back/interrupted`, сохранение русского имени пакета и атомарную запись `plan.json`, `before.json`, `result.json`.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "AppPaths|TransactionRepository|TransactionFiles"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать хранилище**

  SQLite включается с foreign keys и транзакционными миграциями. JSON-файлы пишутся через временный файл + атомарное переименование. Каталог транзакции создаётся до запуска Executor.

- [ ] **Step 5: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "AppPaths|TransactionRepository|TransactionFiles"`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/persistence tests/persistence tests/CMakeLists.txt
  git commit -m "feat: persist transaction history and journal files"
  ```

## Task 8: Сериализация плана и строгая граница Executor

**Files:**

- Create: `src/execution/CMakeLists.txt`
- Create: `src/execution/ExecutionProtocol.h`
- Create: `src/execution/ExecutionProtocol.cpp`
- Create: `src/execution/PlanValidator.h`
- Create: `src/execution/PlanValidator.cpp`
- Create: `tests/execution/ExecutionProtocolTest.cpp`
- Create: `tests/execution/PlanValidatorTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие round-trip и negative-тесты**

  Проверить стабильный JSON, неизвестную schema version, неизвестный operation type, лишние поля, путь реестра вне разрешённых hive, подменённый fingerprint и поле `command`.

- [ ] **Step 2: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "ExecutionProtocol|PlanValidator"`

  Expected: FAIL.

- [ ] **Step 3: Реализовать протокол**

  `ExecutionProtocol` кодирует только whitelist структур. `PlanValidator` повторно проверяет OS profile, идентификатор транзакции, возраст плана, допустимые hive/view/type и SHA-256 канонического тела. Неизвестное поле является ошибкой.

- [ ] **Step 4: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "ExecutionProtocol|PlanValidator"`

  Expected: PASS.

- [ ] **Step 5: Commit**

  ```powershell
  git add src/execution tests/execution tests/CMakeLists.txt
  git commit -m "feat: define strict executor protocol"
  ```

## Task 9: Снимок, применение, проверка и откат DWORD

**Files:**

- Create: `src/execution/RegistrySnapshot.h`
- Create: `src/execution/RegistrySnapshot.cpp`
- Create: `src/execution/RegistryDwordExecutor.h`
- Create: `src/execution/RegistryDwordExecutor.cpp`
- Create: `src/execution/TransactionRunner.h`
- Create: `src/execution/TransactionRunner.cpp`
- Create: `tests/execution/RegistryDwordExecutorTest.cpp`
- Create: `tests/execution/TransactionRunnerTest.cpp`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие тесты снимка и отката**

  Покрыть исходные состояния: значение отсутствует, DWORD `0`, DWORD `1`, неверный тип. Проверить точное восстановление типа/байтов и удаление значения, если до операции оно отсутствовало.

- [ ] **Step 2: Написать падающие тесты гонки и ошибки**

  Проверить: before-fingerprint изменился; запись вернула ошибку; контрольное чтение не совпало; вторая операция упала. В последнем случае первая операция должна откатиться в обратном порядке.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "RegistryDwordExecutor|TransactionRunner"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать runner**

  Последовательность строго фиксирована: reread → compare fingerprint → persist snapshot → mark running → write → reread/verify → persist result. При ошибке runner останавливается и откатывает уже подтверждённые операции.

- [ ] **Step 5: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "RegistryDwordExecutor|TransactionRunner"`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/execution tests/execution tests/CMakeLists.txt
  git commit -m "feat: execute and roll back registry DWORD transactions"
  ```

## Task 10: Повышенный процесс и локальный IPC

**Files:**

- Create: `apps/executor/CMakeLists.txt`
- Create: `apps/executor/main.cpp`
- Create: `src/execution/ExecutorServer.h`
- Create: `src/execution/ExecutorServer.cpp`
- Create: `src/execution/ExecutorLauncher.h`
- Create: `src/execution/ExecutorLauncher.cpp`
- Create: `src/execution/ExecutorClient.h`
- Create: `src/execution/ExecutorClient.cpp`
- Create: `tests/execution/ExecutorIpcTest.cpp`
- Create: `tests/execution/ExecutorLauncherTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающий IPC-тест без повышения прав**

  Запустить Executor в `--test-mode` с fake backend; проверить handshake с одноразовым nonce, совпадение PID, передачу прогресса, финального результата и отказ второго клиента.

- [ ] **Step 2: Написать тесты отказов launcher**

  Абстрагировать `ShellExecuteExW`; проверить `runas`, отмену UAC, невозможность запуска, timeout и преждевременное завершение процесса. Ни один из этих исходов не должен маркировать транзакцию успешной.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "ExecutorIpc|ExecutorLauncher"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать launcher/client/server**

  Главное приложение создаёт локальный сервер со случайным именем и nonce, затем запускает Executor через `ShellExecuteExW(..., L\"runas\", ...)`. План передаётся только после взаимной проверки nonce и PID; командная строка не содержит системных значений и содержимого плана.

- [ ] **Step 5: Реализовать `Tweakopedia.Executor.exe`**

  Процесс подключается к IPC, валидирует план, использует `WindowsRegistryBackend`, пишет файлы транзакции и возвращает структурированный результат. Режим `--test-mode` компилируется только при `TWEAKOPEDIA_TESTING`.

- [ ] **Step 6: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "ExecutorIpc|ExecutorLauncher"`

  Expected: PASS.

- [ ] **Step 7: Commit**

  ```powershell
  git add apps/executor src/execution tests/execution CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat: run typed plans in elevated executor"
  ```

## Task 11: Application service и модели для QML

**Files:**

- Create: `src/app/CMakeLists.txt`
- Create: `src/app/AppController.h`
- Create: `src/app/AppController.cpp`
- Create: `src/app/TweakListModel.h`
- Create: `src/app/TweakListModel.cpp`
- Create: `src/app/QueueListModel.h`
- Create: `src/app/QueueListModel.cpp`
- Create: `src/app/HistoryListModel.h`
- Create: `src/app/HistoryListModel.cpp`
- Create: `tests/app/AppControllerTest.cpp`
- Create: `tests/app/TweakListModelTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие controller-тесты**

  С fake dependencies проверить startup, загрузку одного твика, определение состояния, добавление только в очередь, создание preview, обработку UAC cancel, успех транзакции, обновление состояния и откат из истории.

- [ ] **Step 2: Написать падающие model-тесты**

  Проверить роли `id/title/summary/currentState/targetState/supported/impact/restart`, стабильность строк и обновление только затронутых ролей.

- [ ] **Step 3: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "AppController|TweakListModel"`

  Expected: FAIL.

- [ ] **Step 4: Реализовать application layer**

  `AppController` оркестрирует уже готовые сервисы, но не содержит WinAPI. Публичные QML-методы: `selectTarget(id,state)`, `removeFromQueue(id)`, `openExplanation(id)`, `buildPreview()`, `applyQueue(name)`, `rollback(transactionId)`.

- [ ] **Step 5: Запустить тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "AppController|TweakListModel"`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/app tests/app tests/CMakeLists.txt
  git commit -m "feat: expose tweak workflow to QML"
  ```

## Task 12: Qt Quick-оболочка, каталог и панель объяснения

**Files:**

- Create: `apps/tweakopedia/CMakeLists.txt`
- Create: `apps/tweakopedia/main.cpp`
- Create: `apps/tweakopedia/qml/Main.qml`
- Create: `apps/tweakopedia/qml/pages/OverviewPage.qml`
- Create: `apps/tweakopedia/qml/pages/TweaksPage.qml`
- Create: `apps/tweakopedia/qml/pages/QueuePage.qml`
- Create: `apps/tweakopedia/qml/pages/HistoryPage.qml`
- Create: `apps/tweakopedia/qml/components/TweakRow.qml`
- Create: `apps/tweakopedia/qml/components/ExplanationDrawer.qml`
- Create: `apps/tweakopedia/qml/components/StatusBadge.qml`
- Create: `apps/tweakopedia/resources.qrc`
- Create: `tests/qml/tst_tweakrow.qml`
- Create: `tests/qml/tst_explanationdrawer.qml`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие QML-тесты**

  Проверить отображение названия и состояния, наличие отдельной кнопки `?`, открытие drawer, полный текст шести секций объяснения и то, что выбор target state только вызывает сигнал очереди.

- [ ] **Step 2: Запустить QML-тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex Qml`

  Expected: FAIL.

- [ ] **Step 3: Реализовать минимальную оболочку**

  Активны вкладки «Обзор», «Твики», «Очередь», «История». Остальные утверждённые вкладки видны как отключённые с пометкой будущего этапа. В строке твика показываются текущее и выбранное состояния, метки совместимости и кнопка `?`.

- [ ] **Step 4: Реализовать панель объяснения**

  Панель показывает назначение, механизм, эффект, ограничения, рекомендацию, технические детали, точный ключ реестра и способ возврата; Markdown рендерится штатными средствами Qt без внешнего WebView.

- [ ] **Step 5: Запустить QML и controller-тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "Qml|AppController"`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add apps/tweakopedia tests/qml CMakeLists.txt tests/CMakeLists.txt
  git commit -m "feat: add tweak catalog and explanation UI"
  ```

## Task 13: Предварительный итог, применение и история в интерфейсе

**Files:**

- Create: `apps/tweakopedia/qml/components/PlanPreview.qml`
- Create: `apps/tweakopedia/qml/components/ApplyProgress.qml`
- Create: `apps/tweakopedia/qml/components/TransactionDetails.qml`
- Modify: `apps/tweakopedia/qml/pages/QueuePage.qml`
- Modify: `apps/tweakopedia/qml/pages/HistoryPage.qml`
- Modify: `apps/tweakopedia/qml/pages/OverviewPage.qml`
- Create: `tests/qml/tst_planpreview.qml`
- Create: `tests/qml/tst_history.qml`
- Create: `tests/integration/VerticalSliceWorkflowTest.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающие QML-тесты сценария**

  Проверить обязательный preview до кнопки применения, имя пакета, исходное/целевое состояние, объект реестра, требования перезапуска, прогресс и финальный статус.

- [ ] **Step 2: Написать падающий сквозной тест на fake backend**

  Сценарий: YAML → detect disabled → queue enabled → preview → executor test mode → snapshot absent → apply `1` → verify → history → rollback → value absent.

- [ ] **Step 3: Добавить сценарии прерывания**

  Покрыть UAC cancel, смену DWORD после preview, обрыв IPC после snapshot и аварийный result. На следующем startup незавершённая транзакция должна отображаться на «Обзоре» как `interrupted`.

- [ ] **Step 4: Запустить тесты и подтвердить падение**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "PlanPreview|History|VerticalSliceWorkflow"`

  Expected: FAIL.

- [ ] **Step 5: Связать UI с application layer**

  Очередь сначала показывает пересчитанный preview. После подтверждения отображается UAC/IPC-прогресс. История читает SQLite и `result.json`; кнопка отката доступна только при наличии полного снимка.

- [ ] **Step 6: Запустить сквозные тесты**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex "PlanPreview|History|VerticalSliceWorkflow"`

  Expected: PASS.

- [ ] **Step 7: Commit**

  ```powershell
  git add apps/tweakopedia tests/qml tests/integration tests/CMakeLists.txt
  git commit -m "feat: complete queued apply and rollback workflow"
  ```

## Task 14: Portable-комплект и автоматическая приёмочная проверка

**Files:**

- Create: `tools/package.ps1`
- Create: `tools/verify-package.ps1`
- Create: `cmake/DeployQt.cmake`
- Create: `tests/package/PackageLayoutTest.ps1`
- Create: `docs/testing/first-vertical-slice-checklist.md`
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`

- [ ] **Step 1: Написать падающий тест portable-структуры**

  Требовать `Tweakopedia.exe`, `Tweakopedia.Executor.exe`, Qt DLL/plugins, QML modules, SQLite driver и `content/tweaks/filesystem/win32-long-paths.yaml`; запрещать абсолютные build-пути и исходники.

- [ ] **Step 2: Запустить тест до упаковки**

  Run: `pwsh -NoProfile -File tests/package/PackageLayoutTest.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia`

  Expected: FAIL, package отсутствует.

- [ ] **Step 3: Реализовать упаковку**

  Release-сборка размещается в `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia`; `windeployqt` копирует только нужные runtime-модули, затем копируется контент. Скрипт удаляет только заранее проверенный каталог `dist\Tweakopedia`, находящийся внутри репозитория.

- [ ] **Step 4: Реализовать автоматическую проверку запуска**

  `verify-package.ps1` запускает приложение с `--self-check --no-elevation`, проверяет загрузку Qt/QML/SQLite/YAML, профиль Windows и один найденный твик, затем ожидает код `0`.

- [ ] **Step 5: Собрать и проверить комплект**

  Run: `pwsh -NoProfile -File tools/package.ps1 -Preset mingw-release`

  Run: `pwsh -NoProfile -File tools/verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia`

  Expected: PASS.

- [ ] **Step 6: Добавить ручной чек-лист реального HKLM-сценария**

  Чек-лист фиксирует исходное наличие/значение `LongPathsEnabled`, применение противоположного состояния, совпадение `before.json`, успешную проверку, запись истории, откат и побайтовое совпадение с исходным состоянием. Проверка выполняется на Windows 10 x64 и Windows 11 x64 либо на отдельных VM.

- [ ] **Step 7: Commit**

  ```powershell
  git add tools cmake tests/package docs/testing CMakeLists.txt CMakePresets.json
  git commit -m "build: package and verify portable vertical slice"
  ```

## Task 15: Полная проверка среза и сверка со спецификацией

**Files:**

- Modify if needed: files implicated by verification only
- Create: `docs/testing/first-vertical-slice-results.md`

- [ ] **Step 1: Выполнить чистую сборку**

  Run: `pwsh -NoProfile -File tools/build.ps1 -Preset mingw-debug -Clean`

  Expected: configure/build PASS без предупреждений проекта.

- [ ] **Step 2: Выполнить весь автоматический набор**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug`

  Expected: все тесты PASS; skipped допустимы только для явно помеченной второй версии Windows.

- [ ] **Step 3: Проверить release portable-комплект**

  Run: `pwsh -NoProfile -File tools/package.ps1 -Preset mingw-release`

  Run: `pwsh -NoProfile -File tools/verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia`

  Expected: PASS на машине без PATH к Qt.

- [ ] **Step 4: Выполнить ручной сценарий применения и отката**

  Использовать `docs/testing/first-vertical-slice-checklist.md`; сохранить только технические итоги и хеши артефактов в `first-vertical-slice-results.md`, не копируя пользовательские значения реестра, не относящиеся к твикам.

- [ ] **Step 5: Сверить критерии раздела 23 спецификации**

  Отметить доказательство для каждого пункта: portable launch, профиль Windows, YAML, каталог, detect, `?`, очередь, preview, Executor, snapshot, apply/verify, history, rollback.

- [ ] **Step 6: Проверить рабочее дерево**

  Run: `git status --short`

  Expected: только ожидаемый файл результатов до финального коммита.

- [ ] **Step 7: Commit**

  ```powershell
  git add docs/testing/first-vertical-slice-results.md
  git commit -m "test: verify first Tweakopedia vertical slice"
  ```

## Definition of Done

- Все критерии раздела 23 утверждённой спецификации выполнены.
- Полный тестовый набор проходит из чистой сборки.
- Portable-комплект запускается без установленного Qt и без сетевых запросов.
- Реальное исходное состояние DWORD полностью восстанавливается, включая отсутствие значения.
- Отмена UAC, устаревший план и обрыв IPC дают корректный неуспешный статус.
- В Executor невозможно передать произвольную shell-команду через YAML либо JSON-план.
- История и файлы транзакции согласованы после успеха, ошибки и отката.
- Результаты сквозной проверки записаны в `docs/testing/first-vertical-slice-results.md`.
