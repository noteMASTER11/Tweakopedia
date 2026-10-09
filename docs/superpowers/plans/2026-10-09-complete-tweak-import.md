# Complete Tweak Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить все оставшиеся самостоятельные настройки из предоставленных твикеров и Winaero, расширив типизированный транзакционный движок без управления службами и без произвольных сценариев.

**Architecture:** Сначала строится исчерпывающий нормализованный audit-manifest с решением для каждого кандидата. Затем движок расширяется вертикальными TDD-срезами: обобщённый реестр и параметризованный ввод, файлы, задачи, BCD, питание, Optional Features/Capabilities. После появления всех типов принятые кандидаты превращаются в YAML-каталог и проходят единый аудит, полную регрессию и portable-упаковку.

**Tech Stack:** C++20, Qt 6.8.3, QML/Qt Quick, yaml-cpp, Win32 Registry API, Task Scheduler COM, BCD WMI Provider, PowrProf API, фиксированные вызовы DISM, CMake, Qt Test, PowerShell 7.

**Spec:** `docs/superpowers/specs/2026-10-09-complete-tweak-import-design.md`

## Global Constraints

- Поддерживаются Windows 10/11 x64; ARM64 и x86 не добавляются.
- Категория и операции управления службами отсутствуют.
- YAML не содержит произвольных PowerShell/CMD-команд или редактируемых executable.
- Пользовательские inputs меняют только payload типизированной операции, но не системный путь или идентификатор объекта.
- Все исходные значения снимка сохраняются с точным native type и байтами.
- Ссылки и библиография не показываются в пользовательских статьях.
- Итоговое количество твиков определяется audit-manifest, а не квотой.
- Промежуточные коммиты не создаются; спецификация, план, код, контент, тесты и отчёт входят в один итоговый коммит.
- План выполняется последовательно текущим агентом через `superpowers:executing-plans`; внутреннее делегирование не используется.
- Временные файлы, инвентаризация и кэши размещаются только под `D:\ChatGPT`.

## Review Focus

- Алиасы hive, `ControlSet001`/`CurrentControlSet`, 32/64-bit views и повторения из нескольких источников должны нормализоваться в одно решение manifest; проверяется в Task 1.
- Изменение или удаление выбранного файла после добавления в очередь должно блокировать применение до создания проверенной копии; проверяется в Task 6.
- Изменение фактического системного значения после preview должно выявляться повторной проверкой fingerprint; проверяется в Tasks 3, 7–10.
- Отсутствующая задача, BCD element, power setting, feature или capability должна давать `unsupported`, а не ложное состояние; проверяется в Tasks 7–10.
- Ошибка в смешанном пакете после нескольких типов операций должна возвращать выполненные операции в обратном порядке; проверяется в Task 11.

---

### Task 1: Исчерпывающая инвентаризация и audit-manifest

**Files:**
- Create: `tools/inventory-tweak-sources.ps1`
- Create: `docs/audits/complete-tweak-import.json`
- Create: `tests/content/CompleteTweakImportAuditTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: исходные каталоги из спецификации, текущие YAML и официальная страница списка функций Winaero.
- Produces: manifest schema `tweakopedia.complete-import-audit/1` с массивом `candidates[]`, полями `candidate_key`, `object_type`, `normalized_object`, `decision`, `tweak_id`, `reason_code`, `details`; PowerShell tool принимает `-SourceRoots`, `-ExistingContentRoot`, `-OutputPath`.

- [x] **Step 1: Write the failing audit contract test**

  Добавить тесты `manifestUsesSupportedSchema`, `candidateKeysAreUnique`, `everyCandidateHasDecision`, `acceptedEntriesHaveStablePlannedIds`, `duplicateEntriesReferenceExistingTweak`, `serviceCandidatesAreRejected`, `normalizesRegistryAliasesAndControlSets`. До manifest тест должен завершиться ошибкой отсутствующего файла.

- [x] **Step 2: Run the audit test to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex CompleteTweakImportAudit`
  Expected: FAIL, `complete-tweak-import.json` отсутствует.

- [x] **Step 3: Implement the read-only inventory tool**

  Сканировать `.reg`, `.ps1`, `.cmd`, `.bat`, конфигурационные JSON и описания функций. Нормализовать регистр hive/path, view, `ControlSet001` к логическому `CurrentControlSet`, тип операции и идентификаторы. Raw inventory писать в `D:\ChatGPT\Temp\Tweakopedia\complete-import`.

