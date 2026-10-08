# Tweakopedia Encyclopedia Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Заменить пустой раздел «Справочник» на Fluent UI-раздел «Твикопедия» с деревом тем, локальным поиском, статьями из YAML и автоматически связанными материалами.

**Architecture:** Новые C++-модели строят иерархию и представление статьи непосредственно из `TweakCatalog` и `CategoryCatalog`; `EncyclopediaController` управляет поиском, выбором и локальной историей. QML получает только структурированные данные и реализует утверждённую компоновку № 2 без второй копии контента.

**Tech Stack:** C++20, Qt 6.8.3 Core/Qml/Quick/QuickControls2/Test, QML, CMake, Qt Test, Qt Quick Test.

**Spec:** `docs/superpowers/specs/2026-10-08-tweakopedia-encyclopedia-design.md`

## Global Constraints

- Windows 10/11 x64; ARM64 не поддерживается.
- Приложение и энциклопедия работают офлайн.
- YAML-каталог остаётся единственным источником статей.
- Пустые секции статьи не отображаются.
- Фильтр неподдерживаемых твиков не влияет на «Твикопедию».
- Новые QML-файлы включаются в существующий единый EXE-контейнер.
- Интерфейс использует существующие `FluentTheme`, SF Pro, логотип и брейкпоинты.
- Существующие экраны твиков, очереди и истории не меняют поведение.

## Review Focus

- Запрос с разным регистром, лишними пробелами и пунктуацией должен находить ту же статью — проверяет Task 2.
- Твик с неизвестной категорией не должен исчезнуть или разрушить дерево — проверяет Task 2 через резервную ветвь «Другие материалы».
- Выбранная статья, скрытая новым поиском, должна оставаться открытой без ложного выделения в дереве — проверяет Task 4.
- Циклические связи и несколько причин релевантности не должны создавать повторные связанные карточки — проверяет Task 3.
- Длинные заголовки и технические пути не должны обрезать полотно на узкой ширине — проверяет Task 6.

---

### Task 1: Сохранить связи твиков из YAML

**Files:**
- Modify: `src/domain/TweakDefinition.h`
- Modify: `src/domain/TweakDefinition.cpp`
- Modify: `src/content/TweakCatalogLoader.cpp`
- Modify: `tests/content/TweakCatalogLoaderTest.cpp`
- Create: `tests/fixtures/content/valid/related-tweaks.yaml`
- Create: `tests/fixtures/content/invalid/unknown-relation.yaml`

**Interfaces:**
- Consumes: существующие YAML-поля `dependencies` и `conflicts`.
- Produces: `TweakDefinition::dependencies` и `TweakDefinition::conflicts` типа `QVector<TweakId>`; загрузчик отклоняет некорректные и неизвестные ID связей.

- [ ] **Step 1: Написать падающие тесты загрузчика**

  Добавить проверки `loadsDependencyAndConflictIds()` и
  `rejectsUnknownRelationId()`: валидная фикстура сохраняет точные ID в двух
  векторах, несуществующая ссылка даёт ошибку `relation.unknown`.

- [ ] **Step 2: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target TweakCatalogLoaderTest; ctest --test-dir build/mingw-debug -R TweakCatalogLoader --output-on-failure`

  Expected: FAIL, потому что `TweakDefinition` ещё не хранит связи.

- [ ] **Step 3: Реализовать хранение и проверку связей**

  Добавить поля `QVector<TweakId> dependencies` и `conflicts`, функцию разбора
  последовательности ID и межфайловую проверку существования ссылок после загрузки
  каталога. Пустые массивы остаются допустимыми.

- [ ] **Step 4: Запустить целевой тест и полный набор content/domain**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --test-dir build/mingw-debug -R "TweakDefinition|TweakCatalogLoader|CategoryCatalogLoader" --output-on-failure`

  Expected: PASS.

- [ ] **Step 5: Зафиксировать изменение**

  ```powershell
  git add src/domain/TweakDefinition.* src/content/TweakCatalogLoader.cpp tests/content/TweakCatalogLoaderTest.cpp tests/fixtures/content
  git commit -m "feat: preserve tweak relationships"
  ```

