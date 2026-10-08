# Single EXE Container Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Преобразовать проверенный portable-каталог Tweakopedia в один распространяемый `Tweakopedia.exe`, который атомарно готовит версионированный runtime-кэш, запускает GUI и удаляет старые неиспользуемые версии.

**Architecture:** Нативный Win32 bootstrap без Qt содержит ZIP/Deflate payload как PE overlay и проверяемый footer. Общая plain-C++ библиотека отвечает за footer, manifest, SHA-256, извлечение и runtime-кэш; build-time packager использует ту же библиотеку для создания контейнера. Qt GUI и отдельный повышенный Executor остаются динамическими файлами внутри payload.

**Tech Stack:** C++20, Win32, CNG SHA-256, miniz 3.1.2, nlohmann/json 3.12.0, Qt 6.8.3 только для приложения и тестового harness, CMake 3.30.5, Ninja 1.12.1, PowerShell 7.

**Spec:** `docs/superpowers/specs/2026-10-08-fluent-ui-single-exe-design.md`

**Prerequisite:** `docs/superpowers/plans/2026-10-08-fluent-ui.md` полностью выполнен и его Final UI Gate пройден.

## Global Constraints

- Конечный `D:\ChatGPT\Projects\Tweakopedia\dist` содержит ровно один файл `Tweakopedia.exe`.
- Qt остаётся динамически подключаемым внутри payload; статическая сборка Qt не вводится.
- Bootstrap и package core не зависят от Qt.
- `miniz-3.1.2.zip` SHA-256: `F0446D863F9C19926AD9483C523FDC42E42B8D4A6A431D27E09D49C79A140D9A`.
- `nlohmann/json 3.12.0 include.zip` SHA-256: `B8CB0EF2DD7F57F18933997C9934BB1FA962594F701CD5A8D3C2C80541559372`.
- Формат footer имеет magic `TWPKG001` и schema version `1`; integers little-endian.
- Идентификатор runtime — lowercase hex SHA-256 всего ZIP payload.
- Рабочий кэш: `%LocalAppData%\Tweakopedia\Runtime\<payload-sha256>`.
- Постоянные данные: `%LocalAppData%\Tweakopedia\Data`.
- Извлечение всегда идёт в `.staging-<pid>-<random>`, затем выполняется atomic rename.
- Относительный путь manifest должен быть UTF-8, использовать `/`, не быть абсолютным, не содержать `.`/`..`, drive prefix, ADS `:` или пустой segment.
- Старый runtime удаляется только после успешного запуска новой GUI и только при доступном exclusive lease.
- Рядом с контейнером файлы не создаются.
- Test/cache/TEMP overrides при разработке направляются только под `D:\ChatGPT`.
- Каждый task выполняется через RED → GREEN → профильный прогон → отдельный commit.

## Review Focus

- Повреждённый или злонамеренный footer/ZIP: offsets не выходят за размер файла, traversal/duplicate/undeclared entries отклоняются до записи; покрывается Tasks 1–3.
- Одновременный первый запуск двух копий: создаётся один готовый runtime, обе GUI используют его, staging не остаётся; покрывается Task 4.
- Обрыв после частичной распаковки или повреждение готового кэша: следующий запуск восстанавливает runtime без использования неполных файлов; покрывается Tasks 3–4.
- Unicode, пробелы и длинный `%LocalAppData%`, недостаток места или запрет записи: путь передаётся без потери символов, ошибка показывается до запуска Qt и возвращается ненулевой код; покрывается Tasks 4 и 6.
- Старая GUI/Executor ещё работает во время запуска новой версии: занятый runtime не удаляется, а свободная старая версия удаляется; покрывается Task 4 и end-to-end Task 9.

---

## File Structure