- [x] **Step 4: Build the reviewed manifest**

  Получить список функций с официальной страницы Winaero, сохранить рабочий снимок только в `D:\ChatGPT\Temp\Tweakopedia\complete-import`, удалить текстовые повторения, сопоставить текущие 685 YAML и присвоить каждому кандидату первоначальное решение. Для `accepted` на этом этапе требуется стабильный планируемый `tweak_id`; фактическое наличие в каталоге проверяется в Tasks 14–15. Для `duplicate` ID уже должен существовать. Не считать строку сценария отдельным кандидатом, если она является частью одной составной настройки.

- [x] **Step 5: Run the audit test to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex CompleteTweakImportAudit`
  Expected: PASS; решений без `decision` нет; `duplicate` ссылаются на существующие твики; кандидаты служб имеют `reason_code=service-management`.

### Task 2: Обобщённая модель значений реестра и YAML

**Files:**
- Modify: `src/domain/RegistryTypes.h`
- Modify: `src/domain/TweakDefinition.h`
- Modify: `src/domain/TweakDefinition.cpp`
- Modify: `src/content/TweakCatalogLoader.cpp`
- Modify: `src/platform/IRegistryBackend.h`
- Modify: `src/platform/WindowsRegistryBackend.cpp`
- Modify: `src/platform/WindowsRegistryBackend.h`
- Modify: `tests/fakes/FakeRegistryBackend.cpp`
- Modify: `tests/fakes/FakeRegistryBackend.h`
- Modify: `tests/domain/TweakDefinitionTest.cpp`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Modify: `tests/platform/WindowsRegistryBackendTest.cpp`
- Create: `tests/fixtures/content/valid/registry-values.yaml`
- Create: `tests/fixtures/content/invalid/registry-input-in-path.yaml`

**Interfaces:**
- Consumes: существующие `RegistryLocation`, `RegistryReadResult`, `registry.set_dword`.
- Produces: `RegistryValueSpec { quint32 nativeType; QByteArray rawValue; }`, `SetRegistryValueOperation`, `DeleteRegistryValueOperation`, `RegistryValueDetection`; backend method `writeValue(location, value)`; YAML types `registry.set_value`, `registry.delete_value`, `registry.value`. Старый DWORD-синтаксис остаётся совместимым.

- [x] **Step 1: Write failing domain and loader tests**

  Проверить round-trip литералов DWORD, QWORD, string, expand-string, multi-string и binary; отсутствие значения; отказ от неизвестного native type; запрет input-подстановки в `key` и `value_name`.

- [x] **Step 2: Run the targeted tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "DomainTweakDefinition|TweakCatalogLoader|WindowsRegistryBackend"`
  Expected: FAIL на неизвестных `registry.set_value` и `registry.value`.

- [x] **Step 3: Implement registry value types and parsing**

  Кодировать строки UTF-16LE с завершающим NUL, multi-string с двойным NUL, integers little-endian, binary base64/hex по одному каноническому правилу. Сохранить `registry.set_dword` как shorthand.

- [x] **Step 4: Implement backend writes for native registry types**

  `WindowsRegistryBackend::writeValue(const RegistryLocation&, const RegistryValueSpec&)` вызывает `RegSetValueExW`; `writeDword` делегирует ему. Fake backend хранит native type и bytes.

- [x] **Step 5: Run the targeted tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "DomainTweakDefinition|TweakCatalogLoader|WindowsRegistryBackend"`
  Expected: PASS.

### Task 3: Обнаружение, план, протокол и возврат обобщённого реестра

**Files:**
- Create: `src/detection/RegistryValueStateDetector.h`
- Create: `src/detection/RegistryValueStateDetector.cpp`
- Create: `src/execution/RegistryValueExecutor.h`
- Create: `src/execution/RegistryValueExecutor.cpp`
- Modify: `src/planning/ExecutionPlan.h`
- Modify: `src/planning/PlanBuilder.cpp`
- Modify: `src/execution/ExecutionProtocol.cpp`
- Modify: `src/execution/PlanValidator.cpp`
- Modify: `src/execution/RegistrySnapshot.cpp`
- Modify: `src/execution/TransactionRunner.cpp`
- Modify: module `CMakeLists.txt` files
- Create: `tests/detection/RegistryValueStateDetectorTest.cpp`
- Create: `tests/execution/RegistryValueExecutorTest.cpp`
- Modify: `tests/planning/PlanBuilderTest.cpp`
- Modify: `tests/execution/ExecutionProtocolTest.cpp`
- Modify: `tests/execution/PlanValidatorTest.cpp`
- Modify: `tests/execution/TransactionRunnerTest.cpp`

**Interfaces:**
- Consumes: `RegistryValueSpec`, generic backend reads/writes, existing `RegistrySnapshot` raw bytes.
- Produces: `PlannedRegistryValueChange`; protocol JSON type `registry.set_value` or `registry.delete_value`; exact type+bytes fingerprint; executor `apply(operation)` and `restore(snapshot)`.

- [x] **Step 1: Write failing detector/executor tests**

  Проверить точное сравнение native type, missing state, string/QWORD/binary writes, delete, verify-after-write и восстановление отсутствующего/существовавшего значения.

- [x] **Step 2: Write failing plan/protocol/transaction tests**

  Проверить канонический JSON, отказ от пустых путей и oversized binary, stale fingerprint и возврат первой выполненной registry-value операции после ошибки второй.

- [x] **Step 3: Run targeted tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "RegistryValue|PlanBuilder|ExecutionProtocol|PlanValidator|TransactionRunner"`
  Expected: FAIL из-за отсутствующих generic variants.

