import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "TweakopediaPage"
    when: windowShown
    visible: true
    width: 1320
    height: 780

    ListModel {
        id: treeModel
        property int articleCount: count
    }

    QtObject {
        id: articleModel
        property bool hasArticle: false
        property var article: ({})
    }

    QtObject {
        id: encyclopediaController
        property var tree: treeModel
        property var article: articleModel
        property string query: ""
        property bool loading: false
        property bool canGoBack: false
        property bool canGoForward: false
        property string lastOpenedId: ""
        property int backCalls: 0
        property int forwardCalls: 0
        function setQuery(value) { query = value }
        function clearSearch() { query = "" }
        function openArticle(id) {
            lastOpenedId = id
            showArticle(id)
            return true
        }
        function goBack() { ++backCalls; return true }
        function goForward() { ++forwardCalls; return true }
    }

    QtObject {
        id: fakeController
        property var encyclopedia: encyclopediaController
    }

    Component {
        id: pageComponent
        TweakopediaPage {
            width: 1260
            height: 740
            controller: fakeController
        }
    }

    function articleData(id) {
        return {
            id: id,
            title: "Диагностические данные Windows",
            summary: "Управляет сбором и отправкой диагностических сведений Windows.",
            purpose: "Поясняет назначение параметра.",
            breadcrumbs: ["Твикопедия", "Конфиденциальность", "Диагностика и телеметрия"],
            sections: [
                {id: "purpose", title: "Назначение", text: "Назначение параметра.", technical: false},
                {id: "mechanism", title: "Как это работает", text: "Механизм Windows.", technical: false},
                {id: "technical", title: "Технические сведения", text: "Описание объекта.", technical: true}
            ],
            technicalObjects: ["HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection\\AllowTelemetry"],
            compatibility: {operatingSystems: ["Windows 10", "Windows 11"], minimumBuild: 19041},
            restart: {id: "none", title: "Не требуется", required: false},
            relatedArticles: [
                {id: "privacy.telemetry-details", title: "Подробности телеметрии", summary: "Связанная статья.", path: "Конфиденциальность › Диагностика"}
            ]
        }
    }

    function showArticle(id) {
        articleModel.article = articleData(id)
        articleModel.hasArticle = true
    }

    function initTestCase() {
        treeModel.append({
            nodeType: "article",
            id: "privacy.diagnostic-data",
            title: "Диагностические данные Windows",
            summary: "Сбор диагностических данных",
            path: "Конфиденциальность › Диагностика и телеметрия",
            depth: 0,
            expanded: false,
            selected: false,
            matchScore: 1000
        })
    }

    function init() {
        encyclopediaController.query = ""
        encyclopediaController.lastOpenedId = ""
        encyclopediaController.backCalls = 0
        encyclopediaController.forwardCalls = 0
        encyclopediaController.canGoBack = false
        encyclopediaController.canGoForward = false
        articleModel.article = ({})
        articleModel.hasArticle = false
    }

    function test_initialStateAndSearch() {
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "tweakopediaTitle").text, "Твикопедия")
        compare(findChild(page, "tweakopediaInitialState").visible, true)
        const search = findChild(page, "tweakopediaSearchField")
        verify(search)
        compare(search.Accessible.name, "Поиск в Твикопедии")
        search.forceActiveFocus()
        const query = "telemetry"
        for (let index = 0; index < query.length; ++index)
            keyClick(query.charAt(index))
        compare(encyclopediaController.query, query)
    }

    function test_treeArticleOpensReader() {
        const page = createTemporaryObject(pageComponent, this)
        const row = findChild(page, "encyclopediaNode_privacy.diagnostic-data")
        verify(row)
        mouseClick(row)
        compare(encyclopediaController.lastOpenedId, "privacy.diagnostic-data")
        tryCompare(findChild(page, "articleReader"), "visible", true)
        compare(findChild(page, "articleTitle").text, "Диагностические данные Windows")
        verify(findChild(page, "articleSection_purpose").visible)
        verify(findChild(page, "articleSection_technical").visible)
        compare(findChild(page, "articleSection_tradeoffs"), null)
        compare(findChild(page, "queueApplyButton"), null)
    }

    function test_contentsAndRelatedArticleNavigation() {
        showArticle("privacy.diagnostic-data")
        const page = createTemporaryObject(pageComponent, this)
        const contents = findChild(page, "articleContents_technical")
        verify(contents)
        contents.clicked()
        compare(page.lastRequestedSection, "technical")

        const related = findChild(page, "relatedArticle_privacy.telemetry-details")
        verify(related)
        compare(related.Accessible.name, "Открыть статью: Подробности телеметрии")
        related.clicked()
        compare(encyclopediaController.lastOpenedId, "privacy.telemetry-details")
    }

    function test_navigationButtonsAndNoResultsState() {
        const page = createTemporaryObject(pageComponent, this)
        const back = findChild(page, "tweakopediaBackButton")
        const forward = findChild(page, "tweakopediaForwardButton")
        compare(back.enabled, false)
        compare(forward.enabled, false)
        encyclopediaController.canGoBack = true
        encyclopediaController.canGoForward = true
        tryCompare(back, "enabled", true)
        tryCompare(forward, "enabled", true)
        back.clicked()
        forward.clicked()
        compare(encyclopediaController.backCalls, 1)
        compare(encyclopediaController.forwardCalls, 1)

        treeModel.clear()
        encyclopediaController.query = "ничего"
        tryCompare(findChild(page, "tweakopediaNoResults"), "visible", true)
        findChild(page, "tweakopediaClearSearchButton").clicked()
        compare(encyclopediaController.query, "")
        treeModel.append({nodeType: "article", id: "privacy.diagnostic-data",
                          title: "Диагностические данные Windows", summary: "",
                          path: "Конфиденциальность", depth: 0, expanded: false,
                          selected: false, matchScore: 1})
    }
}
