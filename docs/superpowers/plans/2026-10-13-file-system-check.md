# File System Check Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить автономную проверку выбранного локального NTFS-тома командой CHKDSK без исправления файловой системы.

**Architecture:** Обобщить существующий типизированный IPC инструментов, добавить Win32-поставщик томов, отдельный workflow CHKDSK и контроллер экрана. Переиспользовать повышенный Executor, watchdog, потоковый вывод и упаковку приложения.

**Tech Stack:** C++20, Qt 6.8 Core/Network/QML/Quick, Win32 volume APIs, CMake/CTest, QML Qt Quick Controls Basic.

**Spec:** `docs/superpowers/specs/2026-10-13-file-system-check-design.md`

## Global Constraints

- Запускать только `chkdsk.exe <буква>:` без исправляющих ключей.
- Принимать целью только локальную букву диска в каноническом формате `^[A-Z]:$`.
- Не разбирать локализованный текст для определения результата; использовать завершение процесса и код выхода.
- Не выполнять реальный CHKDSK в автоматических тестах.
- Сохранять полный сырой вывод в `Logs/Tools/FileSystemCheck`, экранный буфер ограничивать 2 МиБ.
- Не добавлять новые сторонние зависимости.

## Review Focus

- Исчезновение тома между поиском и запуском должно завершать сеанс, а не оставлять экран занятым.
- Код CHKDSK `2` должен отображаться как найденные проблемы, а не как авария процесса.
- Цель с пробелами, путём, UNC или аргументами должна отклоняться до `QProcess`.
- Отмена UAC должна сохранить выбор тома и позволить повторный запуск.
- Поток с `\r`, разделёнными многобайтными фрагментами и объёмом свыше 2 МиБ не должен ломать интерфейс.

---

### Task 1: Generic system-tool contract

**Files:**
- Modify: `src/execution/SystemToolProtocol.h`
- Modify: `src/execution/SystemToolProtocol.cpp`
- Modify: `src/execution/ISystemToolRunner.h`
- Modify: `src/execution/SystemToolSession.h`
- Modify: `src/execution/SystemToolSession.cpp`
- Modify: `src/execution/SystemToolLogWriter.h`
- Modify: `src/execution/SystemToolLogWriter.cpp`
- Modify: DISM controllers/workflow and their tests for renamed generic outcome
- Test: `tests/execution/SystemToolProtocolTest.cpp`
- Test: `tests/execution/SystemToolLogWriterTest.cpp`
- Test: `tests/execution/SystemToolSessionTest.cpp`

**Interfaces:**
- Produces: `SystemToolRequest{requestId, tool, operation, target}`, `SystemToolOutcome`, `ISystemToolRunner::start(SystemToolKind, SystemToolOperation, QString)`.

- [x] Write protocol/session/log tests for tool kind, canonical target, directory choice and preserved DISM behavior.
- [x] Run the focused tests and confirm failure on missing generic fields and APIs.
- [x] Implement the generic contract and update DISM consumers minimally.
- [x] Run focused tests and the existing DISM workflow/controller tests; expect all to pass.
- [x] Commit as `refactor: generalize system tool execution contract`.

### Task 2: Native volume discovery

**Files:**
- Create: `src/platform/LocalVolume.h`
- Create: `src/platform/ILocalVolumeProvider.h`
- Create: `src/platform/WindowsLocalVolumeProvider.h`
- Create: `src/platform/WindowsLocalVolumeProvider.cpp`
- Modify: `src/platform/CMakeLists.txt`
- Create: `tests/platform/WindowsLocalVolumeProviderTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `QVector<LocalVolume> ILocalVolumeProvider::volumes() const`, with root, target, label, file system, total/free bytes and system-volume flag.

- [x] Write tests against an injected Win32-query seam for filtering, canonicalization and system-first sorting.
- [x] Run the focused test and confirm the provider is missing.
- [x] Implement the provider with Win32 volume APIs and no filesystem mutation.
- [x] Run focused and platform tests; expect all to pass.
- [x] Commit as `feat: discover local NTFS volumes`.

### Task 3: CHKDSK workflow and executor routing

**Files:**
- Create: `src/execution/FileSystemCheckWorkflow.h`
- Create: `src/execution/FileSystemCheckWorkflow.cpp`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `apps/executor/main.cpp`
- Create: `tests/execution/FileSystemCheckWorkflowTest.cpp`
- Modify: `tests/execution/ExecutorIpcTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: generic request/outcome from Task 1.
- Produces: `FileSystemCheckWorkflow::start(const SystemToolRequest&)` and executor dispatch by `SystemToolKind`.

- [x] Write tests proving the exact `chkdsk.exe C:` invocation and mappings for exit codes `0`, `2`, `3`, `1`, invalid target and failed start.
- [x] Run focused tests and confirm failure because the workflow/route does not exist.
- [x] Implement workflow, streaming, logs and fixed executor routing.
- [x] Add fixture IPC coverage for `FileSystem` and run workflow/IPC tests; expect all to pass.
- [x] Commit as `feat: execute read-only CHKDSK checks`.

### Task 4: File-system check controller

**Files:**
- Create: `src/app/FileSystemCheckController.h`
- Create: `src/app/FileSystemCheckController.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `apps/tweakopedia/main.cpp`
- Create: `tests/fakes/FakeLocalVolumeProvider.h`
- Create: `tests/app/FileSystemCheckControllerTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ILocalVolumeProvider`, generic runner, existing desktop file actions.
- Produces: QML properties for volumes, selection, state, output/result and invokables `refreshVolumes`, `startCheck`, `copyOutput`, `openSessionLog`.

- [x] Write controller tests for system-first default, selection, target forwarding, UAC cancellation, exit outcomes, no-volume state, 2-MiB truncation and session log actions.
- [x] Run focused test and confirm failure because controller is missing.
- [x] Implement controller and wire it into the app context.
- [x] Run controller plus DISM controller/session tests; expect all to pass.
- [x] Commit as `feat: model file system checks in the application`.

### Task 5: Fluent CHKDSK screen

**Files:**
- Create: `apps/tweakopedia/qml/pages/FileSystemCheckPage.qml`
- Modify: `apps/tweakopedia/qml/pages/ToolsPage.qml`
- Modify: `apps/tweakopedia/qml/Main.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Create: `tests/qml/tst_filesystemcheckpage.qml`
- Modify: `tests/qml/tst_toolspage.qml`
- Create: `tests/visual/FileSystemCheckPagePreview.qml`

**Interfaces:**
- Consumes: `fileSystemCheckController` from Task 4.
- Produces: dedicated second tools screen with responsive Fluent layout and live terminal.

- [x] Write QML tests for navigation, volume selection, disabled states, primary action, result actions and narrow layout bounds.
- [x] Run QML tests and confirm failure because the page is absent.
- [x] Implement the page and replace the second placeholder route.
- [x] Run all QML tests and capture wide/narrow previews; expect no warnings or overflow.
- [x] Commit as `feat: add Fluent file system check screen`.

### Task 6: Documentation, packaging and verification

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture.md`
- Create: `docs/testing/file-system-check-results.md`

**Interfaces:**
- Consumes: completed Tasks 1–5.

- [x] Document behavior, limits, log location and manual scenarios.
- [x] Run the complete Debug CTest suite and record the exact result.
- [x] Build Release, create the single EXE and run `tools/verify-package.ps1`.
- [x] Record container/payload hashes and manual-test limitations.
- [x] Commit as `docs: document file system check tool`.