- [x] **Step 4: Implement detection, plan and protocol**

  Сохранить чтение старых DWORD-твиков через прежний detector либо адаптер. Новые значения используют exact native type/bytes and missing state.

- [x] **Step 5: Implement executor and mixed rollback**

  Все generic registry snapshots используют существующий raw snapshot format; TransactionRunner размещает их в общей последовательности отката.

- [x] **Step 6: Run targeted tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "RegistryValue|PlanBuilder|ExecutionProtocol|PlanValidator|TransactionRunner"`
  Expected: PASS.

### Task 4: Создание и удаление ветвей реестра

**Files:**
- Create: `src/domain/RegistryTree.h`
- Modify: `src/domain/RegistryTypes.h`
- Modify: `src/platform/IRegistryBackend.h`
- Modify: `src/platform/WindowsRegistryBackend.cpp`
- Modify: `src/content/TweakCatalogLoader.cpp`
- Create: `src/execution/RegistryTreeExecutor.h`
- Create: `src/execution/RegistryTreeExecutor.cpp`
- Modify: `src/planning/ExecutionPlan.h`
- Modify: `src/planning/PlanBuilder.cpp`
- Modify: `src/execution/ExecutionProtocol.cpp`
- Modify: `src/execution/TransactionRunner.cpp`
- Create: `tests/execution/RegistryTreeExecutorTest.cpp`
- Modify: `tests/platform/WindowsRegistryBackendTest.cpp`
- Modify: `tests/execution/ExecutionProtocolTest.cpp`
- Modify: `tests/execution/TransactionRunnerTest.cpp`

**Interfaces:**
- Produces: `RegistryTreeSnapshot`, backend methods `readTree`, `createKey`, `deleteTree`, `restoreTree`; operations `registry.create_key`, `registry.delete_key`; protocol variants with fixed location.

- [x] **Step 1: Write failing subtree snapshot tests**

  Создать отдельную ветвь HKCU с вложенными значениями разных типов; проверить полную сериализацию, удаление и точное восстановление.

- [x] **Step 2: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "RegistryTree|WindowsRegistryBackend|ExecutionProtocol|TransactionRunner"`
  Expected: FAIL, tree API отсутствует.

- [x] **Step 3: Implement bounded tree traversal and restore**

  Ограничить snapshot по числу узлов и общему размеру; при превышении вернуть ошибку до изменения ветви. Не следовать ссылкам вне выбранного subtree.

- [x] **Step 4: Integrate plan, protocol and rollback**

  Канонический object key включает hive/view/path. Create/delete конфликтуют с любыми value operations внутри subtree.