### Task 2: Иерархическое дерево и локальный поиск

**Files:**
- Create: `src/app/EncyclopediaTreeModel.h`
- Create: `src/app/EncyclopediaTreeModel.cpp`
- Create: `tests/app/EncyclopediaTreeModelTest.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `TweakCatalog`, `CategoryCatalog`.
- Produces: `EncyclopediaTreeModel : QAbstractItemModel` с методами
  `reset(const TweakCatalog&, const CategoryCatalog&)`,
  `setQuery(const QString&)`, `query() const`,
  `setSelectedArticleId(const QString&)`; роли `nodeType`, `id`, `title`,
  `summary`, `path`, `depth`, `expanded`, `selected`, `matchScore`.

- [ ] **Step 1: Написать падающие тесты структуры дерева**

  Проверить `buildsCategorySubcategoryArticleHierarchy()`,
  `placesOrphanTweaksUnderOtherMaterials()` и отсутствие повторяющихся article ID.

- [ ] **Step 2: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaTreeModelTest`

  Expected: FAIL, цель или класс отсутствует.

- [ ] **Step 3: Реализовать минимальную иерархическую модель**

  Узлы модели владеют дочерними `std::unique_ptr<Node>` и сохраняют стабильный
  `parent`; индекс использует `internalPointer`. Порядок категорий и статей
  повторяет каталоги. Неизвестные category/subcategory попадают в последнюю ветвь
  «Другие материалы».

- [ ] **Step 4: Запустить тест структуры и подтвердить GREEN**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaTreeModelTest; ctest --test-dir build/mingw-debug -R EncyclopediaTreeModel --output-on-failure`

  Expected: PASS.

- [ ] **Step 5: Добавить падающие тесты поиска и выделения**

  Проверить:

  - точное название выше совпадения основного текста;
  - регистр, повторные пробелы и пунктуация не меняют результат;
  - поиск по ID и объекту реестра;
  - пустые ветви скрываются, родители найденной статьи сохраняются;
  - `selected` установлен ровно у выбранной статьи;
  - очистка запроса восстанавливает полное дерево.

- [ ] **Step 6: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaTreeModelTest; ctest --test-dir build/mingw-debug -R EncyclopediaTreeModel --output-on-failure`

  Expected: FAIL на фильтрации или ранжировании.

- [ ] **Step 7: Реализовать индекс и ранжирование**

  Нормализовать запрос через `toCaseFolded()`, замену пунктуации пробелами и
  `simplified()`. Построить поисковый документ один раз при `reset`; присвоить веса
  по порядку из спецификации и перестраивать только видимое дерево при запросе.

- [ ] **Step 8: Запустить целевой и полный app-набор**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --test-dir build/mingw-debug -R "EncyclopediaTreeModel|TweakListModel|TweakFilterProxyModel" --output-on-failure`

  Expected: PASS.

- [ ] **Step 9: Зафиксировать изменение**

  ```powershell
  git add src/app/EncyclopediaTreeModel.* src/app/CMakeLists.txt tests/app/EncyclopediaTreeModelTest.cpp tests/CMakeLists.txt
  git commit -m "feat: add encyclopedia tree search"
  ```

### Task 3: Конструктор статьи и связанные материалы

**Files:**
- Create: `src/app/EncyclopediaArticleModel.h`
- Create: `src/app/EncyclopediaArticleModel.cpp`
- Create: `tests/app/EncyclopediaArticleModelTest.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: каталоги и сохранённые связи Task 1.
- Produces: `EncyclopediaArticleModel : QObject` с `article: QVariantMap`,
  `hasArticle: bool`, `selectArticle(QString) -> bool`, `clear()`,
  `reset(const TweakCatalog&, const CategoryCatalog&)` и сигналом
  `articleChanged()`.
- `article` содержит `id`, `title`, `summary`, `breadcrumbs`, `sections`,
  `technicalObjects`, `compatibility`, `restart`, `relatedArticles`.

- [ ] **Step 1: Написать падающие тесты конструктора статьи**

  Проверить точный порядок секций, отсутствие пустой секции, локализованные
  хлебные крошки, перенос технического объекта, совместимость и перезапуск.

