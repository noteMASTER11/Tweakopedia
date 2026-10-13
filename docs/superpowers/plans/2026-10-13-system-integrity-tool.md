# System Integrity Tool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Заменить первую заглушку раздела «Инструменты» рабочей проверкой и восстановлением хранилища компонентов Windows через DISM с потоковым выводом, понятным итогом и отдельным журналом сеанса.

**Architecture:** QML-страница работает через долгоживущий `SystemIntegrityController`. Контроллер запускает асинхронный `SystemToolSession`, который переиспользует существующие `ExecutorServer`, `ExecutorLauncher` и проверенный локальный IPC. Повышенный `Tweakopedia.Executor.exe` принимает только типизированные операции `system_integrity.scan` и `system_integrity.restore`, сам выбирает фиксированные аргументы DISM, потоково отправляет этапы/вывод/прогресс и возвращает типизированный итог.

**Tech Stack:** C++20, Qt 6.8.3 Core/Concurrent/Gui/Network/QML/Quick, QProcess, Win32 `ShellExecuteExW`, CMake 3.30, Qt Test/Quick Test/CTest, MinGW 13.1.

**Spec:** `docs/superpowers/specs/2026-10-13-system-integrity-tool-design.md`

## Global Constraints

- Поддерживаются Windows 10/11 x64; ARM64 не добавляется.
- Поставка остаётся одним portable EXE с существующим runtime-кэшем.
- Основное приложение не запускает DISM напрямую и не принимает произвольные команды или аргументы.
- Проверка всегда выполняет `/ScanHealth /English`, затем `/CheckHealth /English`.
- Восстановление всегда выполняет `/RestoreHealth /English`, затем `/CheckHealth /English`.
- Следующий этап запускается только после нормального завершения предыдущего с кодом `0`.
- GUI-поток не ждёт завершения Executor или DISM синхронно.
- Операции инструмента не создают транзакцию твиков и не появляются в разделе «История».
- Отмена пользователем во время DISM в этой версии не реализуется.
- Экранный вывод ограничен 2 МиБ; полный поток остаётся в журнале сеанса.
- Переход на другую страницу не уничтожает контроллер, сеанс, накопленный вывод или итог.
- Системный журнал открывается только по фиксированному пути `%WINDIR%\Logs\DISM\dism.log`.
- Все команды сборки и тестов выполняются с `TEMP`/`TMP` в `D:\ChatGPT\Temp`.

## Review Focus

- Фраза состояния или процент разорваны между несколькими порциями вывода: парсер всё равно распознаёт итог, а прогресс внутри этапа не уменьшается — Tasks 1 и 3.
- Пользователь отменяет UAC до подключения IPC: экран возвращается в `idle`, показывает ненавязчивое сообщение и допускает повтор — Tasks 5 и 6.
- Executor завершается или отключается после частичного вывода: контроллер переходит в `failed`, но сохраняет вывод и путь к уже созданному журналу — Tasks 4–6.
- Пользователь уходит со страницы во время проверки и возвращается: второй процесс не запускается, состояние и терминал продолжают обновляться — Tasks 6 и 7.
- Поток превышает 2 МиБ: старые данные удаляются только из модели экрана, журнал содержит полный поток — Tasks 3 и 6.

---

## Карта файлов