- [x] **Step 5: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "RegistryTree|WindowsRegistryBackend|ExecutionProtocol|TransactionRunner"`
  Expected: PASS.

### Task 5: Параметризованные definitions и очередь

**Files:**
- Create: `src/domain/TweakInput.h`
- Modify: `src/domain/TweakDefinition.h`
- Modify: `src/content/TweakCatalogLoader.cpp`
- Modify: `src/planning/TweakQueue.h`
- Modify: `src/planning/TweakQueue.cpp`
- Modify: `src/planning/PlanBuilder.h`
- Modify: `src/planning/PlanBuilder.cpp`
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `src/app/TweakListModel.h`
- Modify: `src/app/TweakListModel.cpp`
- Modify: `src/app/QueueListModel.h`
- Modify: `src/app/QueueListModel.cpp`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Modify: `tests/planning/TweakQueueTest.cpp`
- Modify: `tests/planning/PlanBuilderTest.cpp`
- Modify: `tests/app/AppControllerTest.cpp`
- Modify: `tests/app/TweakListModelTest.cpp`

**Interfaces:**
- Produces: `TweakInputDefinition`, typed `TweakInputValue`, `TweakInputMap`; `QueueItem { tweakId, targetState, inputs }`; QML role `inputs`; invokable `selectParameterizedTarget(id, state, QVariantMap)`.

- [x] **Step 1: Write failing input validation tests**

  Проверить required text, trimmed length, integer range, choice membership, boolean type, allowed file extensions и отсутствие undeclared fields.

- [x] **Step 2: Write failing queue and planner tests**

  Проверить нормализацию inputs, замену уже queued значения, сохранение inputs в preview, подстановку только в payload и отказ от missing required input.

- [x] **Step 3: Run targeted tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "TweakCatalogLoader|TweakQueue|PlanBuilder|AppController|TweakListModel"`
  Expected: FAIL на отсутствующих input types/API.

- [x] **Step 4: Implement domain parsing and validation**

  Значения inputs остаются типизированными до QML boundary. Не использовать строковую подстановку для системных путей.

- [x] **Step 5: Implement queue, model and preview propagation**

  Queue summary показывает label и нормализованное значение; чувствительные поля в этой итерации отсутствуют.

- [x] **Step 6: Run targeted tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "TweakCatalogLoader|TweakQueue|PlanBuilder|AppController|TweakListModel"`
  Expected: PASS.

### Task 6: Выбранные файлы и транзакционные input artifacts

**Files:**
- Create: `src/persistence/PendingInputStore.h`
- Create: `src/persistence/PendingInputStore.cpp`
- Modify: `src/persistence/AppPaths.h`
- Modify: `src/persistence/AppPaths.cpp`
- Modify: `src/persistence/TransactionFiles.h`
- Modify: `src/persistence/TransactionFiles.cpp`
- Create: `src/execution/FileOperationExecutor.h`
- Create: `src/execution/FileOperationExecutor.cpp`
- Modify: `src/domain/TweakDefinition.h`
- Modify: `src/content/TweakCatalogLoader.cpp`
- Modify: `src/planning/ExecutionPlan.h`
- Modify: `src/execution/ExecutionProtocol.cpp`
- Modify: `src/execution/PlanValidator.cpp`
- Modify: `src/execution/TransactionRunner.cpp`
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/persistence/PendingInputStoreTest.cpp`
- Create: `tests/execution/FileOperationExecutorTest.cpp`
- Modify: `tests/execution/ExecutionProtocolTest.cpp`
- Modify: `tests/execution/TransactionRunnerTest.cpp`

**Interfaces:**
- Produces: managed `InputArtifact { id, managedPath, size, sha256 }`; file operations `file.copy`, `file.replace`, `file.delete`; transaction-relative artifact reference accepted by Executor.

- [x] **Step 1: Write failing artifact lifecycle tests**

  Проверить копирование выбранного файла в data root, extension/size validation, hash, отказ при изменении исходника во время копирования и очистку неиспользуемых pending artifacts.

- [x] **Step 2: Write failing file executor tests**

  Проверить fixed destination, reject path traversal, hash mismatch, replacement backup, deletion backup and rollback.

- [x] **Step 3: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "PendingInputStore|FileOperation|ExecutionProtocol|TransactionRunner"`
  Expected: FAIL, artifact/file operation classes отсутствуют.

- [x] **Step 4: Implement pending store and transaction materialization**

  Перед sendPlan копировать artifact в `transactions/<id>/inputs`, повторно считать size/hash и кодировать только relative path.

- [x] **Step 5: Implement file executor and rollback**

  Destination берётся только из catalog-derived plan. Backup располагается внутри transaction directory.

- [x] **Step 6: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "PendingInputStore|FileOperation|ExecutionProtocol|TransactionRunner"`
  Expected: PASS.

### Task 7: Состояние задач планировщика