- [ ] **Step 2: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaArticleModelTest`

  Expected: FAIL, цель или класс отсутствует.

- [ ] **Step 3: Реализовать структурированный конструктор статьи**

  Формировать QVariant-структуру из полей `TweakDefinition`; не генерировать HTML.
  Для registry/AppX/Feature Store использовать единый helper технического объекта.

- [ ] **Step 4: Запустить тест и подтвердить GREEN**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaArticleModelTest; ctest --test-dir build/mingw-debug -R EncyclopediaArticleModel --output-on-failure`

  Expected: PASS.

- [ ] **Step 5: Добавить падающие тесты связанных статей**

  Проверить приоритет явной связи над подкатегорией, затем категорию, общий
  технический объект и значимые слова; максимум шесть элементов; исключение
  текущего ID и дублей; стабильный порядок при одинаковой оценке; циклические
  зависимости не зацикливают вычисление.

- [ ] **Step 6: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaArticleModelTest; ctest --test-dir build/mingw-debug -R EncyclopediaArticleModel --output-on-failure`

  Expected: FAIL на пустом списке или неверном порядке.

- [ ] **Step 7: Реализовать детерминированную оценку связей**

  Использовать сумму фиксированных весов, дедупликацию по `TweakId` и исходный
  индекс каталога как последний ключ сортировки. Не выполнять рекурсивный обход
  графа.

- [ ] **Step 8: Запустить целевой тест и app/content-регрессию**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --test-dir build/mingw-debug -R "EncyclopediaArticleModel|TweakCatalogLoader|AppController" --output-on-failure`

  Expected: PASS.

- [ ] **Step 9: Зафиксировать изменение**

  ```powershell
  git add src/app/EncyclopediaArticleModel.* src/app/CMakeLists.txt tests/app/EncyclopediaArticleModelTest.cpp tests/CMakeLists.txt
  git commit -m "feat: build encyclopedia articles from catalog"
  ```

### Task 4: Контроллер поиска, выбора и истории

**Files:**
- Create: `src/app/EncyclopediaController.h`
- Create: `src/app/EncyclopediaController.cpp`
- Create: `tests/app/EncyclopediaControllerTest.cpp`
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/app/AppControllerTest.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: модели Tasks 2–3 и каталоги `AppController`.
- Produces: `EncyclopediaController` со свойствами `tree`, `article`, `query`,
  `loading`, `canGoBack`, `canGoForward`; invokable-методами `setQuery(QString)`,
  `openArticle(QString)`, `goBack()`, `goForward()`, `clearSearch()`.
- `AppController` предоставляет `Q_PROPERTY(EncyclopediaController* encyclopedia READ encyclopedia CONSTANT)`.

- [ ] **Step 1: Написать падающие тесты контроллера**

  Проверить выбор статьи, синхронизацию `selected` в дереве, переход по связанной
  карточке, назад/вперёд, обрезку будущей истории после нового перехода и отсутствие
  двух одинаковых соседних записей. Неизвестный ID возвращает `false` и переводит
  представление в начальное состояние. Новый поисковый запрос, скрывающий выбранный
  лист, не закрывает статью и снимает видимое выделение дерева.

- [ ] **Step 2: Запустить тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaControllerTest`

  Expected: FAIL, цель или класс отсутствует.

- [ ] **Step 3: Реализовать контроллер и локальную историю**

  Хранить историю как `QVector<QString>` и текущий индекс. Поисковый фильтр не
  закрывает уже открытую статью; если её узел скрыт, у дерева временно нет
  выделенного видимого листа.

- [ ] **Step 4: Запустить тест и подтвердить GREEN**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target EncyclopediaControllerTest; ctest --test-dir build/mingw-debug -R EncyclopediaController --output-on-failure`

  Expected: PASS.

- [ ] **Step 5: Добавить падающие интеграционные проверки AppController**

  Проверить создание свойства, заполнение энциклопедии после `startup()` и
  повторный `reset` после завершения сканирования приложений без влияния флажка
  скрытия неподдерживаемых твиков.

