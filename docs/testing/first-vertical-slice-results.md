# Результаты проверки первого вертикального среза

Дата: 2026-10-08  
Проверенный commit до обновления отчёта: `b06e3201c4b80476862a8aef8977d71e11cd72b2`  
Portable-каталог: `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia`

## Среда

- ОС: Windows 10 Pro, x64.
- EditionID: `Professional`.
- Build: `19045.7725`.
- Qt: `6.8.3`.
- MinGW: `13.1.0` x64.
- CMake: `3.30.5`.
- Ninja: `1.12.1`.

## Автоматические результаты

| Проверка | Результат | Доказательство |
|---|---:|---|
| Чистая debug-сборка | PASS | `tools/build.ps1 -Preset mingw-debug -Clean`; 189 build-шагов, ошибок и предупреждений компилятора проекта нет |
| Полный набор тестов | PASS | 23/23, skipped 0, 2.27 s |
| Release-упаковка | PASS | `tools/package.ps1 -Preset mingw-release` |
| Структура portable | PASS | оба EXE, Qt DLL, QML, `qwindows`, `qsqlite`, YAML; исходники и абсолютные build-пути отсутствуют |
| Self-check без Qt в PATH | PASS | `Tweakopedia.exe --self-check --no-elevation`, exit code 0 |
| Сквозной fake-сценарий | PASS | YAML → missing/disabled → queue enabled → preview → snapshot → DWORD 1 → verify → history → rollback → missing |
| Отказы | PASS | UAC cancel, изменившийся DWORD, пустой план, тайм-аут Executor, незавершённая running-транзакция и failed result не получают статус success |

Сообщение CMake `WrapVulkanHeaders` относится к необязательному компоненту Qt. Tweakopedia не использует Vulkan; предупреждений исходного кода при чистой сборке нет.

## SHA-256 артефактов

| Файл | SHA-256 |
|---|---|
| `Tweakopedia.exe` | `D8B03DF1A4B98BF8A2B2A289C83B16DE7E84533E6E56BFF92E1B070953768FD0` |
| `Tweakopedia.Executor.exe` | `DC356F1A6CD711240133CBB0780188D1746B81BEFA02E65CFA3B62D9BA73AAAB` |
| `content/tweaks/filesystem/win32-long-paths.yaml` | `FFAB958A3BB32D068A18EC8B820C76A394F38023BDACF3957FB64CA6330F099E` |

## Критерии раздела 23

| Критерий | Статус | Доказательство |
|---|---:|---|
| Запуск из portable-каталога | PASS | package self-check с локальными runtime-файлами |
| Определение Windows 10/11 x64 и build | PASS | `WindowsSystemProfile` + self-check на Windows 10 x64 build 19045 |
| Загрузка реального YAML-твика | PASS | `TweakCatalogLoader`, `ContentCatalog`, self-check |
| Отображение в каталоге | PASS | `TweakListModel`, QML shell smoke |
| Определение текущего состояния | PASS | `RegistryDwordStateDetector`, `WindowsRegistryBackend` |
| Объяснение через `?` | PASS | `QmlPlanPreviewHistory` (`TweakRow`, `ExplanationDrawer`) |
| Добавление только в очередь | PASS | `AppController`, `TweakQueue` |
| Предварительный план | PASS | `PlanBuilder`, `PlanPreview`, `VerticalSliceWorkflow` |
| Отдельный Executor | PASS | `ExecutorIpc`, `ExecutorLauncher`, package layout |
| Снимок исходного значения | PASS | `TransactionRunner`, `VerticalSliceWorkflow` |
| Применение и контрольное чтение | PASS | `RegistryDwordExecutor`, `TransactionRunner` |
| История транзакции | PASS | `TransactionRepository`, `VerticalSliceWorkflow`, QML history |
| Полный возврат исходного значения | PASS на fake и unit backend | missing, DWORD 0/1 и другой native type/bytes; реальный HKLM указан ниже |

## Ручной HKLM-сценарий

Статус: **не выполнен**.

Причины:

- сценарий изменяет реальный `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled` и требует интерактивного подтверждения UAC;
- в текущей сессии доступна Windows 10 x64, но нет отдельной среды Windows 11 x64;
- автоматические fake/unit-тесты не выдаются за проверку двух реальных ОС.

Для закрытия используются шаги из `docs/testing/first-vertical-slice-checklist.md`. В отчёт после выполнения добавляются только итог PASS/FAIL, версия ОС и SHA-256 файлов транзакции; посторонние значения реестра не копируются.

## Итог

Первый вертикальный срез собран, автоматическая часть критериев пройдена. Полное Definition of Done ожидает ручной HKLM-сценарий на Windows 10 x64 и Windows 11 x64.
