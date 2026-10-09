# Tweak Subgroup Tabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить проверенные группы твиков, однострочную горизонтальную Fluent-ленту и единообразную системную Fluent-типографику для обычных и HiDPI-дисплеев.

**Architecture:** Существующие `category/subcategory` остаются единственным источником группировки. Content-валидатор проверяет соответствие YAML каталогу категорий, `TweakGroupListModel` строит упорядоченные непустые группы для текущего профиля, а `TweakFilterProxyModel` применяет выбранную группу как дополнительную ось фильтрации. Новый QML-компонент отвечает только за отображение, горизонтальную прокрутку и навигационные стрелки. Отдельный `FluentText` централизует системное семейство Segoe, метрики и нативную растеризацию; иконки и моноширинный текст сохраняют собственные семейства.

**Tech Stack:** C++20, Qt 6.8.3 Core/Gui/QML/Quick/Quick Controls 2, yaml-cpp, Qt Test, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-09-subcategory-tabs-design.md`

## Global Constraints

- Не вводить новый уровень или новое обязательное поле YAML: использовать существующие `category` и `subcategory`.
- Порядок и русские названия групп брать только из `content/categories.yaml`.
- Вкладка «Все» имеет пустой `id` и всегда идёт первой внутри конкретной категории.
- Поисковый запрос не должен менять состав или порядок вкладок.
- Пустые группы скрываются с учётом профиля ОС и флажка скрытия неподдерживаемых твиков.
- Лента скрыта для «Все категории» и до завершения первичного поиска установленных приложений.
- Вкладки располагаются в одну строку без переноса; счётчики в интерфейсе не показываются.
- Кнопки по краям выбирают предыдущую/следующую вкладку, не зацикливаются и отключаются на границах.
- В широком режиме лента ограничена точной шириной колонки карточек и не входит в колонку категорий.
- Использовать системный `Segoe UI Variable`, затем `Segoe UI`, затем системный UI-шрифт Qt; встроенный SF Pro удалить из проекта и EXE.
- Обычный текст использует один режим нативной растеризации и вертикальный hinting; `Segoe MDL2 Assets` и `Consolas` сохраняют собственные семейства.
- Не добавлять внешних зависимостей и не менять светлую Fluent-тему.

## Review Focus

- Одинаковые ID подкатегорий в разных категориях должны оставаться допустимыми и разрешаться только в паре с `category` — закрепить в Task 1.
- Если выбранная группа исчезла после изменения флажка поддержки, модель должна атомарно выбрать «Все» — закрепить в Task 3.
- Переход к твику при активных поиске и скрытии неподдерживаемых должен открыть его категорию и группу — закрепить в Task 4.
- Колесо над лентой не должно одновременно прокручивать вертикальный список карточек — закрепить в Task 5.
- При узкой ширине крайние вкладки должны оставаться достижимыми стрелками и не пересекать соседние области — закрепить в Task 7.
- Regular/medium/semibold/bold должны разрешаться без подмены семейства или синтетического веса, а контрольные снимки должны покрывать DPR 1.0/1.25/1.5/2.0 — закрепить в Task 6.

---

### Task 1: Проверка ссылочной целостности групп каталога

**Files:**
- Create: `src/content/CategoryMembershipValidator.h`
- Create: `src/content/CategoryMembershipValidator.cpp`
- Modify: `src/content/CMakeLists.txt`
- Modify: `src/app/AppController.cpp`
- Create: `tests/content/CategoryMembershipValidatorTest.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `content/categories.yaml` and affected `content/tweaks/**/*.yaml` only when the validator finds real inconsistencies

**Interfaces:**
- Consumes: `content::CategoryCatalog`, `content::TweakCatalog`.
- Produces: `QVector<CatalogError> CategoryMembershipValidator::validate(const CategoryCatalog&, const TweakCatalog&) const` with codes `category.reference_unknown`, `subcategory.reference_unknown`, and `subcategory.empty`.

- [ ] **Step 1: Write failing `CategoryMembershipValidatorTest` cases**

Cover: unknown category; missing/unknown subcategory; configured empty subcategory; the same subcategory ID under two different categories; and the real 1109-file catalog returning no errors.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "CategoryMembershipValidator"`

Expected: FAIL because the target and validator do not exist.

- [ ] **Step 3: Implement the validator and call it during `AppController::startup()`**

Use the pair `(category, subcategory)` as the lookup key. Return all deterministic errors in catalog order; do not mutate either catalog and do not invent missing groups.

- [ ] **Step 4: Correct only inconsistencies reported by the real-catalog test**

Run the generator-independent test after each correction; keep existing IDs stable whenever an appropriate group already exists.

- [ ] **Step 5: Run content tests to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "CategoryMembershipValidator|CategoryCatalogLoader|TweakCatalogLoader|ContentCatalog"`