- [ ] **Step 6: Запустить проверку и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target AppControllerTest; ctest --test-dir build/mingw-debug -R AppController --output-on-failure`

  Expected: FAIL на отсутствующем свойстве или пустой модели.

- [ ] **Step 7: Подключить контроллер к AppController**

  Вызывать `encyclopedia_.reset(catalog_, categoryCatalog_)` после успешного
  старта и каждого изменения каталога. Старый `openExplanation()` оставить для
  карточек твиков и очереди.

- [ ] **Step 8: Запустить полный app-набор**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --test-dir build/mingw-debug -R "Encyclopedia|AppController|TweakListModel|TweakFilterProxyModel" --output-on-failure`

  Expected: PASS.

- [ ] **Step 9: Зафиксировать изменение**

  ```powershell
  git add src/app/EncyclopediaController.* src/app/AppController.* src/app/CMakeLists.txt tests/app/EncyclopediaControllerTest.cpp tests/app/AppControllerTest.cpp tests/CMakeLists.txt
  git commit -m "feat: integrate encyclopedia navigation"
  ```

### Task 5: Fluent UI-страница «Твикопедия»

**Files:**
- Create: `apps/tweakopedia/qml/pages/TweakopediaPage.qml`
- Create: `apps/tweakopedia/qml/components/EncyclopediaTree.qml`
- Create: `apps/tweakopedia/qml/components/ArticleReader.qml`
- Create: `apps/tweakopedia/qml/components/ArticleSection.qml`
- Create: `apps/tweakopedia/qml/components/ArticleContents.qml`
- Create: `apps/tweakopedia/qml/components/RelatedArticleCard.qml`
- Create: `tests/qml/tst_tweakopedia.qml`
- Modify: `apps/tweakopedia/qml/Main.qml`
- Modify: `apps/tweakopedia/qml/components/FluentNavigation.qml`
- Modify: `apps/tweakopedia/resources.qrc`
- Modify: `tests/qml/tst_navigation.qml`

**Interfaces:**
- Consumes: `appController.encyclopedia` из Task 4 и существующий `FluentTheme`.
- Produces: рабочая страница с `objectName` для поиска, дерева, статьи, содержания,
  пустых состояний и связанных карточек; пункт навигации «Твикопедия» с иконкой
  открытой книги.

- [ ] **Step 1: Написать падающие QML-тесты навигации и страницы**

  Проверить новое название и иконку, замену `PlaceholderPage`, передачу запроса,
  открытие статьи из дерева, кнопки назад/вперёд, отображение только заполненных
  секций и открытие связанной статьи. Отдельно проверить начальное состояние,
  сообщение «ничего не найдено», очистку запроса, переход содержания к секции,
  отсутствие действий с очередью и доступные имена интерактивных элементов.

- [ ] **Step 2: Запустить QML-тесты и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target QmlComponentTests; ctest --test-dir build/mingw-debug -R QmlPlanPreviewHistory --output-on-failure`

  Expected: FAIL, страница и новое название отсутствуют.

- [ ] **Step 3: Реализовать оболочку, дерево и маршрутизацию**

  Использовать `TreeView`, `FluentSearchField`, существующие токены темы и сильные
  состояния фокуса. Поиск не выполняется на стороне QML. Начальное и пустое
  состояния находятся в области статьи. `Enter` открывает сфокусированный лист,
  стрелки используют штатную навигацию `TreeView`, а раскрытие узлов передаётся
  экранному диктору.

- [ ] **Step 4: Реализовать полотно статьи**

  Отрисовать хлебные крошки, заголовок, lead, информационную плашку, секции,
  сворачиваемые технические сведения, компактное содержание и связанные карточки.
  Значения из YAML выводятся как plain text с переносом и выделением технических
  объектов моноширинным начертанием.

- [ ] **Step 5: Запустить QML-тесты и подтвердить GREEN**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target QmlComponentTests; ctest --test-dir build/mingw-debug -R QmlPlanPreviewHistory --output-on-failure`

  Expected: PASS.