- `src/package/PackageTypes.h` — plain structs, error codes and result types.
- `src/package/Sha256.*` — streaming SHA-256 via Windows CNG.
- `src/package/PackageFooter.*` — fixed footer serialization and container inspection.
- `src/package/PayloadManifest.*` — strict JSON manifest parsing/writing.
- `src/package/ZipPayload.*` — deterministic archive creation and checked extraction.
- `src/package/RuntimeLease.*` — exclusive lock for active runtime.
- `src/package/RuntimeCache.*` — staging, validation, publish, recovery and cleanup.
- `src/package/LegacyDataMigrator.*` — one-time root-to-Data migration.
- `apps/bootstrap/main.cpp` — thin `wWinMain`.
- `apps/bootstrap/BootstrapApplication.*` — orchestration with injectable platform boundary.
- `apps/bootstrap/WindowsBootstrapPlatform.*` — LocalAppData, TaskDialog, process launch and waiting.
- `apps/packager/main.cpp` — build-only container creation/inspection tool.
- `cmake/BootstrapDependencies.cmake` — exact pinned dependency archives and hashes.
- `tests/package/*.cpp` — package core and bootstrap unit tests.
- `tests/package/BootstrapProbe.cpp` — child process for concurrency and argument tests.
- `tools/package.ps1` — staging → payload → single EXE pipeline.
- `tools/verify-package.ps1` — isolated first/repeated launch checks.

### Task 1: Pinned native dependencies, SHA-256 and package footer

**Files:**
- Create: `cmake/BootstrapDependencies.cmake`
- Create: `src/package/CMakeLists.txt`
- Create: `src/package/PackageTypes.h`
- Create: `src/package/Sha256.h`
- Create: `src/package/Sha256.cpp`
- Create: `src/package/PackageFooter.h`
- Create: `src/package/PackageFooter.cpp`
- Create: `tests/package/PackageFooterTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `cmake/Dependencies.cmake`
- Modify: `tests/CMakeLists.txt`
- Modify: `tools/toolchain-lock.json`

**Interfaces:**
- Consumes: a seekable container file and pinned dependency URLs.
- Produces: `package::Digest` (`std::array<std::byte, 32>`); `sha256File(const std::filesystem::path&, uint64_t offset = 0, std::optional<uint64_t> length = {}) -> Result<Digest>`; `PackageFooter::encode() -> std::array<std::byte, fixedSize>`; `inspectContainer(path) -> Result<PackageInfo>`.

- [ ] **Step 1: Write failing footer/hash tests**

  Проверить known SHA-256 vector, exact magic/version, footer round trip, too-small file, bad magic, unknown version, offset+size overflow, payload overlap with footer, truncated payload и footer digest mismatch.

- [ ] **Step 2: Run RED test**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex PackageFooter`

  Expected: FAIL, target отсутствует.

- [ ] **Step 3: Pin miniz and nlohmann/json by URL_HASH**

  Root project enables `LANGUAGES C CXX`. `BootstrapDependencies.cmake` uses the exact versions and SHA-256 values from Global Constraints; tests must not silently fetch another version.

- [ ] **Step 4: Implement streaming CNG hash and fixed footer**

  Footer fields: magic[8], `uint32 schemaVersion`, `uint32 flags`, `uint64 payloadOffset`, `uint64 payloadSize`, digest[32]. Reject arithmetic overflow before seeking or hashing.

- [ ] **Step 5: Run package footer and build smoke tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'PackageFooter|BuildSmoke'`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add CMakeLists.txt cmake src/package tests/package/PackageFooterTest.cpp tests/CMakeLists.txt tools/toolchain-lock.json
  git commit -m "feat: define verified package footer"
  ```

### Task 2: Strict payload manifest

**Files:**
- Create: `src/package/PayloadManifest.h`
- Create: `src/package/PayloadManifest.cpp`
- Create: `tests/package/PayloadManifestTest.cpp`
- Modify: `src/package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: UTF-8 JSON bytes and a payload staging tree.
- Produces: `ManifestEntry { std::string path; uint64_t size; Digest sha256; }`; `PayloadManifest::parse(std::string_view) -> Result<PayloadManifest>`; `PayloadManifest::fromDirectory(path) -> Result<PayloadManifest>`; `toCanonicalJson() -> std::string`; `find(path) -> const ManifestEntry*`.

- [ ] **Step 1: Write failing strict-schema tests**

  Проверить canonical ordering, lowercase digest, unknown root/entry fields, duplicate path, absolute/drive/UNC/ADS path, `.`/`..`, repeated slash, backslash, invalid UTF-8, wrong digest length, size overflow and отсутствие обязательных `Tweakopedia.App.exe`, `Tweakopedia.Executor.exe`, `content/categories.yaml`.

- [ ] **Step 2: Run RED test**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex PayloadManifest`

  Expected: FAIL.