Expected: all selected tests PASS and the real catalog has complete group coverage.

- [ ] **Step 6: Commit**

```powershell
git add src/content src/app/AppController.cpp tests/content tests/CMakeLists.txt content
git commit -m "feat: validate tweak subgroup membership"
```

### Task 2: Дополнительный фильтр подкатегории

**Files:**
- Modify: `src/app/TweakFilterProxyModel.h`
- Modify: `src/app/TweakFilterProxyModel.cpp`
- Modify: `tests/app/TweakFilterProxyModelTest.cpp`

**Interfaces:**
- Consumes: `TweakListModel::CategoryRole`, `TweakListModel::SubcategoryRole`.
- Produces: `Q_PROPERTY(QString subcategoryId READ subcategoryId WRITE setSubcategoryId NOTIFY subcategoryIdChanged)` and `void setSubcategoryId(QString)`.

- [ ] **Step 1: Add failing filter-composition tests**

Add `filtersByCategoryAndSubcategoryTogether`, `emptySubcategoryMeansAllGroups`, `searchDoesNotChangeSubcategory`, and `duplicateSubcategoryIdsRemainCategoryScoped`. Assert the conjunction of support, category, subcategory, and query filters.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "TweakFilterProxyModel"`

Expected: FAIL because `subcategoryId` and its setter are missing.

- [ ] **Step 3: Implement `subcategoryId` filtering**

Trim incoming IDs, invalidate rows only on change, and filter `SubcategoryRole` only when both the group ID is non-empty and the category is concrete.

- [ ] **Step 4: Run the focused test to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "TweakFilterProxyModel"`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/app/TweakFilterProxyModel.* tests/app/TweakFilterProxyModelTest.cpp
git commit -m "feat: filter tweaks by subgroup"
```

### Task 3: Модель доступных групп

**Files:**
- Create: `src/app/TweakGroupListModel.h`
- Create: `src/app/TweakGroupListModel.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/app/TweakGroupListModelTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `CategoryCatalog`, `TweakCatalog`, `QHash<TweakId, bool> supported`, a concrete `categoryId`, and `hideUnsupported`.
- Produces: roles `IdRole`, `TitleRole`, `CountRole`; `Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectedIdChanged)`; `void reset(const CategoryCatalog&, const TweakCatalog&, const QHash<TweakId, bool>&, QString categoryId, bool hideUnsupported)`; `bool select(QStringView id)`; `QString adjacentId(int delta) const`.

- [ ] **Step 1: Write failing model tests**

Assert: «Все» first; category YAML order; counts; unsupported-only group hidden only when requested; query-independent data; selection preservation; selection reset to «Все» when its group disappears; a category with one non-empty subgroup still exposes both «Все» and that subgroup; non-wrapping `adjacentId(-1/+1)` boundaries.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "TweakGroupListModel"`

Expected: FAIL because the model is absent.

- [ ] **Step 3: Implement `TweakGroupListModel`**

Store a compact vector of `{id, title, count}`. Derive order from the selected `CategoryDefinition`; derive counts from matching tweaks and the support map. Emit a model reset followed by `selectedIdChanged` only when selection actually changes.

- [ ] **Step 4: Run model tests to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "TweakGroupListModel"`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/app/TweakGroupListModel.* src/app/CMakeLists.txt tests/app/TweakGroupListModelTest.cpp tests/CMakeLists.txt
git commit -m "feat: model available tweak subgroups"
```

### Task 4: Координация категории, группы и перехода к твику

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/app/AppControllerTest.cpp`
- Modify: `tests/integration/VerticalSliceWorkflowTest.cpp`

**Interfaces:**
- Consumes: `TweakGroupListModel`, `TweakFilterProxyModel::setSubcategoryId`.
- Produces: `Q_PROPERTY(TweakGroupListModel* tweakGroups READ tweakGroups CONSTANT)`; `Q_INVOKABLE void setTweakSubcategory(const QString&)`; `Q_INVOKABLE void stepTweakSubcategory(int delta)`.

- [ ] **Step 1: Add failing controller and workflow tests**

Assert: selecting a category resets group, selecting a group updates the proxy, leaving and reopening the Tweaks page preserves category/group within the session, arrow stepping stops at boundaries, hiding unsupported refreshes groups and repairs selection, and `revealTweak(id)` clears search/support hiding then selects the tweak's category and subcategory before returning its proxy row.