- [ ] **Step 6: Запустить app и QML-регрессию**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --test-dir build/mingw-debug -R "QmlPlanPreviewHistory|AppController|Encyclopedia" --output-on-failure`

  Expected: PASS.

- [ ] **Step 7: Зафиксировать изменение**

  ```powershell
  git add apps/tweakopedia/qml apps/tweakopedia/resources.qrc tests/qml
  git commit -m "feat: add Fluent encyclopedia page"
  ```

### Task 6: Адаптивность, визуальная проверка и единый EXE

**Files:**
- Create: `tests/visual/TweakopediaPagePreview.qml`
- Modify: `tests/qml/tst_tweakopedia.qml`
- Modify: `docs/testing/fluent-ui-checklist.md`
- Modify: `docs/testing/single-exe-results.md`

**Interfaces:**
- Consumes: законченная страница Task 5 и существующий `TweakopediaVisualCapture`.
- Produces: проверенные широкая/средняя/узкая компоновки и обновлённый
  `D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`.

- [ ] **Step 1: Добавить падающие QML-тесты адаптивных режимов**

  Проверить: широкое окно показывает дерево и содержание; среднее заменяет
  содержание кнопкой; узкое скрывает дерево в выдвижную панель; длинный заголовок
  и путь переносятся без выхода за правую границу; связанные карточки становятся
  вертикальными.

- [ ] **Step 2: Запустить QML-тест и подтвердить RED**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target QmlComponentTests; ctest --test-dir build/mingw-debug -R QmlPlanPreviewHistory --output-on-failure`

  Expected: FAIL на одном или нескольких брейкпоинтах.

- [ ] **Step 3: Реализовать адаптивные состояния и preview**

  Переиспользовать `FluentTheme.compactBreakpoint`; добавить только внутренний
  порог для правого содержания, если фактическая ширина полотна этого требует.
  Preview содержит длинный технический путь и шесть связанных карточек.

- [ ] **Step 4: Подтвердить GREEN и снять два изображения**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug --target QmlComponentTests TweakopediaVisualCapture; ctest --test-dir build/mingw-debug -R QmlPlanPreviewHistory --output-on-failure`

  Run wide: `build\mingw-debug\tests\TweakopediaVisualCapture.exe --input tests\visual\TweakopediaPagePreview.qml --output D:\ChatGPT\Temp\Tweakopedia\tweakopedia-wide.png --width 1440 --height 900`

  Run narrow: `build\mingw-debug\tests\TweakopediaVisualCapture.exe --input tests\visual\TweakopediaPagePreview.qml --output D:\ChatGPT\Temp\Tweakopedia\tweakopedia-narrow.png --width 900 --height 700`

  Expected: PASS; оба PNG существуют и не содержат обрезанных или наложенных элементов.

- [ ] **Step 5: Запустить полный debug-набор**

  Run: `. .\tools\enter-build-env.ps1; cmake --build --preset mingw-debug; ctest --preset mingw-debug`

  Expected: все тесты PASS.

- [ ] **Step 6: Собрать и проверить единый EXE**

  Run: `pwsh -NoProfile -File tools/package.ps1 -Preset mingw-release`

  Run: `pwsh -NoProfile -File tools/verify-package.ps1 -PackagePath D:\ChatGPT\Projects\Tweakopedia\dist\Tweakopedia.exe`

  Expected: обе команды PASS; `dist` содержит только `Tweakopedia.exe`.

- [ ] **Step 7: Обновить результаты проверки и зафиксировать изменение**

  Записать фактическое число тестов, результат визуальной проверки, размер и
  SHA-256 контейнера.

  ```powershell
  git add tests/visual/TweakopediaPagePreview.qml tests/qml/tst_tweakopedia.qml docs/testing
  git commit -m "test: verify encyclopedia release"
  ```

## Итоговая проверка

- [ ] Повторно сопоставить все критерии готовности спецификации с Tasks 1–6.
- [ ] Проверить `git diff --check` и отсутствие случайных изменений.
- [ ] Провести итоговый review всей ветки.
- [ ] Исправить Critical/Important замечания одним TDD-проходом.
- [ ] Повторить полный debug-набор и package verification после исправлений.