- [ ] **Step 3: Implement schema version 1 parser/writer**

  JSON root contains exactly `schema_version` and `files`; entries exactly `path`, `size`, `sha256`. `payload-manifest.json` itself is not listed in `files`, preventing recursive hash definition.

- [ ] **Step 4: Run manifest and footer tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'PayloadManifest|PackageFooter'`

  Expected: PASS.

- [ ] **Step 5: Commit**

  ```powershell
  git add src/package/PayloadManifest.* src/package/CMakeLists.txt tests/package/PayloadManifestTest.cpp tests/CMakeLists.txt
  git commit -m "feat: add strict payload manifest"
  ```

### Task 3: Deterministic ZIP payload and checked extraction

**Files:**
- Create: `src/package/ZipPayload.h`
- Create: `src/package/ZipPayload.cpp`
- Create: `tests/package/ZipPayloadTest.cpp`
- Modify: `src/package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `PayloadManifest`, source directory or package payload slice.
- Produces: `ZipPayload::create(sourceRoot, outputZip, manifest) -> Result<void>`; `ZipPayload::extract(containerPath, PackageInfo, destination) -> Result<void>`.

- [ ] **Step 1: Write failing archive/extraction tests**

  Проверить одинаковые bytes при двух builds одного дерева, fixed timestamps/order, successful extraction, archive entry not declared, missing declared entry, duplicate entry, traversal path, symlink-like external attribute, decompressed size mismatch, digest mismatch, corrupted central directory и aggregate expanded-size limit.

- [ ] **Step 2: Run RED test**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex ZipPayload`

  Expected: FAIL.

- [ ] **Step 3: Implement deterministic writer and extract-to-new-files-only**

  Архив содержит первым `payload-manifest.json`, затем manifest files в ordinal path order. Извлечение создаёт только normal files/directories под уже проверенным destination; каждый файл хешируется до принятия результата.

- [ ] **Step 4: Run package core tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'ZipPayload|PayloadManifest|PackageFooter'`

  Expected: PASS.

- [ ] **Step 5: Commit**

  ```powershell
  git add src/package/ZipPayload.* src/package/CMakeLists.txt tests/package/ZipPayloadTest.cpp tests/CMakeLists.txt
  git commit -m "feat: create and extract verified payloads"
  ```

### Task 4: Atomic runtime cache, concurrency and cleanup

**Files:**
- Create: `src/package/RuntimeLease.h`
- Create: `src/package/RuntimeLease.cpp`
- Create: `src/package/RuntimeCache.h`
- Create: `src/package/RuntimeCache.cpp`
- Create: `tests/package/RuntimeCacheTest.cpp`
- Create: `tests/package/RuntimeCacheProbe.cpp`
- Modify: `src/package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `PackageInfo`, verified `ZipPayload`, LocalAppData override.
- Produces: move-only `RuntimeLease`; `PreparedRuntime { path root; bool reused; RuntimeLease lease; }`; `RuntimeCache::prepare(container, packageInfo) -> Result<PreparedRuntime>`; `cleanupOld(currentDigest) -> CleanupReport`.

- [ ] **Step 1: Write failing cache lifecycle tests**

  Проверить first prepare, reuse without rewriting marker, stale staging recovery, ready marker with wrong digest, missing/corrupt cached file, quarantine then re-extract, two concurrent probes, free old runtime removed, leased old runtime retained, current runtime never selected for removal, Unicode/space path and simulated create/write/rename errors.

- [ ] **Step 2: Run RED test**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex RuntimeCache`

  Expected: FAIL.

- [ ] **Step 3: Implement mutex, staging and atomic publish**

  Mutex name derives only from digest. Ready marker records schema and digest. Publish uses same-volume rename. Any existing final directory is revalidated after mutex acquisition before replacing it.

