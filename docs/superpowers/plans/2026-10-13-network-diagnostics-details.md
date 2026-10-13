# Network Diagnostics Details Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Сделать результаты диагностики сети кликабельными, показать полную детализацию и добавить подтверждаемое удаление выбранных правил Windows Firewall.

**Architecture:** Диагностический workflow расширяет структурированный результат элементами правил. Отдельный типизированный workflow Executor резервирует политику и удаляет ровно одно правило по внутреннему имени. Контроллер хранит диагностику и состояние действия раздельно, QML открывает Fluent-панель подробностей.

**Tech Stack:** C++20, Qt 6.8.3 Core/QML/Quick Controls, Windows PowerShell 5.1, Windows Firewall cmdlets, CTest/Qt Test.

**Spec:** `docs/superpowers/specs/2026-10-13-network-diagnostics-details-design.md`

## Global Constraints

- Windows 10/11 x64, portable one-file distribution.
- Сценарии встроены и не исполняют пользовательский PowerShell-текст.
- Изменение требует отдельного подтверждения в GUI и повышения прав.
- Сбой удаления не стирает последний диагностический результат.
- Все временные, сборочные и тестовые файлы размещаются на `D:\ChatGPT`.

## Review Focus

- Ключ правила с кавычками или PowerShell-метасимволами остаётся данными, а не кодом — покрыть workflow-тестом.
- Неудачный экспорт `.wfw` не должен переходить к удалению — покрыть ошибочным завершением workflow.
- CLIXML с маркером посередине строки не должен терять результат — покрыть диагностическим workflow-тестом.
- События чужого request ID не должны завершать действие — покрыть тестом контроллера.
- Закрытие и повторное открытие панели не должно терять выбранные данные — покрыть QML-тестом.

---

### Task 1: Расширить структурированный результат и очистить CLIXML

**Files:**
- Modify: `src/execution/NetworkDiagnosticsScript.h`
- Modify: `src/execution/NetworkDiagnosticsWorkflow.cpp`
- Modify: `tests/execution/NetworkDiagnosticsWorkflowTest.cpp`

**Interfaces:**
- Produces: `checks[firewall].items[]` с ключом и сетевыми полями.

- [ ] Написать падающие тесты структурированных правил и маркера внутри CLIXML.
- [ ] Запустить `NetworkDiagnosticsWorkflowTest` и зафиксировать ожидаемый RED.
- [ ] Добавить сбор объектов правил, подавление Progress и ограниченный парсинг `items`.
- [ ] Запустить целевой тест до GREEN.
- [ ] Закоммитить `feat: expose detailed network findings`.

### Task 2: Добавить типизированное удаление правила

**Files:**
- Modify: `src/execution/SystemToolProtocol.h`
- Modify: `src/execution/SystemToolProtocol.cpp`
- Create: `src/execution/FirewallRuleRemovalWorkflow.h`
- Create: `src/execution/FirewallRuleRemovalWorkflow.cpp`
- Modify: `src/execution/CMakeLists.txt`
- Modify: `src/execution/SystemToolLogWriter.cpp`
- Modify: `apps/executor/main.cpp`
- Modify: `tests/execution/SystemToolProtocolTest.cpp`
- Create: `tests/execution/FirewallRuleRemovalWorkflowTest.cpp`
- Modify: `tests/execution/ExecutorIpcTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SystemToolRequest{NetworkDiagnostics, RemoveFirewallRule, key}`.
- Produces: результат с `details.rule_key` и `details.backup_path`.

- [ ] Написать падающие тесты протокола, фиксированной команды, Base64-ключа, резервного пути и ошибок.
- [ ] Запустить целевые тесты и зафиксировать RED.
- [ ] Реализовать `FirewallRuleRemovalWorkflow::start(const SystemToolRequest&)` и dispatch Executor.
- [ ] Запустить protocol/workflow/IPC тесты до GREEN.
- [ ] Закоммитить `feat: remove selected firewall rules`.

### Task 3: Смоделировать действие в контроллере

**Files:**
- Modify: `src/app/NetworkDiagnosticsController.h`
- Modify: `src/app/NetworkDiagnosticsController.cpp`
- Modify: `tests/app/NetworkDiagnosticsControllerTest.cpp`

**Interfaces:**
- Produces: свойства `actionState`, `actionMessage`, `activeRuleKey`, `removedRuleKeys`; метод `removeFirewallRule(QString)`.

- [ ] Написать падающие тесты проверки ключа, запуска, успеха, ошибки и изоляции request ID.
- [ ] Запустить `NetworkDiagnosticsControllerTest` и зафиксировать RED.
- [ ] Реализовать отдельное состояние действия без очистки `checks`.
- [ ] Запустить тест до GREEN.
- [ ] Закоммитить `feat: control firewall rule removal`.

### Task 4: Собрать Fluent-панель подробностей

**Files:**
- Modify: `apps/tweakopedia/qml/pages/NetworkDiagnosticsPage.qml`
- Modify: `tests/qml/tst_networkdiagnosticspage.qml`
- Modify: `tests/visual/NetworkDiagnosticsPagePreview.qml`

**Interfaces:**
- Consumes: расширенные `checks` и action API контроллера.

- [ ] Написать падающие QML-тесты клика карточки, подробностей, подтверждения и состояния действия.
- [ ] Запустить QML-набор и зафиксировать RED.
- [ ] Заменить делегат карточки на `AbstractButton`, добавить правую панель и диалог подтверждения.
- [ ] Запустить QML-набор до GREEN и проверить wide/narrow снимки.
- [ ] Закоммитить `feat: browse network diagnostic details`.

### Task 5: Документация и артефакт

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture.md`
- Modify: `docs/testing/network-diagnostics-results.md`

- [ ] Обновить пользовательское и архитектурное описание.
- [ ] Выполнить `git diff --check`, полный Debug CTest и Release-сборку.
- [ ] Собрать и проверить `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`.
- [ ] Записать результаты и SHA-256, выполнить финальный просмотр и закоммитить.