- `src/execution/SystemToolProtocol.*` — типизированные операции, этапы, результаты и строгий JSON-контракт инструмента.
- `src/execution/DismOutputParser.*` — инкрементальный разбор англоязычного вывода DISM.
- `src/execution/SystemIntegrityWorkflow.*` — фиксированная последовательность DISM-команд в Executor.
- `src/execution/SystemToolLogWriter.*` — журнал полного вывода повышенного процесса.
- `src/execution/SystemToolSession.*` — асинхронный запуск Executor и жизненный цикл IPC на стороне интерфейса.
- `src/execution/ExecutorServer.*`, `src/execution/ExecutorClient.*` — транспорт новых сообщений без изменения транзакционного протокола.
- `src/execution/ExecutorLauncher.*` — неблокирующий опрос созданного процесса и явное закрытие handle.
- `src/app/SystemIntegrityController.*` — конечный автомат и свойства для QML.
- `apps/executor/main.cpp` — маршрутизация типизированного запроса в workflow.
- `apps/tweakopedia/main.cpp` — создание сеанса/контроллера и регистрация context property.
- `apps/tweakopedia/qml/pages/SystemIntegrityPage.qml` — состояние, действия и терминальная область.
- `apps/tweakopedia/qml/pages/ToolsPage.qml` — рабочая первая карточка и прежние заглушки остальных инструментов.
- `tests/fixtures/dism/` — короткие сохранённые фрагменты вывода для трёх итогов и ошибок.

### Task 1: Типизированный протокол и инкрементальный парсер DISM