- [ ] **Step 4: Implement leases and best-effort old-version cleanup**

  Lease is an exclusively opened `.runtime.lock` handle kept until GUI exits. Cleanup sorts only digest-named sibling directories, attempts a nonblocking lease, deletes free old versions and records failures without failing current launch.

- [ ] **Step 5: Run cache and package tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'RuntimeCache|ZipPayload|PayloadManifest|PackageFooter'`

  Expected: PASS including concurrent probe.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/package/Runtime* src/package/CMakeLists.txt tests/package/RuntimeCache* tests/CMakeLists.txt
  git commit -m "feat: manage atomic runtime cache"
  ```

### Task 5: Persistent Data layout and resumable legacy migration

**Files:**
- Create: `src/package/LegacyDataMigrator.h`
- Create: `src/package/LegacyDataMigrator.cpp`
- Create: `tests/package/LegacyDataMigratorTest.cpp`
- Modify: `src/persistence/AppPaths.h`
- Modify: `src/persistence/AppPaths.cpp`
- Modify: `tests/persistence/AppPathsTest.cpp`
- Modify: `src/package/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: legacy `%LocalAppData%\Tweakopedia\{tweakopedia.db,logs,transactions}`.
- Produces: `AppPaths::productRoot()`, `runtimeRoot()`, data root `%LocalAppData%\Tweakopedia\Data`; `LegacyDataMigrator::migrate(productRoot, dataRoot) -> MigrationReport` with `complete`, moved paths and conflicts.

- [ ] **Step 1: Write failing path/migration tests**

  Проверить new root paths, empty legacy root, move all three objects, interrupted partial migration resumes, existing destination is never overwritten, marker after completion и source object remains when its destination conflicts.

- [ ] **Step 2: Run RED tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'AppPaths|LegacyDataMigrator'`

  Expected: FAIL.

- [ ] **Step 3: Implement explicit product/data/runtime paths**

  `dataRootOverride` continues to mean exact Data root for tests/self-check. Default constructor derives product root from `LOCALAPPDATA` and data root as its `Data` child.

- [ ] **Step 4: Implement resumable per-object migration**

  Use same-volume rename when possible and copy-then-verify-then-remove fallback. Conflicts are reported and not overwritten; marker appears only when no legacy object remains pending.

- [ ] **Step 5: Run persistence, package and integration tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'AppPaths|LegacyDataMigrator|Transaction|VerticalSliceWorkflow'`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add src/persistence src/package/LegacyDataMigrator.* src/package/CMakeLists.txt tests/persistence tests/package/LegacyDataMigratorTest.cpp tests/CMakeLists.txt
  git commit -m "feat: separate persistent data from runtime cache"
  ```

### Task 6: Native bootstrap orchestration

**Files:**
- Create: `apps/bootstrap/CMakeLists.txt`
- Create: `apps/bootstrap/main.cpp`
- Create: `apps/bootstrap/BootstrapApplication.h`
- Create: `apps/bootstrap/BootstrapApplication.cpp`
- Create: `apps/bootstrap/IBootstrapPlatform.h`
- Create: `apps/bootstrap/WindowsBootstrapPlatform.h`
- Create: `apps/bootstrap/WindowsBootstrapPlatform.cpp`
- Create: `tests/package/BootstrapApplicationTest.cpp`
- Create: `tests/package/BootstrapArgumentProbe.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: current container path, wide argv, `RuntimeCache`, `LegacyDataMigrator`.
- Produces: `BootstrapApplication::run(const std::filesystem::path& self, std::span<const std::wstring> args) -> int`; `IBootstrapPlatform::launchAndWait(exe, args, workingDirectory) -> Result<uint32_t>`; target `TweakopediaBootstrap` with output name `Tweakopedia.exe`.

- [ ] **Step 1: Write failing orchestration tests with fake platform**

  Проверить inspect failure dialog/code, prepare failure, incomplete migration prevents launch, exact forwarding of Unicode/quoted arguments plus injected `--runtime-root` and `--container-path`, GUI nonzero exit propagation, cleanup only after launch succeeds and no cleanup when process creation fails.

- [ ] **Step 2: Run RED test**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex BootstrapApplication`

  Expected: FAIL.

