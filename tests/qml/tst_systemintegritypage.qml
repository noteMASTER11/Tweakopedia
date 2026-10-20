import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"

TestCase {
    name: "SystemIntegrityPage"
    when: windowShown
    visible: true
    width: 1000
    height: 720

    QtObject {
        id: fakeController
        property string state: "idle"
        property string stageText: ""
        property int progress: 0
        property bool progressKnown: false
        property string outputText: ""
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property int exitCode: -1
        property string sessionLogPath: ""
        property string dismLogPath: "C:/Windows/Logs/DISM/dism.log"
        property bool busy: false
        property bool canScan: true
        property bool canRestore: false
        property int scanCalls: 0
        property int restoreCalls: 0
        property int copyCalls: 0
        property int openCalls: 0
        function startScan() { ++scanCalls; return true }
        function startRestore() { ++restoreCalls; return true }
        function copyOutput() { ++copyCalls; return true }
        function openDismLog() { ++openCalls; return true }
    }

    Component {
        id: pageComponent
        SystemIntegrityPage {
            width: 960
            height: 680
            controller: fakeController
        }
    }

    function init() {
        fakeController.state = "idle"
        fakeController.stageText = ""
        fakeController.progress = 0
        fakeController.progressKnown = false
        fakeController.outputText = ""
        fakeController.resultTitle = ""
        fakeController.resultDescription = ""
        fakeController.errorCode = ""
        fakeController.exitCode = -1
        fakeController.busy = false
        fakeController.canScan = true
        fakeController.canRestore = false
        fakeController.scanCalls = 0
        fakeController.restoreCalls = 0
        fakeController.copyCalls = 0
        fakeController.openCalls = 0
    }

    function test_idleStartsScan() {
        const page = createTemporaryObject(pageComponent, this)
        const primary = findChild(page, "systemIntegrityPrimaryButton")
        compare(primary.text, "Начать проверку")
        verify(primary.enabled)
        mouseClick(primary)
        compare(fakeController.scanCalls, 1)
    }

    function test_runningShowsStageProgressAndTerminal() {
        fakeController.state = "scanning"
        fakeController.stageText = "Проверка хранилища компонентов"
        fakeController.progress = 47
        fakeController.progressKnown = true
        fakeController.outputText = "DISM output\n47.0%"
        fakeController.busy = true
        fakeController.canScan = false
        const page = createTemporaryObject(pageComponent, this)

        compare(findChild(page, "systemIntegrityStageText").text,
                "Проверка хранилища компонентов")
        compare(findChild(page, "systemIntegrityProgressBar").value, 47)
        compare(findChild(page, "systemIntegrityTerminal").text,
                "DISM output\n47.0%")
        verify(!findChild(page, "systemIntegrityPrimaryButton").enabled)
    }

    function test_repairableOffersRestoreButHealthyDoesNot() {
        fakeController.state = "repairable"
        fakeController.resultTitle = "Обнаружены исправимые повреждения"
        fakeController.resultDescription = "DISM может восстановить хранилище компонентов."
        fakeController.canRestore = true
        let page = createTemporaryObject(pageComponent, this)
        const primary = findChild(page, "systemIntegrityPrimaryButton")
        compare(primary.text, "Восстановить")
        mouseClick(primary)
        compare(fakeController.restoreCalls, 1)
        page.destroy()

        fakeController.state = "healthy"
        fakeController.resultTitle = "Хранилище компонентов исправно"
        fakeController.canRestore = false
        page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "systemIntegrityPrimaryButton").text, "Проверить ещё раз")
    }

    function test_secondaryActionsAndBackSignal() {
        fakeController.outputText = "output"
        const page = createTemporaryObject(pageComponent, this)
        const backSpy = signalSpy.createObject(page, {
            target: page,
            signalName: "backRequested"
        })
        mouseClick(findChild(page, "systemIntegrityCopyButton"))
        mouseClick(findChild(page, "systemIntegrityOpenLogButton"))
        mouseClick(findChild(page, "systemIntegrityBackButton"))
        compare(fakeController.copyCalls, 1)
        compare(fakeController.openCalls, 1)
        compare(backSpy.count, 1)
    }

    function test_narrowActionsStayInsidePage() {
        const page = createTemporaryObject(pageComponent, this, { width: 620, height: 680 })
        const primary = findChild(page, "systemIntegrityPrimaryButton")
        const point = primary.mapToItem(page, 0, 0)
        verify(point.x >= 0)
        verify(point.x + primary.width <= page.width)
    }

    function test_resultGuideOpensFullArticle() {
        const page = createTemporaryObject(pageComponent, this)
        const articleSpy = signalSpy.createObject(page, {
            target: page,
            signalName: "articleRequested"
        })
        const button = findChild(page, "toolResultGuideArticleButton")
        verify(button)
        mouseClick(button)
        compare(articleSpy.count, 1)
        compare(articleSpy.signalArguments[0][0], "guide.system-integrity")
    }

    Component { id: signalSpy; SignalSpy {} }
}
