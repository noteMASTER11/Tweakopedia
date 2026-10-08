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

    ListModel { id: tweaksModel }
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
        function setTweakSearch(query) { lastSearch = query }
        function setTweakCategory(category) { lastCategory = category }
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
            binary: true, pending: false, supported: true, supportDetails: "",
            impact: "low", restart: "none"
        })
    }

    function init() {
        fakeController.lastSearch = ""
        fakeController.lastCategory = ""
        fakeController.lastTarget = ""
        queueModel.count = 2
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

    Component { id: signalSpy; SignalSpy {} }
}