**Files:**
- Create: `src/execution/SystemToolProtocol.h`
- Create: `src/execution/SystemToolProtocol.cpp`
- Create: `src/execution/DismOutputParser.h`
- Create: `src/execution/DismOutputParser.cpp`
- Create: `tests/execution/SystemToolProtocolTest.cpp`
- Create: `tests/execution/DismOutputParserTest.cpp`
- Create: `tests/fixtures/dism/check-health-healthy.txt`
- Create: `tests/fixtures/dism/check-health-repairable.txt`
- Create: `tests/fixtures/dism/check-health-non-repairable.txt`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class SystemToolOperation { Scan, Restore }`.
- Produces: `enum class SystemToolStage { ScanHealth, CheckHealth, RestoreHealth, VerifyRestore }`.
- Produces: `enum class ComponentStoreHealth { Healthy, Repairable, NonRepairable, Unknown }`.
- Produces: `SystemToolRequest`, `SystemToolStageEvent`, `SystemToolOutputEvent`, `SystemToolProgressEvent`, `SystemToolResult`.
- Produces: `SystemToolProtocol::encode/decode...()` с проверкой UUID, допустимой операции, этапа, `sequence`, процента и итогового состояния.
- Produces: `DismOutputParser::reset(stage)`, `consume(QByteArrayView)`, `progress()`, `health()`.

- [ ] **Step 1: Написать падающие тесты контракта и парсера**

Проверить круглую сериализацию всех сообщений, отказ для неизвестной операции/этапа, пустого или некорректного UUID, отрицательного `sequence` и процента вне `0..100`. Парсер должен распознавать три точные фразы `CheckHealth`, не считать `The operation completed successfully.` состоянием, извлекать `NN.N%` из разорванных порций, не уменьшать процент и сбрасывать его при смене этапа.

- [ ] **Step 2: Запустить тесты до реализации**

Run:

```powershell
$env:TEMP='D:\ChatGPT\Temp'; $env:TMP='D:\ChatGPT\Temp'
cmake --preset mingw-debug
cmake --build --preset mingw-debug --target SystemToolProtocolTest DismOutputParserTest
ctest --test-dir build/mingw-debug -R "^(SystemToolProtocol|DismOutputParser)$" --output-on-failure
```

Expected: FAIL при конфигурации или сборке из-за отсутствующих типов.

- [ ] **Step 3: Реализовать строгий протокол и потоковый парсер**

Сохранять небольшой хвост предыдущей порции, чтобы фразы и проценты распознавались на границе chunk. Итоговое `health` выдавать только после точной фразы; ограничить прогресс диапазоном и хранить максимум достигнутого значения внутри этапа.

- [ ] **Step 4: Запустить целевые тесты**

Run: `ctest --test-dir build/mingw-debug -R "^(SystemToolProtocol|DismOutputParser)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/execution tests/CMakeLists.txt tests/execution tests/fixtures/dism
git commit -m "feat: define system tool protocol and DISM parser"
```

### Task 2: Потоковые сообщения в существующем проверенном IPC

**Files:**
- Modify: `src/execution/ExecutorServer.h`
- Modify: `src/execution/ExecutorServer.cpp`
- Modify: `src/execution/ExecutorClient.h`
- Modify: `src/execution/ExecutorClient.cpp`
- Modify: `tests/execution/ExecutorIpcTest.cpp`

**Interfaces:**
- `ExecutorServer::sendToolRequest(const SystemToolRequest&)`.
- Signals: `toolStageReceived`, `toolOutputReceived`, `toolProgressReceived`, `toolResultReceived`.
- Signal: `ExecutorClient::toolRequestReceived(const SystemToolRequest&)`.
- Methods: `sendToolStage`, `sendToolOutput`, `sendToolProgress`, `sendToolResult`.
- `tool_result_ack` подтверждает доставку результата так же, как существующий `result_ack`.

- [ ] **Step 1: Расширить `ExecutorIpcTest` падающими сценариями**

Проверить один аутентифицированный tool-сеанс, сохранение `request_id`, порядок `sequence`, подтверждение результата, отклонение сообщения с чужим UUID, повторным/пропущенным `sequence`, неизвестным этапом и попытку второго клиента захватить сеанс. Существующий plan/rollback путь должен остаться зелёным.

- [ ] **Step 2: Запустить IPC-тест до реализации**

Run: `cmake --build --preset mingw-debug --target ExecutorIpcTest && ctest --test-dir build/mingw-debug -R "^ExecutorIpc$" --output-on-failure`

Expected: FAIL по новым ожиданиям.

- [ ] **Step 3: Добавить сообщения инструментов**

Декодировать payload только через `SystemToolProtocol`. После принятия запроса сервер запоминает UUID; клиент ведёт отдельный возрастающий `sequence`. Не менять имена и смысл существующих `progress`, `result`, `plan`, `rollback`.

- [ ] **Step 4: Запустить IPC и протокольные тесты**

Run: `ctest --test-dir build/mingw-debug -R "^(ExecutorIpc|ExecutionProtocol|SystemToolProtocol)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/execution/ExecutorServer.* src/execution/ExecutorClient.* tests/execution/ExecutorIpcTest.cpp
git commit -m "feat: stream system tool events over executor IPC"
```

### Task 3: Фиксированный DISM workflow и полный журнал Executor

**Files:**
- Create: `src/execution/IDismProcessRunner.h`
- Create: `src/execution/QProcessDismRunner.h`
- Create: `src/execution/QProcessDismRunner.cpp`
- Create: `src/execution/SystemToolLogWriter.h`
- Create: `src/execution/SystemToolLogWriter.cpp`
- Create: `src/execution/SystemIntegrityWorkflow.h`
- Create: `src/execution/SystemIntegrityWorkflow.cpp`
- Create: `tests/fakes/FakeDismProcessRunner.h`
- Create: `tests/fakes/FakeDismProcessRunner.cpp`
- Create: `tests/execution/SystemIntegrityWorkflowTest.cpp`
- Create: `tests/execution/SystemToolLogWriterTest.cpp`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- `IDismProcessRunner::start(QString program, QStringList arguments, OutputCallback, CompletionCallback)`; одновременно разрешён один процесс.
- `QProcessDismRunner` использует `QProcess::MergedChannels`, отправляет сырые порции и различает normal/crash/failed-to-start.
- `SystemToolLogWriter::open(logsRoot, operation, metadata)`, `appendCommand`, `appendOutput`, `path()`.
- `SystemIntegrityWorkflow::start(request)` и сигналы `stageChanged`, `outputReady`, `progressChanged`, `finished`.
- Workflow преобразует англоязычный консольный поток в UTF-8 JSON-текст и держит незавершённую строку; `\r` переписывает текущую строку, а не добавляет новую.

- [ ] **Step 1: Написать падающие тесты workflow и журнала**

Проверить точные program/arguments:

```text
DISM.exe /Online /Cleanup-Image /ScanHealth /English
DISM.exe /Online /Cleanup-Image /CheckHealth /English
DISM.exe /Online /Cleanup-Image /RestoreHealth /English
DISM.exe /Online /Cleanup-Image /CheckHealth /English
```

Проверить остановку после ненулевого кода/аварийного завершения/ошибки запуска, обязательный контрольный этап после успеха, итог `failed` при нераспознанном `CheckHealth`, поток событий и сохранение полного вывода в `logs/Tools/SystemIntegrity/yyyy-MM-dd_HH-mm-ss_<operation>.log`. Проверить, что banner каждой команды виден в интерфейсном потоке, а серия `\r`-обновлений процента заменяет одну строку. Невозможность открыть журнал должна завершать запрос отдельным кодом до запуска DISM.

- [ ] **Step 2: Запустить тесты до реализации**

Run: `cmake --build --preset mingw-debug --target SystemIntegrityWorkflowTest SystemToolLogWriterTest && ctest --test-dir build/mingw-debug -R "^(SystemIntegrityWorkflow|SystemToolLogWriter)$" --output-on-failure`

Expected: FAIL из-за отсутствующих классов.

- [ ] **Step 3: Реализовать runner, журнал и конечную последовательность**

Не использовать shell. Передавать аргументы списком. Парсер сбрасывать для каждого этапа, но итог брать только из завершающего `CheckHealth`. Заголовок журнала содержит операцию, Windows/DISM, время, команды и границы этапов. Продолжать дописывать текущую команду после разрыва IPC.

- [ ] **Step 4: Запустить целевые тесты**

Run: `ctest --test-dir build/mingw-debug -R "^(SystemIntegrityWorkflow|SystemToolLogWriter|DismOutputParser)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/execution tests/fakes/FakeDismProcessRunner.* tests/execution/SystemIntegrityWorkflowTest.cpp tests/execution/SystemToolLogWriterTest.cpp tests/CMakeLists.txt
git commit -m "feat: execute fixed DISM integrity workflows"
```

### Task 4: Маршрутизация tool-запроса в повышенном Executor

**Files:**
- Modify: `apps/executor/main.cpp`
- Modify: `apps/executor/CMakeLists.txt`
- Modify: `tests/execution/ExecutorIpcTest.cpp`

**Interfaces:**
- Executor создаёт `SystemIntegrityWorkflow` после `toolRequestReceived`.
- Каждый этап/вывод/процент пересылается через `ExecutorClient` с тем же `request_id`.
- После `tool_result_ack` Executor завершает работу; при разрыве IPC текущий workflow и журнал продолжаются до завершения.
- Test-only mode использует сохранённые fixture-порции вместо реального DISM.

- [ ] **Step 1: Добавить падающий end-to-end IPC-тест tool-сеанса**

Запустить `Tweakopedia.Executor.exe --test-tool-mode` отдельным процессом. Проверить handshake по PID/nonce, точный порядок `scan_health -> check_health -> tool_result`, поток вывода, возрастающий `sequence`, `healthy` и нормальное завершение только после ack. Отдельный сценарий разрывает GUI-сторону после первого chunk и проверяет, что тестовый Executor завершает fixture-команду и дописывает журнал.

- [ ] **Step 2: Запустить `ExecutorIpcTest` до реализации**

Run: `cmake --build --preset mingw-debug --target ExecutorIpcTest && ctest --test-dir build/mingw-debug -R "^ExecutorIpc$" --output-on-failure`

Expected: FAIL по новым tool-сценариям.

- [ ] **Step 3: Подключить workflow к `apps/executor/main.cpp`**

Executor принимает ровно один тип запроса за процесс. Для tool-сеанса не открывает базу транзакций и не создаёт запись истории. Путь журналов вычисляет сам через `AppPaths`; входящий запрос не содержит файлового пути.

- [ ] **Step 4: Запустить IPC-тест**

Run: `ctest --test-dir build/mingw-debug -R "^(ExecutorIpc|BuildSmoke)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add apps/executor tests/execution/ExecutorIpcTest.cpp
git commit -m "feat: host system integrity workflow in executor"
```

### Task 5: Асинхронный жизненный цикл повышенного сеанса

**Files:**
- Create: `src/execution/ISystemToolRunner.h`
- Create: `src/execution/SystemToolSession.h`
- Create: `src/execution/SystemToolSession.cpp`
- Create: `tests/execution/SystemToolSessionTest.cpp`
- Modify: `src/execution/ExecutorLauncher.h`
- Modify: `src/execution/ExecutorLauncher.cpp`
- Modify: `tests/execution/ExecutorLauncherTest.cpp`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- `ISystemToolRunner : QObject` с `start(SystemToolOperation)` и сигналами `launchCancelled`, `stageChanged`, `outputReceived`, `progressChanged`, `finished`, `failed`.
- `SystemToolSession` реализует один активный запрос и отвергает повторный запуск до результата.
- `ExecutorLauncher::poll(session)` возвращает `Running/Exited/Failed` без закрытия или завершения процесса.
- `ExecutorLauncher::close(session)` закрывает handle ровно один раз.

- [ ] **Step 1: Написать падающие тесты launcher/session**

Fake `IShellExecuteBackend` проверяет: `ShellExecuteExW` вызывается вне синхронного ожидания результата; отмена UAC выдаёт `launchCancelled`; после аутентификации отправляется запрос; вывод и прогресс сбрасывают 90-минутный inactivity watchdog; ранний exit, разрыв IPC и неизвестный UUID дают стабильные коды; результат закрывает handle; повторный `start()` во время операции не создаёт второй процесс.

- [ ] **Step 2: Запустить тесты до реализации**

Run: `cmake --build --preset mingw-debug --target ExecutorLauncherTest SystemToolSessionTest && ctest --test-dir build/mingw-debug -R "^(ExecutorLauncher|SystemToolSession)$" --output-on-failure`

Expected: FAIL по новым API.

- [ ] **Step 3: Реализовать неблокирующий сеанс**

Запуск `runas` выполнять через `QtConcurrent::run` и `QFutureWatcher`, затем устанавливать ожидаемый PID в `ExecutorServer`. Процесс опрашивать коротким `QTimer` через `poll()`. Watchdog измеряет отсутствие IPC-активности, а не общую длительность команды. На обычном закрытии приложения не вызывать `TerminateProcess`; закрыть локальный handle и позволить Executor завершить уже начатую команду.

- [ ] **Step 4: Запустить тесты session/launcher/IPC**

Run: `ctest --test-dir build/mingw-debug -R "^(ExecutorLauncher|SystemToolSession|ExecutorIpc)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/execution/ISystemToolRunner.h src/execution/SystemToolSession.* src/execution/ExecutorLauncher.* src/execution/CMakeLists.txt tests/execution/ExecutorLauncherTest.cpp tests/execution/SystemToolSessionTest.cpp tests/CMakeLists.txt
git commit -m "feat: add asynchronous system tool session"
```

### Task 6: Контроллер состояния и ограниченный терминальный буфер

**Files:**
- Create: `src/app/SystemIntegrityController.h`
- Create: `src/app/SystemIntegrityController.cpp`
- Create: `src/app/ISystemIntegrityDesktopServices.h`
- Create: `tests/fakes/FakeSystemToolRunner.h`
- Create: `tests/fakes/FakeSystemToolRunner.cpp`
- Create: `tests/app/SystemIntegrityControllerTest.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Properties: `state`, `stageText`, `progress`, `progressKnown`, `outputText`, `resultTitle`, `resultDescription`, `errorCode`, `exitCode`, `sessionLogPath`, `dismLogPath`, `busy`, `canScan`, `canRestore`.
- Invokables: `startScan()`, `startRestore()`, `copyOutput()`, `openDismLog()`.
- Produces: состояния `idle`, `awaiting_elevation`, `scanning`, `checking`, `healthy`, `repairable`, `non_repairable`, `repairing`, `verifying_repair`, `failed`.
- Desktop actions передаются через узкий `ISystemIntegrityDesktopServices`, чтобы clipboard/open-file тестировались без реального рабочего стола.