- [ ] **Step 3: Implement testable orchestration**

  `BootstrapApplication` owns order only; filesystem and process-specific UI go through interfaces. The current runtime lease remains alive for the full child lifetime.

- [ ] **Step 4: Implement Windows wide-character entry/platform**

  Use `wWinMain`, `GetModuleFileNameW`, `CommandLineToArgvW`, `CreateProcessW`, explicit Windows quoting, child working directory = runtime root, `WaitForSingleObject`, `GetExitCodeProcess`, native TaskDialog with MessageBox fallback.

- [ ] **Step 5: Run bootstrap, argument probe and package core tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'BootstrapApplication|BootstrapArgumentProbe|RuntimeCache|PackageFooter'`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add apps/bootstrap CMakeLists.txt tests/package/Bootstrap* tests/CMakeLists.txt
  git commit -m "feat: add native Tweakopedia bootstrap"
  ```

### Task 7: Runtime-aware GUI and Executor layout

**Files:**
- Modify: `apps/tweakopedia/CMakeLists.txt`
- Modify: `apps/tweakopedia/main.cpp`
- Modify: `apps/executor/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/execution/ExecutorIpcTest.cpp`
- Modify: `tests/integration/VerticalSliceWorkflowTest.cpp`
- Modify: `tests/smoke/BuildSmokeTest.cpp`

**Interfaces:**
- Consumes: bootstrap arguments `--runtime-root`, `--container-path`; existing `AppPaths` Data root.
- Produces: payload executables `Tweakopedia.App.exe` and `Tweakopedia.Executor.exe`; GUI resolves content and Executor strictly under validated runtime root.

- [ ] **Step 1: Write failing target/path tests**

  Проверить target filenames, accepted runtime root equal to application directory, rejection of mismatched injected runtime root, content lookup and Executor launch path containing Unicode/spaces. Existing test-mode IPC remains operational.

- [ ] **Step 2: Run RED tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'BuildSmoke|ExecutorIpc|VerticalSliceWorkflow'`

  Expected: FAIL на старом `Tweakopedia.exe` target/arguments.

- [ ] **Step 3: Rename only payload GUI output and parse bootstrap arguments**

  CMake target may remain `TweakopediaAppExecutable`, but `OUTPUT_NAME` is `Tweakopedia.App`. The public container owns name `Tweakopedia`. Development launch without bootstrap defaults runtime root to `applicationDirPath()`.

- [ ] **Step 4: Keep Executor boundary unchanged**

  Resolve `Tweakopedia.Executor.exe` via runtime root; do not merge elevated mode into GUI or bootstrap. Preserve nonce/PID/hash validation and timeout behavior.

- [ ] **Step 5: Run full execution/app suite**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex 'BuildSmoke|Executor|AppController|VerticalSliceWorkflow'`

  Expected: PASS.

- [ ] **Step 6: Commit**

  ```powershell
  git add apps/tweakopedia apps/executor CMakeLists.txt tests/execution tests/integration tests/smoke
  git commit -m "feat: make payload runtime-aware"
  ```

### Task 8: Build-time packager and one-file release pipeline

**Files:**
- Create: `apps/packager/CMakeLists.txt`
- Create: `apps/packager/main.cpp`
- Create: `tests/package/PackagerCliTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `cmake/DeployQt.cmake`
- Modify: `tools/package.ps1`
- Modify: `tests/package/PackageLayoutTest.ps1`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: bootstrap EXE and verified portable staging directory.
- Produces: `Tweakopedia.Packager.exe create --bootstrap <path> --payload-root <dir> --output <exe>` and `inspect --container <exe> --json`; final `dist\Tweakopedia.exe`.

- [ ] **Step 1: Write failing packager CLI/layout tests**

  Проверить create/inspect round trip, обязательные GUI/Executor/content/category/license files в manifest, deterministic identical output, refusal to overwrite input/bootstrap, exact output path validation, and PowerShell layout test requiring exactly one regular file named `Tweakopedia.exe`.

- [ ] **Step 2: Run RED tests**

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug -Regex PackagerCli`

  Expected: FAIL.

