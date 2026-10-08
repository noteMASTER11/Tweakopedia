import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/pages"

TestCase {
    name: "TweaksPage"
    when: windowShown
    visible: true
    width: 1300
    height: 760

    ListModel {
        id: tweaksModel
        property string categoryId: ""
    }
    ListModel { id: categoriesModel }
    QtObject {
        id: queueModel
        property int count: 2
        function rowCount() { return count }
    }
    QtObject {
        id: fakeController
        property var filteredTweaks: tweaksModel
        property var categories: categoriesModel
        property var queue: queueModel
        property string lastSearch: ""
        property string lastCategory: ""
        property string lastTarget: ""
        property string appRemovalScanStatus: "idle"
        property string appRemovalScanError: ""
        property int scanCalls: 0
        property string lastRevealedTweak: ""
        function setTweakSearch(query) { lastSearch = query }
        function setTweakCategory(category) {
            lastCategory = category
            tweaksModel.categoryId = category
        }
        function selectTarget(id, state) { lastTarget = id + ":" + state; return true }
        function openExplanation(id) {
            return {
                title: id === "second" ? "Второе пояснение" : "Первое пояснение",
                purpose: "Назначение", mechanism: "Механизм", effect: "Эффект",
                tradeoffs: "Ограничения", recommendation: "Рекомендация",
                technicalDetails: "Детали", registryObject: "HKLM\\Object",
                rollback: "Возврат"
            }
        }
        function revealTweak(id) {
            lastRevealedTweak = id
            return id === "filesystem.win32-long-paths" ? 0 : -1
        }
        function scanInstalledApps() {
            ++scanCalls
            appRemovalScanStatus = "running"
        }
    }

    Component {
        id: pageComponent
        TweaksPage {
            width: 1180
            height: 720
            controller: fakeController
        }
    }

    function initTestCase() {
        categoriesModel.append({id: "", title: "Все категории", subcategories: []})
        categoriesModel.append({id: "filesystem", title: "Файловая система", subcategories: []})
        tweaksModel.append({
            id: "filesystem.win32-long-paths", title: "Поддержка длинных путей Win32",
            summary: "Работа с длинными путями", currentState: "disabled",
            currentStateTitle: "выключено", targetState: "", targetStateTitle: "",
            availableStates: [{id: "disabled", title: "Выключено"}, {id: "enabled", title: "Включено"}],
            binary: true, action: false, pending: false, supported: true, supportDetails: "",
            impact: "low", restart: "none"
        })
    }

    function init() {
        fakeController.lastSearch = ""
        fakeController.lastCategory = ""
        fakeController.lastTarget = ""
        fakeController.appRemovalScanStatus = "idle"
        fakeController.appRemovalScanError = ""
        fakeController.scanCalls = 0
        fakeController.lastRevealedTweak = ""
        tweaksModel.categoryId = ""
        queueModel.count = 2
    }

    function test_appRemovalRequiresExplicitSearchAndShowsProgress() {
        categoriesModel.append({id: "app-removal", title: "Удаление приложений", subcategories: []})
        const page = createTemporaryObject(pageComponent, this)

        mouseClick(findChild(page, "categoryButton_app-removal"))
        const prompt = findChild(page, "appRemovalSearchPrompt")
        verify(prompt.visible)
        verify(!findChild(page, "tweakList").visible)

        const button = findChild(page, "appRemovalSearchButton")
        mouseClick(button)
        compare(fakeController.scanCalls, 1)
        compare(button.enabled, false)
        verify(findChild(page, "appRemovalSearchSpinner").visible)

        fakeController.appRemovalScanStatus = "succeeded"
        tryCompare(prompt, "visible", false)
        verify(findChild(page, "tweakList").visible)
        categoriesModel.remove(categoriesModel.count - 1)
    }

    function test_searchCategoryRolesAndQueueReview() {
        const page = createTemporaryObject(pageComponent, this)
        const search = findChild(page, "tweakSearchField")
        search.forceActiveFocus()
        const query = "PATHS"
        for (let index = 0; index < query.length; ++index)
            keyClick(query.charAt(index))
        compare(fakeController.lastSearch, "PATHS")

        mouseClick(findChild(page, "categoryButton_filesystem"))
        compare(fakeController.lastCategory, "filesystem")

        const row = findChild(page, "tweakRow_filesystem.win32-long-paths")
        verify(row)
        compare(row.currentStateTitle, "выключено")
        compare(row.binary, true)
        compare(row.pending, false)

        const bar = findChild(page, "queueCommandBar")
        compare(bar.visible, true)
        compare(findChild(bar, "queueCountLabel").text, "2 изменения")
        const spy = signalSpy.createObject(page, {target: page, signalName: "reviewRequested"})
        findChild(bar, "reviewQueueButton").clicked()
        compare(spy.count, 1)
    }

    function test_infoPaneDockOverlayReplacementAndEscapeFocus() {
        const page = createTemporaryObject(pageComponent, this)
        const pane = findChild(page, "infoPane")
        const row = findChild(page, "tweakRow_filesystem.win32-long-paths")

        mouseClick(row, 16, 16)
        compare(pane.opened, true)
        compare(pane.docked, true)
        compare(findChild(pane, "infoTitle").text, "Первое пояснение")

        page.showExplanation("second", row)
        compare(pane.opened, true)
        compare(findChild(pane, "infoTitle").text, "Второе пояснение")

        page.width = 1179
        compare(pane.docked, false)
        pane.forceActiveFocus()
        keyClick(Qt.Key_Escape)
        compare(pane.opened, false)
        tryCompare(row, "activeFocus", true)
    }

    function test_openTweakRevealsRowAndExplanation() {
        const page = createTemporaryObject(pageComponent, this)
        findChild(page, "tweakSearchField").text = "старый фильтр"

        page.openTweak("filesystem.win32-long-paths")

        compare(fakeController.lastRevealedTweak, "filesystem.win32-long-paths")
        compare(findChild(page, "tweakSearchField").text, "")
        tryCompare(findChild(page, "tweakList"), "currentIndex", 0)
        tryCompare(findChild(page, "infoPane"), "opened", true)
        compare(findChild(page, "infoTitle").text, "Первое пояснение")
    }

    function test_emptyQueueHidesCommandBar() {
        queueModel.count = 0
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "queueCommandBar").visible, false)
    }

    function test_queueBarTracksCountChanges() {
        queueModel.count = 0
        const page = createTemporaryObject(pageComponent, this)
        const bar = findChild(page, "queueCommandBar")
        compare(bar.visible, false)

        queueModel.count = 1
        tryCompare(bar, "visible", true)
        compare(findChild(bar, "queueCountLabel").text, "1 изменение")

        queueModel.count = 0
        tryCompare(bar, "visible", false)
    }

    function test_eachTopLevelCategoryHasItsOwnIcon() {
        const additionalCategories = [
            ["behavior", "Поведение Windows"],
            ["boot", "Загрузка и вход"],
            ["accounts", "Учётные записи"],
            ["desktop", "Рабочий стол"],
            ["privacy", "Конфиденциальность"],
            ["network", "Сеть"],
            ["apps", "Приложения"],
            ["app-removal", "Удаление приложений"],
            ["devices", "Устройства"],
            ["experimental", "Экспериментальные функции"],
            ["gaming", "Игры"],
            ["power", "Питание"],
            ["updates", "Обновления"]
        ]
        for (const category of additionalCategories)
            categoriesModel.append({id: category[0], title: category[1], subcategories: []})

        const page = createTemporaryObject(pageComponent, this)
        page.height = 1100
        const categoryIds = [
            "", "filesystem", "behavior", "boot", "accounts", "desktop", "privacy",
            "network", "apps", "app-removal", "devices", "experimental", "gaming",
            "power", "updates"
        ]
        const glyphs = []
        for (const categoryId of categoryIds) {
            const icon = findChild(page, "categoryIcon_" + categoryId)
            verify(icon)
            verify(icon.text.length > 0)
            compare(icon.font.family, "Segoe MDL2 Assets")
            verify(glyphs.indexOf(icon.text) === -1)
            glyphs.push(icon.text)
        }

        categoriesModel.remove(2, additionalCategories.length)
    }

    Component { id: signalSpy; SignalSpy {} }
}