- [ ] **Step 1: Написать падающие тесты конечного автомата**

Проверить все переходы проверки/восстановления, наличие `canRestore` только для `repairable`, блокировку повторного запуска, возврат в `idle` после отмены UAC, `failed` после ошибок с сохранением частичного вывода, обновление журнала и кода завершения, русские подписи каждого этапа и результата.

Отдельно подать более 2 МиБ вывода: `outputText` должен хранить последние данные и маркер усечения, а fake runner/fixture-журнал — весь поток. Проверить фиксированный путь DISM и понятное сообщение при отсутствии файла.

- [ ] **Step 2: Запустить тест до реализации**

Run: `cmake --build --preset mingw-debug --target SystemIntegrityControllerTest && ctest --test-dir build/mingw-debug -R "^SystemIntegrityController$" --output-on-failure`

Expected: FAIL из-за отсутствующего контроллера.

- [ ] **Step 3: Реализовать контроллер**

Контроллер живёт на уровне приложения, подписывается на runner один раз и не сбрасывает состояние при навигации. `startScan()` очищает старый экранный вывод только перед новым подтверждённым запросом; отмена UAC сохраняет старый результат до следующего запуска и добавляет уведомление. Не разбирать текст DISM в контроллере. `TweakopediaApp` получает явную зависимость от `TweakopediaExecution`, но обратной зависимости не создаётся.