- [ ] **Step 2: Run controller tests to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "AppController|VerticalSliceWorkflow"`

Expected: FAIL because group properties and actions are missing.

- [ ] **Step 3: Integrate the group model in `AppController`**

Add `refreshTweakGroups()` as the single synchronization point. Call it after startup, category changes, support-filter changes, and app-removal catalog refresh. Keep search changes out of this path.

- [ ] **Step 4: Update `revealTweak` ordering**

Clear query and support hiding, select category, refresh groups, select the tweak's subcategory, then locate the row. Do not expose an intermediate invalid group to QML.

- [ ] **Step 5: Run controller and integration tests to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "AppController|VerticalSliceWorkflow|TweakGroupListModel|TweakFilterProxyModel"`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/app/AppController.* tests/app/AppControllerTest.cpp tests/integration/VerticalSliceWorkflowTest.cpp
git commit -m "feat: coordinate tweak subgroup navigation"
```

### Task 5: Fluent-компонент горизонтальной ленты

**Files:**
- Create: `apps/tweakopedia/qml/components/TweakGroupStrip.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Create: `tests/qml/tst_tweakgroupstrip.qml`

**Interfaces:**
- Consumes: group model roles `id`, `title`, `count`, controller-selected ID.
- Produces: properties `model`, `currentGroup`, signals `groupSelected(string groupId)` and `stepRequested(int delta)`; object names for strip, horizontal list, arrow buttons, and delegates.

- [ ] **Step 1: Write failing QML component tests**

Assert: one horizontal row; pale-blue selected pill; exact Russian labels; circular arrow buttons; boundary disabled states; clicking a pill emits its ID; arrows emit `-1/+1`; selected delegate is brought into view both after direct selection and after a model reset; long titles elide and expose a tooltip.

- [ ] **Step 2: Add a wheel-isolation test**

Place the strip above a vertically flickable sentinel. Send a vertical wheel event over the strip and assert horizontal `contentX` changes while the sentinel vertical offset does not.

