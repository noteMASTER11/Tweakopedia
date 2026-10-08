# Проверка единого EXE-контейнера

Автоматический сценарий запускается командой:

```powershell
pwsh -NoProfile -File tools/verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe
```

Сценарий проверяет:

- единственный файл `Tweakopedia.exe` в `dist`;
- footer, digest и manifest payload;
- запуск без Qt и MinGW в `PATH`;
- одновременный первый запуск двух копий и повторное использование runtime;
- отсутствие незавершённых staging-каталогов;
- восстановление изменённого файла runtime;
- удаление свободной старой версии;
- сохранение занятой старой версии и её удаление после освобождения lease;
- размещение всех временных и пользовательских данных в изолированном каталоге на `D:`.

Ручные проверки Windows 10/11 и реального HKLM остаются в `first-vertical-slice-checklist.md`.
