# План реализации диагностики сети

**Spec:** `docs/superpowers/specs/2026-10-13-network-diagnostics-design.md`

**Цель:** превратить третью карточку «Инструментов» в read-only аудит сетевой
конфигурации с живым выводом и структурированными результатами.

**Архитектура:** существующий `SystemToolSession` отправляет типизированный запрос
повышенному Executor. Новый `NetworkDiagnosticsWorkflow` запускает встроенный
PowerShell-сценарий, отделяет JSON-маркеры от пользовательского вывода и возвращает
результаты через расширенный `SystemToolResult`. Контроллер преобразует `details`
в QML-модель.

## Задача 1. Расширить системный протокол

Файлы:

- `src/execution/SystemToolProtocol.*`
- `src/execution/SystemToolLogWriter.cpp`
- `tests/execution/SystemToolProtocolTest.cpp`
- `tests/execution/SystemToolLogWriterTest.cpp`

Шаги:

1. Добавить падающие тесты round-trip для нового инструмента, операции, этапов и
   `details`, а также каталога `NetworkDiagnostics`.
2. Убедиться, что тесты падают по отсутствующим значениям.
3. Реализовать минимальное расширение протокола и журналов.
4. Запустить целевые тесты и закоммитить.

## Задача 2. Реализовать workflow Executor

Файлы:

- создать `src/execution/NetworkDiagnosticsScript.h`
- создать `src/execution/NetworkDiagnosticsWorkflow.*`
- изменить `src/execution/CMakeLists.txt`
- изменить `apps/executor/main.cpp`
- создать `tests/execution/NetworkDiagnosticsWorkflowTest.cpp`
- изменить `tests/execution/ExecutorIpcTest.cpp`
- изменить `tests/CMakeLists.txt`

Шаги:

1. Тестами зафиксировать точный путь и аргументы PowerShell, UTF-8, потоковый
   парсинг маркеров, шесть результатов, общий итог и ошибки запуска.
2. Убедиться в RED.
3. Добавить компактный фиксированный PowerShell-сценарий и workflow.
4. Подключить dispatch Executor и fixture IPC.
5. Запустить workflow и IPC тесты, затем закоммитить.

## Задача 3. Добавить контроллер приложения

Файлы:

- создать `src/app/NetworkDiagnosticsController.*`
- изменить `src/app/CMakeLists.txt`
- создать `tests/app/NetworkDiagnosticsControllerTest.cpp`
- изменить `tests/CMakeLists.txt`
- изменить `apps/tweakopedia/main.cpp`

Шаги:

1. Тестами зафиксировать начальное, ожидающее UAC, выполняющееся, завершённое и
   ошибочное состояния; результаты, счётчики, журнал и 2-МиБ буфер.
2. Убедиться в RED.
3. Реализовать контроллер и зарегистрировать его QML context property.
4. Запустить целевые тесты и закоммитить.

## Задача 4. Собрать Fluent UI экран

Файлы:

- создать `apps/tweakopedia/qml/pages/NetworkDiagnosticsPage.qml`
- изменить `apps/tweakopedia/qml/pages/ToolsPage.qml`
- изменить `apps/tweakopedia/qml/Main.qml`
- изменить `apps/tweakopedia/resources.qrc`
- создать `tests/qml/tst_networkdiagnosticspage.qml`
- изменить `tests/qml/tst_toolspage.qml`
- создать `tests/visual/NetworkDiagnosticsPagePreview.qml`
- изменить `tests/smoke/BuildSmokeTest.cpp`

Шаги:

1. Добавить падающие QML-тесты маршрута, состояний и кнопок.
2. Убедиться в RED.
3. Реализовать адаптивную страницу, сетку результатов и терминал.
4. Запустить QML/smoke-тесты и визуальные снимки широкого и узкого окна.
5. Закоммитить.

## Задача 5. Документация и выпускной артефакт

Файлы:

- `README.md`
- `docs/architecture.md`
- создать `docs/testing/network-diagnostics-results.md`

Шаги:

1. Обновить пользовательское и архитектурное описание.
2. Выполнить `git diff --check`, полный Debug CTest и Release-сборку.
3. Собрать и проверить единый `dist/Tweakopedia.exe`.
4. Зафиксировать команды, результаты и SHA-256, затем закоммитить.

## Review focus

- фиксированный сценарий не принимает пользовательский ввод;
- read-only команды не переходят к операциям восстановления;
- маркеры не отображаются в терминале и не могут бесконтрольно раздувать результат;
- сбой отдельного блока не уничтожает остальные результаты;
- события общей `SystemToolSession` фильтруются по request ID;
- одинаковая работа на Windows 10 и Windows 11 с Windows PowerShell 5.1.
