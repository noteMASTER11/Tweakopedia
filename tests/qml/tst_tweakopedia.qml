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
                {id: "effect", title: "Что изменится", text: "Изменяется поведение компонентов Windows.", technical: false},
                {id: "recommendation", title: "Рекомендация", text: "Проверьте совместимость перед применением параметра.", technical: false},
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

    function test_treeShowsFullTruncatedTitleInTooltip() {
        const originalTitle = treeModel.get(0).title
        treeModel.setProperty(0, "title",
            "Очень длинное полное название энциклопедической статьи о параметрах Windows")
        const page = createTemporaryObject(pageComponent, this, {
            width: 1440,
            availableWindowWidth: 1440
        })
        const row = findChild(page, "encyclopediaNode_privacy.diagnostic-data")
        const title = findChild(page, "encyclopediaNodeTitle_privacy.diagnostic-data")
        verify(row)
        verify(title)
        verify(title.truncated)
        compare(row.fullTitleTooltip, title.text)
        treeModel.setProperty(0, "title", originalTitle)
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

    function test_contentsScrollsToSectionAndTechnicalObjectIsSelectable() {
        showArticle("privacy.diagnostic-data")
        const page = createTemporaryObject(pageComponent, this, {
            width: 900,
            height: 360,
            availableWindowWidth: 900
        })
        const scroll = findChild(page, "articleScroll")
        const technicalSection = findChild(page, "articleSection_technical")
        verify(scroll)
        verify(technicalSection)
        technicalSection.expanded = true
        wait(0)

        const technical = findChild(page, "technicalObject_0")
        verify(technical)
        compare(technical.selectByMouse, true)

        const initialY = scroll.contentItem.contentY
        findChild(page, "articleContentsButton").clicked()
        const popupContents = findChild(page, "articleContentsPopupPanel")
        verify(popupContents)
        findChild(popupContents, "articleContents_technical").clicked()
        tryVerify(function() { return scroll.contentItem.contentY > initialY + 20 })

        scroll.contentItem.contentY = 0
        tryCompare(findChild(popupContents, "articleContents_purpose"), "current", true)
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

    function test_adaptiveTreeAndContents() {
        showArticle("privacy.diagnostic-data")
        const page = createTemporaryObject(pageComponent, this, {
            width: 1440,
            availableWindowWidth: 1440
        })
        const tree = findChild(page, "tweakopediaTreePanel")
        const treeButton = findChild(page, "tweakopediaTreeButton")
        const contents = findChild(page, "articleContentsPanel")
        const contentsButton = findChild(page, "articleContentsButton")
        const sectionsGrid = findChild(page, "articleSectionsGrid")
        const treeRow = findChild(page, "encyclopediaNode_privacy.diagnostic-data")
        verify(tree)
        verify(treeButton)
        verify(contents)
        verify(contentsButton)
        verify(sectionsGrid)
        verify(treeRow)
        compare(tree.visible, true)
        compare(treeButton.visible, false)
        compare(contents.visible, true)
        compare(contentsButton.visible, false)
        compare(sectionsGrid.columns, 2)
        verify(treeRow.width >= tree.width - 12)

        const purposeCard = findChild(page, "articleSection_purpose")
        const mechanismCard = findChild(page, "articleSection_mechanism")
        const effectCard = findChild(page, "articleSection_effect")
        const recommendationCard = findChild(page, "articleSection_recommendation")
        const purposeIcon = findChild(page, "articleSectionIcon_purpose")
        const mechanismIcon = findChild(page, "articleSectionIcon_mechanism")
        verify(purposeCard)
        verify(mechanismCard)
        verify(effectCard)
        verify(recommendationCard)
        verify(purposeIcon)
        verify(mechanismIcon)
        compare(purposeCard.height, mechanismCard.height)
        compare(effectCard.height, recommendationCard.height)
        compare(purposeIcon.width, mechanismIcon.width)
        compare(purposeIcon.height, mechanismIcon.height)
        verify(findChild(page, "articleSectionGlyph_purpose").text
               !== findChild(page, "articleSectionGlyph_mechanism").text)

        page.width = 1180
        page.availableWindowWidth = 1180
        tryCompare(tree, "visible", true)
        tryCompare(contents, "visible", false)
        tryCompare(contentsButton, "visible", true)
        compare(sectionsGrid.columns, 1)

        page.width = 900
        page.availableWindowWidth = 900
        tryCompare(tree, "visible", false)
        tryCompare(treeButton, "visible", true)
        treeButton.clicked()
        tryCompare(findChild(page, "tweakopediaTreeDrawer"), "opened", true)
    }

    function test_narrowLongContentAndRelatedCardsStayInsidePage() {
        const data = articleData("privacy.diagnostic-data")
        data.title = "Очень длинный заголовок статьи о параметрах Windows, который обязан переноситься и не выходить за правую границу окна"
        data.technicalObjects = [
            "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection\\ExtremelyLongTechnicalObjectNameThatMustWrapInsideTheArticleCanvas"
        ]
        data.relatedArticles = []
        for (let index = 0; index < 6; ++index) {
            data.relatedArticles.push({
                id: "related." + index,
                title: "Связанная статья " + (index + 1),
                summary: "Описание связанного материала.",
                path: "Конфиденциальность › Диагностика"
            })
        }
        articleModel.article = data
        articleModel.hasArticle = true

        const page = createTemporaryObject(pageComponent, this, {
            width: 900,
            availableWindowWidth: 900
        })
        const title = findChild(page, "articleTitle")
        const section = findChild(page, "articleSection_technical")
        verify(title)
        verify(section)
        section.expanded = true
        wait(0)

        const technical = findChild(page, "technicalObject_0")
        const related = findChild(page, "relatedArticle_related.0")
        verify(technical)
        verify(related)
        compare(title.wrapMode, Text.WordWrap)
        compare(technical.wrapMode, Text.WrapAnywhere)
        const titlePoint = title.mapToItem(page, 0, 0)
        const technicalPoint = technical.mapToItem(page, 0, 0)
        verify(titlePoint.x + title.width <= page.width + 1)
        verify(technicalPoint.x + technical.width <= page.width + 1)
        verify(related.width > page.width * 0.7)
    }
}
