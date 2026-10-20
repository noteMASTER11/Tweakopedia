import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"

TestCase {
    name: "FileSystemCheckPage"
    when: windowShown
    visible: true
    width: 1000
    height: 720

    QtObject {
        id: fakeController
        property var volumes: [
            { target: "C:", title: "C: — Windows", details: "NTFS · 125 ГБ свободно из 500 ГБ · Системный том" },
            { target: "D:", title: "D: — Data", details: "NTFS · 700 ГБ свободно из 1 000 ГБ" }
        ]
        property int selectedVolumeIndex: 0
        property bool hasVolumes: true
        property string state: "idle"
        property string stageText: ""
        property string outputText: ""
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property int exitCode: -1
        property string sessionLogPath: ""
        property bool busy: false
        property bool canCheck: true
        property int checkCalls: 0
        property int copyCalls: 0
        property int openCalls: 0
        function setSelectedVolumeIndex(index) { selectedVolumeIndex = index; return true }
        function startCheck() { ++checkCalls; return true }
        function refreshVolumes() { return true }
        function copyOutput() { ++copyCalls; return true }
        function openSessionLog() { ++openCalls; return true }
    }

    Component {
        id: pageComponent
        FileSystemCheckPage {
            width: 960
            height: 680
            controller: fakeController
        }
    }

    function init() {
        fakeController.selectedVolumeIndex = 0
        fakeController.hasVolumes = true
        fakeController.state = "idle"
        fakeController.stageText = ""
        fakeController.outputText = ""
        fakeController.resultTitle = ""
        fakeController.resultDescription = ""
        fakeController.errorCode = ""
        fakeController.exitCode = -1
        fakeController.sessionLogPath = ""
        fakeController.busy = false
        fakeController.canCheck = true
        fakeController.checkCalls = 0
        fakeController.copyCalls = 0
        fakeController.openCalls = 0
    }

    function test_idleShowsVolumeAndStartsReadOnlyCheck() {
        const page = createTemporaryObject(pageComponent, this)
        const selector = findChild(page, "fileSystemVolumeSelector")
        compare(selector.currentIndex, 0)
        compare(selector.currentText, "C: — Windows")
        const primary = findChild(page, "fileSystemCheckPrimaryButton")
        compare(primary.text, "Начать проверку")
        verify(primary.enabled)
        mouseClick(primary)
        compare(fakeController.checkCalls, 1)
    }

    function test_runningShowsIndeterminateProgressAndTerminal() {
        fakeController.state = "checking"
        fakeController.stageText = "Проверка файловой системы"
        fakeController.outputText = "CHKDSK проверяет файлы…"
        fakeController.busy = true
        fakeController.canCheck = false
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "fileSystemCheckStageText").text,
                "Проверка файловой системы")
        verify(findChild(page, "fileSystemCheckProgressBar").indeterminate)
        compare(findChild(page, "fileSystemCheckTerminal").text,
                "CHKDSK проверяет файлы…")
        verify(!findChild(page, "fileSystemVolumeSelector").enabled)
        verify(!findChild(page, "fileSystemCheckPrimaryButton").enabled)
    }

    function test_resultAndSecondaryActions() {
        fakeController.state = "issues_found"
        fakeController.resultTitle = "Обнаружены ошибки файловой системы"
        fakeController.resultDescription = "Проверка выполнялась без исправлений."
        fakeController.outputText = "Windows обнаружила проблемы."
        fakeController.sessionLogPath = "D:/logs/chkdsk.log"
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "fileSystemCheckResultTitle").text,
                "Обнаружены ошибки файловой системы")
        mouseClick(findChild(page, "fileSystemCheckCopyButton"))
        mouseClick(findChild(page, "fileSystemCheckOpenLogButton"))
        compare(fakeController.copyCalls, 1)
        compare(fakeController.openCalls, 1)
    }

    function test_narrowControlsStayInsidePage() {
        const page = createTemporaryObject(pageComponent, this, { width: 620, height: 680 })
        const selector = findChild(page, "fileSystemVolumeSelector")
        const point = selector.mapToItem(page, 0, 0)
        verify(point.x >= 0)
        verify(point.x + selector.width <= page.width)
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
        compare(articleSpy.signalArguments[0][0], "guide.file-system-check")
    }

    Component { id: signalSpy; SignalSpy {} }
}