**Files:**
- Create: `src/platform/IScheduledTaskBackend.h`
- Create: `src/platform/WindowsScheduledTaskBackend.h`
- Create: `src/platform/WindowsScheduledTaskBackend.cpp`
- Create: `src/detection/ScheduledTaskStateDetector.h`
- Create: `src/detection/ScheduledTaskStateDetector.cpp`
- Create: `src/execution/ScheduledTaskExecutor.h`
- Create: `src/execution/ScheduledTaskExecutor.cpp`
- Modify: domain, loader, plan, protocol and `TransactionRunner` variants
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/platform/WindowsScheduledTaskBackendTest.cpp`
- Create: `tests/detection/ScheduledTaskStateDetectorTest.cpp`
- Create: `tests/execution/ScheduledTaskExecutorTest.cpp`

**Interfaces:**
- Produces: `ScheduledTaskLocation { folder, name }`, states `missing/enabled/disabled/error`, operation `scheduled_task.set_enabled`, exact enabled snapshot.

- [x] **Step 1: Write failing backend/detector/executor tests**

  Использовать временную тестовую задачу с нейтральным action; проверить missing→unsupported, enabled/disabled, stale fingerprint and rollback.

- [x] **Step 2: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex ScheduledTask`
  Expected: FAIL, task interfaces отсутствуют.

- [x] **Step 3: Implement Task Scheduler COM backend**

  Подключаться к local service, открывать только fixed folder/name, менять только `Enabled`; не создавать и не удалять задачи.

- [x] **Step 4: Integrate detector, plan, protocol and transaction runner**

  Missing component filters as unsupported and preserves category hiding behavior.

- [x] **Step 5: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex ScheduledTask`
  Expected: PASS.

### Task 8: Типизированные BCD elements

**Files:**
- Create: `src/platform/IBcdBackend.h`
- Create: `src/platform/WindowsBcdBackend.h`
- Create: `src/platform/WindowsBcdBackend.cpp`
- Create: `src/detection/BcdStateDetector.h`
- Create: `src/detection/BcdStateDetector.cpp`
- Create: `src/execution/BcdElementExecutor.h`
- Create: `src/execution/BcdElementExecutor.cpp`
- Modify: domain, loader, plan, protocol, validator and transaction runner variants
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/detection/BcdStateDetectorTest.cpp`
- Create: `tests/execution/BcdElementExecutorTest.cpp`
- Create: `tests/platform/WindowsBcdBackendTest.cpp`

**Interfaces:**
- Produces: `BcdElementSpec { objectId, elementType, valueKind }`, typed values bool/integer/string, operation `bcd.set_element` or reset to missing through BCD WMI Provider.

- [x] **Step 1: Write failing fake-backed detector/executor tests**

  Проверить whitelist element types, missing element, typed equality, write verification and restore-to-missing.

- [x] **Step 2: Add a read-only Windows integration test**

  Прочитать только выбранные элементы `{current}`/`{bootmgr}` и отметить unsupported при отсутствии provider. Тест не записывает рабочий BCD.

- [x] **Step 3: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex Bcd`
  Expected: FAIL, BCD API отсутствует.

- [x] **Step 4: Implement WMI backend and typed pipeline**

  Не принимать произвольные identifiers из QML; все IDs и element types загружаются из проверенного content.

- [x] **Step 5: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex Bcd`
  Expected: PASS либо только документированный read-only skip при отсутствии provider.

### Task 9: Значения схем питания

**Files:**
- Create: `src/platform/IPowerSettingBackend.h`
- Create: `src/platform/WindowsPowerSettingBackend.h`
- Create: `src/platform/WindowsPowerSettingBackend.cpp`
- Create: `src/detection/PowerSettingStateDetector.h`
- Create: `src/detection/PowerSettingStateDetector.cpp`
- Create: `src/execution/PowerSettingExecutor.h`
- Create: `src/execution/PowerSettingExecutor.cpp`
- Modify: domain, loader, plan, protocol and transaction runner variants
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/detection/PowerSettingStateDetectorTest.cpp`
- Create: `tests/execution/PowerSettingExecutorTest.cpp`
- Create: `tests/platform/WindowsPowerSettingBackendTest.cpp`

**Interfaces:**
- Produces: `PowerSettingLocation { scheme, subgroup, setting, source }`, source `ac|dc`, operation `power.set_index`; backend uses PowrProf reads/writes and applies scheme notification only after successful package.

- [x] **Step 1: Write failing fake-backed tests**

  Проверить AC/DC independence, paired tweak, missing setting→unsupported, index range, stale read and rollback exact index.

- [x] **Step 2: Add bounded Windows integration fixture**

  Создать временную дубликат-схему, изменить test setting, проверить и удалить fixture in teardown; рабочая активная схема не меняется.

- [x] **Step 3: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex PowerSetting`
  Expected: FAIL, power interfaces отсутствуют.