- [ ] **Step 4: Запустить тесты контроллера и session**

Run: `ctest --test-dir build/mingw-debug -R "^(SystemIntegrityController|SystemToolSession)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Закоммитить**

```powershell
git add src/app/SystemIntegrityController.* src/app/ISystemIntegrityDesktopServices.h src/app/CMakeLists.txt tests/fakes/FakeSystemToolRunner.* tests/app/SystemIntegrityControllerTest.cpp tests/CMakeLists.txt
git commit -m "feat: model system integrity tool state"
```

### Task 7: Fluent-страница проверки и интеграция с «Инструментами»

**Files:**
- Create: `apps/tweakopedia/qml/pages/SystemIntegrityPage.qml`
- Modify: `apps/tweakopedia/qml/pages/ToolsPage.qml`
- Modify: `apps/tweakopedia/qml/Main.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Modify: `apps/tweakopedia/main.cpp`
- Modify: `tests/qml/tst_toolspage.qml`
- Create: `tests/qml/tst_systemintegritypage.qml`
- Modify: `tests/visual/ToolsPagePreview.qml`
- Create: `tests/visual/SystemIntegrityPagePreview.qml`

**Interfaces:**
- `ToolsPage.integrityController` передаётся из `Main.qml`.
- Первая карточка показывает `SystemIntegrityPage`; индексы 1–4 используют существующую заглушку.
- Страница эмитит только `backRequested`; операция остаётся в C++-контроллере.