- [ ] **Step 3: Implement packager CLI using shared package core**

  Create copies bootstrap bytes, appends deterministic ZIP, then encoded footer. Inspect emits schema, payload digest/size and manifest file list; it never extracts.

- [ ] **Step 4: Convert package.ps1 to staging then container**

  Staging root is exactly `D:\ChatGPT\Temp\Tweakopedia\package-staging`; release build is clean; `windeployqt` targets `Tweakopedia.App.exe`; Qt/LGPL and third-party notices are copied into `licenses`; final dist is replaced only after container passes inspect and one-file layout checks.

- [ ] **Step 5: Run release packaging**

  Run: `pwsh -NoProfile -File tools/package.ps1 -Preset mingw-release`

  Expected: PASS; `Get-ChildItem D:\ChatGPT\Projects\Tweakopedia\dist` returns only `Tweakopedia.exe`.

- [ ] **Step 6: Commit**

  ```powershell
  git add apps/packager CMakeLists.txt cmake/DeployQt.cmake tools/package.ps1 tests/package tests/CMakeLists.txt
  git commit -m "build: package Tweakopedia as one executable"
  ```

### Task 9: Isolated end-to-end container verification

**Files:**
- Modify: `tools/verify-package.ps1`
- Create: `docs/testing/single-exe-checklist.md`
- Create: `docs/testing/single-exe-results.md`
- Modify: `docs/testing/first-vertical-slice-checklist.md`

**Interfaces:**
- Consumes: final `dist\Tweakopedia.exe` and packager inspect target from the release build.
- Produces: repeatable isolated first-launch/reuse/update verification with evidence and hashes.

- [ ] **Step 1: Extend verification script before implementation claims**

  Process-scoped `LOCALAPPDATA`, TEMP and TMP point to `D:\ChatGPT\Temp\Tweakopedia\single-exe-self-check`; Qt/toolchain paths are removed from PATH; Qt plugin/QML env vars are cleared.

- [ ] **Step 2: Verify first and repeated self-check launches**

  Run container twice with `--self-check --no-elevation`. Assert exit 0, one runtime digest directory, complete marker/manifest, unchanged marker timestamp on second run and no staging directories.

- [ ] **Step 3: Verify repair and cleanup scenarios**

  Corrupt a copied runtime file under the isolated root and assert re-extraction. Create a valid unused old runtime fixture and assert removal. Hold its lease with `RuntimeCacheProbe` and assert it is retained until the next launch.

- [ ] **Step 4: Run clean debug build and complete tests**

  Run: `pwsh -NoProfile -File tools/build.ps1 -Preset mingw-debug -Clean`

  Run: `pwsh -NoProfile -File tools/test.ps1 -Preset mingw-debug`

  Expected: clean build PASS; all tests PASS with zero skipped.

- [ ] **Step 5: Rebuild and verify final release**

  Run: `pwsh -NoProfile -File tools/package.ps1 -Preset mingw-release`

  Run: `pwsh -NoProfile -File tools/verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`

  Expected: one-file layout PASS; isolated first/repeated/repair/cleanup self-check PASS.

- [ ] **Step 6: Record exact evidence**

  `single-exe-results.md` records commit, Windows build, tool versions, test count, container SHA-256, payload SHA-256, first/repeated launch result and any manual Windows 10/11 gaps without presenting an unrun scenario as PASS.

- [ ] **Step 7: Commit**

  ```powershell
  git add tools/verify-package.ps1 docs/testing
  git commit -m "test: verify single-exe Tweakopedia release"
  ```

## Final Container Gate

- [ ] `git diff --check` passes.
- [ ] Clean debug build passes.
- [ ] Full CTest passes with zero failures and zero skipped.
- [ ] Clean release package contains only `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`.
- [ ] Container starts with Qt removed from PATH and no installed runtime dependency.
- [ ] First launch, cache reuse, repair, concurrent launch and old-version cleanup pass in the D:-scoped isolated root.
- [ ] Container contains no absolute source/build paths in footer, manifest, ZIP entries or extracted binaries.
- [ ] Existing queue → preview → Executor → snapshot → apply → history → rollback workflow passes.
- [ ] Manual real-HKLM checks remain reported separately for Windows 10 x64 and Windows 11 x64.