- [x] **Step 4: Implement PowrProf backend and pipeline**

  GUID parsing is strict; content cannot target an empty/active alias without resolved scheme semantics.

- [x] **Step 5: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex PowerSetting`
  Expected: PASS.

### Task 10: Optional Features и Capabilities

**Files:**
- Create: `src/platform/IWindowsComponentBackend.h`
- Create: `src/platform/WindowsComponentBackend.h`
- Create: `src/platform/WindowsComponentBackend.cpp`
- Create: `src/detection/WindowsComponentStateDetector.h`
- Create: `src/detection/WindowsComponentStateDetector.cpp`
- Create: `src/execution/WindowsComponentExecutor.h`
- Create: `src/execution/WindowsComponentExecutor.cpp`
- Modify: domain, loader, plan, protocol and transaction runner variants
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/detection/WindowsComponentStateDetectorTest.cpp`
- Create: `tests/execution/WindowsComponentExecutorTest.cpp`
- Create: `tests/platform/WindowsComponentBackendTest.cpp`

**Interfaces:**
- Produces: component kind `feature|capability`, state `enabled|disabled|absent|unsupported`, operations `windows_feature.set_state`, `windows_capability.set_state`; fixed validated DISM argument builder with no shell parsing.

- [x] **Step 1: Write failing command-builder and fake-backed tests**

  Проверить name pattern, exact argument vector, missing payload, restart code, result verification, unsupported state and reverse operation.

- [x] **Step 2: Add read-only Windows detection tests**

  Проверить известный встроенный feature и capability without changing them.

- [x] **Step 3: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex WindowsComponent`
  Expected: FAIL, component backend отсутствует.

- [x] **Step 4: Implement typed DISM backend and pipeline**

  Launch `%SystemRoot%\System32\dism.exe` directly with argument list; capture exit code/output; determine reboot requirement from documented result codes.

- [x] **Step 5: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex WindowsComponent`
  Expected: PASS.

### Task 11: Смешанные пакеты и Executor wiring

**Files:**
- Modify: `src/execution/TransactionRunner.h`
- Modify: `src/execution/TransactionRunner.cpp`
- Modify: `apps/executor/main.cpp`
- Modify: `apps/executor/CMakeLists.txt`
- Modify: `apps/tweakopedia/main.cpp`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/execution/ExecutorIpcTest.cpp`
- Modify: `tests/execution/TransactionRunnerTest.cpp`
- Modify: `tests/integration/VerticalSliceWorkflowTest.cpp`

**Interfaces:**
- Consumes: every planned operation/backend from Tasks 3–10.
- Produces: one ordered executor dispatch and reverse-order rollback across heterogeneous snapshots; progress includes operation title/type/index.

- [x] **Step 1: Write failing heterogeneous transaction test**

  План содержит generic registry value, file, task and power operation; injected failure in fourth operation must restore first three in reverse order and persist all snapshots.

- [x] **Step 2: Write failing IPC round-trip test**

  Send mixed plan through local server/client in test mode and assert schema, progress, result and before.json operation types.

- [x] **Step 3: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "TransactionRunner|ExecutorIpc|VerticalSliceWorkflow"`
  Expected: FAIL on missing backend dispatch.

- [x] **Step 4: Introduce typed backend bundle**

  Replace the growing constructor argument list with `ExecutionBackends` containing nullable interfaces. Missing required backend returns `operation.unsupported` before mutation.

- [x] **Step 5: Wire production and test executors**

  Main process supplies detectors; elevated process constructs write backends. Test mode uses fakes and no live system mutation.

- [x] **Step 6: Run tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "TransactionRunner|ExecutorIpc|VerticalSliceWorkflow"`
  Expected: PASS.

### Task 12: Fluent input editors and queue details

**Files:**
- Create: `apps/tweakopedia/qml/components/TweakInputEditor.qml`
- Create: `apps/tweakopedia/qml/components/TextInputEditor.qml`
- Create: `apps/tweakopedia/qml/components/IntegerInputEditor.qml`
- Create: `apps/tweakopedia/qml/components/FileInputEditor.qml`
- Modify: `apps/tweakopedia/qml/components/TweakRow.qml`
- Modify: `apps/tweakopedia/qml/pages/TweaksPage.qml`
- Modify: `apps/tweakopedia/qml/pages/QueuePage.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Modify: `src/app/TweakListModel.cpp`
- Modify: `src/app/QueueListModel.cpp`
- Create: `tests/qml/tst_tweakinputs.qml`
- Modify: `tests/qml/tst_tweakrow.qml`
- Modify: `tests/qml/tst_remainingpages.qml`