- [ ] **Step 1: Написать падающие QML-тесты**

Fake QML-controller проверяет: исходную кнопку «Начать проверку»; недоступность действий и этап/прогресс во время работы; кнопку «Восстановить» только для `repairable`; отсутствие этой кнопки для `healthy`; терминальный текст; «К последней строке» после ручной прокрутки; возврат к сетке без вызова отмены; повторное открытие с прежним состоянием.

В узком размере проверить, что действия переносятся вниз и не выходят за `availableWidth`. Проверить object names для состояния, терминала, progress, основных и вторичных кнопок.

- [ ] **Step 2: Запустить QML-тесты до реализации**

Run: `cmake --build --preset mingw-debug --target QmlComponentTests && ctest --test-dir build/mingw-debug -R "^QmlPlanPreviewHistory$" --output-on-failure`

Expected: FAIL по отсутствующей странице/новым ожиданиям.

- [ ] **Step 3: Реализовать страницу и wiring**

Собрать три зоны из спецификации. Терминал реализовать через read-only `TextArea`/`Flickable`: `\r` уже нормализован Executor, auto-follow активен только у нижней границы. Добавить кнопки копирования и открытия журнала. В `main.cpp` создать `WindowsShellExecuteBackend`, `SystemToolSession`, desktop-services и `SystemIntegrityController` с временем жизни до завершения `QGuiApplication`, затем зарегистрировать `systemIntegrityController` до загрузки QML.