- [ ] **Step 3: Run QML tests to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "QmlPlanPreviewHistory"`

Expected: FAIL because `TweakGroupStrip.qml` is absent.

- [ ] **Step 4: Implement `TweakGroupStrip.qml`**

Use a clipped horizontal `ListView`, Fluent pill delegates, circular icon buttons and a local wheel handler. Keep the arrows inside component bounds and reserve no second line.

- [ ] **Step 5: Run QML tests to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "QmlPlanPreviewHistory"`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add apps/tweakopedia/qml/components/TweakGroupStrip.qml apps/tweakopedia/resources.qrc tests/qml/tst_tweakgroupstrip.qml
git commit -m "feat: add Fluent tweak subgroup strip"
```

### Task 6: Нормализация системной Fluent-типографики на обычных и HiDPI-дисплеях

**Files:**
- Delete: `apps/tweakopedia/UiFontLoader.h`
- Delete: `apps/tweakopedia/UiFontLoader.cpp`
- Delete: `apps/tweakopedia/fonts.qrc`
- Delete: `apps/tweakopedia/fonts/SF-Pro.ttf`
- Create: `apps/tweakopedia/UiTypography.h`
- Create: `apps/tweakopedia/UiTypography.cpp`
- Modify: `apps/tweakopedia/CMakeLists.txt`
- Modify: `apps/tweakopedia/main.cpp`
- Modify: `apps/tweakopedia/qml/style/FluentTheme.qml`
- Create: `apps/tweakopedia/qml/components/FluentText.qml`
- Modify: `apps/tweakopedia/qml/components/*.qml`
- Modify: `apps/tweakopedia/qml/pages/*.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Delete: `tests/ui/UiFontLoaderTest.cpp`
- Create: `tests/ui/UiTypographyTest.cpp`
- Modify: `tests/qml/QmlTestMain.cpp`
- Create: `tests/qml/tst_typography.qml`
- Create: `tests/visual/TypographyPreview.qml`
- Modify: `tests/visual/VisualCaptureMain.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tools/capture-ui.ps1`

**Interfaces:**
- `UiTypography::applicationFont()` resolves `Segoe UI Variable`, `Segoe UI`, then the Qt system UI font and returns a configured `QFont` with vertical hinting and typographic line metrics.
- `FluentTheme.fontFamily` reads `Application.font.family` instead of hard-coding a family name.
- `FluentText` derives from `Text` and applies the application family, `Text.NativeRendering`, `Font.PreferVerticalHinting`, and `font.preferTypoLineMetrics: true`.

- [ ] **Step 1: Add failing font-resolution and QML typography tests**

Assert the fallback order, regular/medium/semibold/bold resolution within the chosen family, vertical hinting, typographic line metrics, and the `FluentText` renderer contract. Include Cyrillic, Latin, digits and mixed strings.

- [ ] **Step 2: Add a failing typography audit**

Scan application QML and fail when an ordinary UI `Text` bypasses `FluentText`; allow explicit `Segoe MDL2 Assets` and `Consolas` exceptions. This prevents later pages from silently returning to a different renderer.

- [ ] **Step 3: Run focused tests to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "UiTypography|QmlPlanPreviewHistory"`

Expected: FAIL because the typography contract and `FluentText` are absent.

- [ ] **Step 4: Configure the font and the window renderer**

Resolve the system font in C++, set it as the application font, enable vertical hinting and typographic line metrics, and select native text rendering before loading QML. Apply the same bootstrap in Quick Test and visual-capture entry points. Remove `SF-Pro.ttf`, `fonts.qrc`, and all loader references. Do not alter icon/code fonts.

- [ ] **Step 5: Implement and adopt `FluentText`**

Replace ordinary UI `Text` instances throughout pages and components with `FluentText`, preserving existing sizes, weights, colors, wrapping, elision, and object names. Keep icon and code `Text` instances explicit.

- [ ] **Step 6: Run focused tests to verify GREEN**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "UiTypography|QmlPlanPreviewHistory"`

Expected: PASS, including the QML audit and exact axis checks.

- [ ] **Step 7: Capture typography at four scale factors**

Extend the capture tool to render `TypographyPreview.qml` with `QT_SCALE_FACTOR=1`, `1.25`, `1.5`, and `2`. Inspect regular and semibold Cyrillic strokes, baselines, wrapping and clipping; no manual geometry compensation is allowed per scale.

- [ ] **Step 8: Commit**

```powershell
git add apps/tweakopedia tests tools
git commit -m "fix: normalize Fluent typography"
```

### Task 7: Компоновка страницы, адаптивность и финальная проверка

**Files:**
- Modify: `apps/tweakopedia/qml/pages/TweaksPage.qml`
- Modify: `tests/qml/tst_navigation.qml`
- Modify: `tests/qml/tst_remainingpages.qml`
- Modify: `tests/visual/TweaksCatalogPreview.qml` if present, otherwise create it
- Modify: `tools/capture-ui.ps1`
- Modify: `README.md`

**Interfaces:**
- Consumes: `controller.tweakGroups`, `setTweakSubcategory`, `stepTweakSubcategory`, `TweakGroupStrip`.
- Produces: approved wide and narrow two-column layouts with no category-list gap and no strip overlap.

- [ ] **Step 1: Add failing wide/narrow page tests**

Assert in wide mode: `Все категории` and the strip share the same vertical band; strip left/right equal list-card left/right; selected category begins on the next row without an empty placeholder. Assert in narrow mode: strip remains a single clipped row above cards and every tab is reachable through stepping.

- [ ] **Step 2: Add visibility-state tests**

Assert the strip is hidden and reserves zero height for «Все категории» and the pre-scan app-removal prompt; it is visible for a concrete category with groups.

- [ ] **Step 3: Run QML tests to verify RED**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "QmlPlanPreviewHistory"`

Expected: FAIL on missing page integration and geometry assertions.

- [ ] **Step 4: Integrate the strip into `TweaksPage.qml`**

Use one shared top coordinate for the `Все категории` row and strip. Anchor the strip to `tweakList.left/right`; anchor category rows independently so hiding the strip never creates blank category space. Reset list position after category/group selection.

- [ ] **Step 5: Add deterministic wide and narrow visual captures**

Use 1484×999 for the approved wide composition and the existing narrow test width. Capture one concrete category with an overflowing strip and one hidden-strip state.

- [ ] **Step 6: Update README and run focused tests**

Document subgroup tabs, mouse-wheel/touchpad scrolling and arrow navigation.

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug -Regex "CategoryMembershipValidator|TweakFilterProxyModel|TweakGroupListModel|AppController|UiTypography|QmlPlanPreviewHistory|VerticalSliceWorkflow"`

Expected: PASS.

- [ ] **Step 7: Run the entire regression suite**

Run: `pwsh .\tools\test.ps1 -Preset mingw-debug`

Expected: 100% tests passed, 0 failed.

- [ ] **Step 8: Build and inspect both captures**

Run: `pwsh .\tools\capture-ui.ps1`

Expected: the wide strip matches card width; the category list has no empty top block; the narrow strip clips and scrolls without overlap.

- [ ] **Step 9: Check the diff and commit**

```powershell
git diff --check
git add apps/tweakopedia README.md tests tools
git commit -m "feat: add tweak subgroup navigation"
```