**Interfaces:**
- Consumes: QML input model role and `selectParameterizedTarget` from Task 5; managed-file chooser endpoint from Task 6.
- Produces: Fluent text/number/file/choice/boolean editors; confirmed input map; concrete queue detail rows.

- [x] **Step 1: Write failing QML tests**

  Проверить editor selection by type, text confirmation button, Enter handling, integer error, file choose callback, dropdown choice, toggle, card click isolation, pending gradient and concrete queue values.

- [x] **Step 2: Run QML tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex QmlPlanPreviewHistory`
  Expected: FAIL because editor components are absent.

- [x] **Step 3: Implement editors and TweakRow layout**

  Use existing FluentTheme dimensions and state selector. On narrow widths editor moves below summary instead of leaving the card bounds.

- [x] **Step 4: Implement file dialog/controller boundary**

  QML receives only selected managed artifact metadata; raw local path is not sent to Executor.

- [x] **Step 5: Implement queue presentation**

  Queue card shows each label/value and lets its free area open the existing explanation pane.

- [x] **Step 6: Run QML tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex QmlPlanPreviewHistory`
  Expected: PASS.

### Task 13: OEM vertical slice

**Files:**
- Create: `content/tweaks/devices/oem-manufacturer.yaml`
- Create: `content/tweaks/devices/oem-model.yaml`
- Create: `content/tweaks/devices/oem-support-hours.yaml`
- Create: `content/tweaks/devices/oem-support-phone.yaml`
- Create: `content/tweaks/devices/oem-support-url.yaml`
- Create: `content/tweaks/devices/oem-logo.yaml`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Modify: `tests/integration/VerticalSliceWorkflowTest.cpp`
- Create: `tests/visual/OemTweaksPreview.qml`
- Modify: `tests/visual/VisualCaptureMain.cpp`
- Modify: `tools/capture-ui.ps1`

**Interfaces:**
- Consumes: parameterized registry/file flow.
- Produces: six independently queued OEM editors in `devices/metadata` with clear/delete state and full articles.

- [x] **Step 1: Write failing catalog and workflow tests**

  Assert six IDs, exact OEMInformation values, BMP constraints, registry string preview, logo artifact hash and rollback of previously missing values/files.

- [x] **Step 2: Run tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "TweakCatalogLoader|VerticalSliceWorkflow"`
  Expected: FAIL because OEM definitions are missing.

- [x] **Step 3: Add six definitions and articles**

  Clear state uses `registry.delete_value`; logo destination is fixed and only BMP is accepted.

- [x] **Step 4: Run tests and visual capture to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "TweakCatalogLoader|VerticalSliceWorkflow|QmlPlanPreviewHistory"`
  Расширить `capture-ui.ps1` параметрами `-InputQml` и `-OutputName`, сохранив текущие значения по умолчанию.
  Run: `pwsh .\tools\capture-ui.ps1 -InputQml .\tests\visual\OemTweaksPreview.qml -OutputName oem-tweaks`
  Expected: PASS and aligned editors at wide/narrow preview widths.

### Task 14: Импорт всех принятых registry-value и registry-tree кандидатов

**Files:**
- Create/Modify: `content/tweaks/**/*.yaml` according to accepted registry candidates
- Modify: `content/categories.yaml` only for necessary subcategories
- Modify: `docs/audits/complete-tweak-import.json`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Modify: `tests/content/CompleteTweakImportAuditTest.cpp`

**Interfaces:**
- Consumes: reviewed manifest and registry operation families.
- Produces: one YAML per independently controllable setting; accepted/duplicate/rejected decision for every registry candidate.

- [x] **Step 1: Change content contract to the manifest-derived accepted ID set**

  Test reads accepted IDs dynamically from manifest, requires exact catalog containment and one decision per normalized candidate. Representative assertions cover every registry native type and category.

- [x] **Step 2: Run content tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "CompleteTweakImportAudit|TweakCatalogLoader|CategoryCatalogLoader"`
  Expected: FAIL listing all accepted IDs not yet implemented.

- [x] **Step 3: Verify candidates against primary documentation**

  Use local Microsoft ADMX first; for browser/application policy use official vendor policy documentation; for Windows policies use official Microsoft material. Correct or reject ambiguous source entries before YAML generation.

- [x] **Step 4: Add registry YAML definitions and Russian articles**

  Reuse categories; add subcategory only when no current group fits. No source references in `explanation`.

- [x] **Step 5: Audit canonical object collisions**

  Fail when two IDs target the same value/tree unless manifest explicitly marks one duplicate and only one implementation exists.

- [x] **Step 6: Run content tests to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "CompleteTweakImportAudit|TweakCatalogLoader|CategoryCatalogLoader|TweakListModel|Encyclopedia"`
  Expected: PASS.