- [ ] **Step 4: Запустить QML и C++ тесты**

Run: `ctest --test-dir build/mingw-debug -R "^(QmlPlanPreviewHistory|SystemIntegrityController|SystemToolSession)$" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Снять и проверить визуальные состояния**

Снять `idle`, `scanning`, `repairable`, `healthy`, `failed` в широком и узком окне. Проверить 100/125/150/200% DPI: текст не обрезается, терминал занимает остаток высоты, кнопки остаются внутри карточки, фокус и hover соответствуют текущему Fluent-набору.

- [ ] **Step 6: Закоммитить**

```powershell
git add apps/tweakopedia tests/qml tests/visual
git commit -m "feat: add Fluent system integrity screen"
```

### Task 8: Полная проверка, portable-сборка и документация

**Files:**
- Modify: `README.md`
- Create: `docs/architecture.md`
- Create: `docs/testing/system-integrity-tool-results.md`
- Modify: `tests/smoke/BuildSmokeTest.cpp`
- Modify: `tools/verify-package.ps1`

- [ ] **Step 1: Усилить smoke/package проверки**

Проверить наличие `SystemIntegrityPage.qml` и нового кода Executor в runtime payload, прохождение `--self-check`, отсутствие отдельного распространяемого файла и прежнюю загрузку остальных страниц. Добавить smoke-проверку, что test-tool mode завершается без реального DISM.

- [ ] **Step 2: Обновить документацию**

Описать пользовательский сценарий проверки/восстановления, расположение журналов, отсутствие пользовательских источников и отмены в первом выпуске, IPC-границу и команды тестирования.

- [ ] **Step 3: Запустить полный debug CTest**

Run:

```powershell
$env:TEMP='D:\ChatGPT\Temp'; $env:TMP='D:\ChatGPT\Temp'
.\tools\test.ps1 -Preset mingw-debug
```

Expected: все тесты PASS.

- [ ] **Step 4: Собрать и проверить portable EXE**

Run:

```powershell
$env:TEMP='D:\ChatGPT\Temp'; $env:TMP='D:\ChatGPT\Temp'
.\tools\package.ps1 -Preset mingw-release
.\tools\verify-package.ps1
```

Expected: один `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`, успешный self-check и отсутствие незапакованных Qt DLL рядом с артефактом.

- [ ] **Step 5: Выполнить ручную проверку на Windows**

Проверить: отмену UAC; исправный `ScanHealth`; живой scroll/auto-follow; переход в другой раздел и обратно; открытие системного журнала; копирование вывода; повторный запуск. Сценарии `repairable` и `non_repairable` проверять только fixture-runner, не изменяя состояние Windows специально.

- [ ] **Step 6: Закоммитить**

```powershell
git add README.md docs tests/smoke/BuildSmokeTest.cpp tools/verify-package.ps1
git commit -m "docs: document system integrity tool"
```

## Итоговые критерии

- Нажатие первой карточки открывает рабочий экран DISM; остальные карточки остаются заглушками.
- Проверка и восстановление используют только утверждённые фиксированные команды с `/English`.
- Вывод и прогресс поступают в интерфейс во время выполнения без блокировки GUI.
- Решение определяется типизированным результатом `CheckHealth`, а не повторным разбором текста в QML/C++-контроллере.
- Восстановление предлагается только после `repairable` и всегда завершается контрольным `CheckHealth`.
- Разрыв интерфейса не обрывает уже запущенную команду и полный журнал Executor.
- Отмена UAC, ошибки запуска, ненулевой код, аварийный exit, разрыв IPC и неизвестный итог имеют разные стабильные коды.
- Все новые unit/IPC/QML/smoke тесты и полный CTest проходят.
- Portable-поставка остаётся одним EXE.
