# Архитектура Tweakopedia

## Процессы

Однофайловый `Tweakopedia.exe` содержит три исполняемых компонента:

- Bootstrap проверяет контейнер, подготавливает версионированный runtime-кэш и
  запускает интерфейс;
- `Tweakopedia.App.exe` содержит Qt Quick-интерфейс, каталог, модели и
  контроллеры;
- `Tweakopedia.Executor.exe` выполняет типизированные операции, которым нужны
  права администратора.

Основной процесс не передаёт Executor произвольные командные строки. Локальный
IPC-сеанс проверяет одноразовый nonce и PID повышенного процесса.

## Транзакции твиков

Выбранные состояния сначала образуют очередь и предварительный план. До записи
Executor повторно сверяет исходное состояние и сохраняет снимок. Поддерживаемые
обратимые операции откатываются в обратном порядке при ошибке пакета. Результат
и снимок остаются в каталоге транзакции и отображаются в «Истории».

## Системные инструменты

Системные инструменты используют тот же проверенный транспорт, но отдельный
контракт сообщений:

```text
SystemIntegrityPage.qml
        ↓
SystemIntegrityController
        ↓
SystemToolSession
        ↓  verified local IPC
Tweakopedia.Executor.exe
        ↓
QProcess → DISM.exe
```

Для проверки разрешены только операции `system_integrity.scan` и
`system_integrity.restore`. Executor сам формирует фиксированную
последовательность `/ScanHealth` или `/RestoreHealth` и завершающий
`/CheckHealth`, всегда с `/Online /Cleanup-Image /English`.

`DismOutputParser` находится в Executor и передаёт интерфейсу типизированное
состояние хранилища. Контроллер не определяет итог повторным разбором текста.
Вывод одновременно идёт по IPC и полностью сохраняется в журнале сеанса; модель
экрана удерживает последние 2 МиБ.

## Границы модулей

- `src/domain` — типы параметров и операций;
- `src/content` — YAML/JSON-каталог и его проверка;
- `src/detection` — чтение фактических состояний Windows;
- `src/planning` — очередь и типизированный план;
- `src/execution` — IPC, исполнители, DISM-workflow и протоколы;
- `src/persistence` — SQLite, настройки и файлы транзакций;
- `src/platform` — Win32, WMI, DXGI, AppX и Feature Store;
- `src/app` — долгоживущие модели и контроллеры QML;
- `apps` — точки входа Bootstrap, GUI, Executor и Packager.

## Portable-контейнер

Packager формирует ZIP-payload и подписанный структурой footer с manifest и
SHA-256. Bootstrap извлекает payload в
`%LOCALAPPDATA%\Tweakopedia\Runtime\<SHA-256>`, проверяет каждый файл и удаляет
свободные устаревшие версии. В каталоге выпуска остаётся только
`Tweakopedia.exe`.