### Task 15: Импорт задач, BCD, питания, Features/Capabilities, AppX, ViVe и файлов

**Files:**
- Create/Modify: `content/tweaks/**/*.yaml` according to remaining accepted candidates
- Modify: `docs/audits/complete-tweak-import.json`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Modify: `tests/content/CompleteTweakImportAuditTest.cpp`
- Modify: operation-family tests for representative real definitions

**Interfaces:**
- Consumes: all typed operation families and reviewed manifest.
- Produces: complete catalog for every non-registry accepted candidate and zero undecided manifest entries.

- [x] **Step 1: Add failing exact-coverage tests**

  Assert every `accepted` manifest ID exists, every implementation uses the declared object type, no `service-management` candidate exists in catalog and no undecided candidate remains. Проверить точную совместимость по object types: `scheduled_task`, `bcd`, `power`, `windows_feature`, `windows_capability`, `file`, `appx` и `feature_configuration`.

- [x] **Step 2: Run coverage tests to verify RED**

  Run: `pwsh .\tools\test.ps1 -Regex "CompleteTweakImportAudit|TweakCatalogLoader"`
  Expected: FAIL listing remaining accepted IDs.

- [x] **Step 3: Validate and add scheduled-task definitions**

  Include component detection and exact task paths; omit creation/deletion requests with an explicit manifest reason.

- [x] **Step 4: Validate and add BCD and power definitions**

  Document measurable effect and defaults by build; reject folklore values without a stable meaning.

- [x] **Step 5: Validate and add Optional Feature/Capability and file definitions**

  Include exact component name, restart requirement, fixed file destination and input constraints. Для AppX использовать существующий `appx.remove`; для ViVe принимать только функции с проверенным пользовательским эффектом и существующий тип `feature.configuration`.

- [x] **Step 6: Run complete content coverage to verify GREEN**

  Run: `pwsh .\tools\test.ps1 -Regex "CompleteTweakImportAudit|TweakCatalogLoader|CompatibilityEvaluator|PlanBuilder"`
  Expected: PASS; manifest decisions complete.

### Task 16: Полная регрессия, отчёт, упаковка и единый коммит

**Files:**
- Create: `docs/testing/complete-tweak-import-results.md`
- Modify: `README.md`
- Modify: `docs/audits/complete-tweak-import.json` only if verification exposes a correction
- Include: all files from Tasks 1–15 and the previously completed settings/logging/dropdown changes still present in the worktree

**Interfaces:**
- Consumes: complete implementation and content.
- Produces: verified `dist/Tweakopedia.exe`, final counts/checksums/report and one Git commit.

- [x] **Step 1: Run source consistency checks**

  Run: `git diff --check`
  Run the inventory tool again against the fixed source roots and compare normalized candidate keys with checked-in manifest.
  Expected: no whitespace errors and no candidate missing from manifest.

- [x] **Step 2: Run the entire test suite**

  Run: `pwsh .\tools\test.ps1`
  Expected: 100% tests passed, 0 failed; record actual count.

- [x] **Step 3: Build and verify the portable container**

  Run: `pwsh .\tools\package.ps1`
  Run: `pwsh .\tools\verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`
  Expected: release build, single-file layout and isolated self-check PASS.

- [x] **Step 4: Perform final manifest/catalog audit**

  Record total candidates, accepted tweaks, duplicates and rejected reason counts. Verify no service operation/category and no arbitrary command payload exists.

- [x] **Step 5: Write the result report and update README**

  Include actual catalog count, operation-family counts, test count, EXE size, container SHA-256, payload SHA-256 and known limitations.

- [x] **Step 6: Re-run final verification after documentation changes**

  Run: `pwsh .\tools\test.ps1`
  Run: `git diff --check`
  Expected: 100% tests passed and clean diff check.

- [x] **Step 7: Create the only implementation commit**

  Run:
  ```powershell
  git add README.md apps content docs src tests tools
  git commit -m "feat: complete typed tweak catalog import"
  ```
  Expected: exactly one new commit for the complete import work; do not push or publish a release.
