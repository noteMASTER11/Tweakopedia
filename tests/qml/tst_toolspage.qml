import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "ToolsPage"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    QtObject {
        id: fakeIntegrityController
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
        function startScan() { return true }
        function startRestore() { return true }
        function copyOutput() { return true }
        function openDismLog() { return true }
    }

    QtObject {
        id: fakeFileSystemController
        property var volumes: [
            { target: "C:", title: "C: — Windows", details: "NTFS · Системный том" }
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
        function setSelectedVolumeIndex(index) { selectedVolumeIndex = index; return true }
        function startCheck() { return true }
        function refreshVolumes() { return true }
        function copyOutput() { return true }
        function openSessionLog() { return true }
    }

    QtObject {
        id: fakeNetworkController
        property string state: "idle"
        property string stageText: ""
        property string outputText: ""
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property string sessionLogPath: ""
        property var checks: []
        property int okCount: 0
        property int attentionCount: 0
        property int unavailableCount: 0
        property bool hasAttention: false
        property bool busy: false
        property bool canStart: true
        function startDiagnostics() { return true }
        function copyOutput() { return true }
        function openSessionLog() { return true }
    }

    Component {
        id: toolsPageComponent
        ToolsPage {
            width: 960
            height: 660
            integrityController: fakeIntegrityController
            fileSystemController: fakeFileSystemController
            networkController: fakeNetworkController
        }
    }

    function test_overviewShowsApprovedToolCards() {
        const page = createTemporaryObject(toolsPageComponent, this)

        compare(findChild(page, "toolsBackground").color, FluentTheme.canvas)
        compare(findChild(page, "toolsTitle").text, "Инструменты")
        compare(findChild(page, "toolsSubtitle").text,
                "Проверка состояния Windows и встроенные средства обслуживания.")

        const cards = findChild(page, "toolCardRepeater")
        compare(cards.count, 5)

        const expected = [
            "Проверка целостности системы",
            "Проверка файловой системы",
            "Диагностика сети",
            "Восстановление сети",
            "Блокировка телеметрии"
        ]
        for (let index = 0; index < expected.length; ++index) {
            const card = findChild(page, "toolCard_" + index)
            verify(card)
            compare(card.Accessible.name, expected[index])
        }
    }

    function test_networkCardOpensDedicatedScreenAndReturns() {
        const page = createTemporaryObject(toolsPageComponent, this)
        const card = findChild(page, "toolCard_2")
        mouseClick(card)

        compare(findChild(page, "toolsOverview").visible, false)
        verify(findChild(page, "networkDiagnosticsPage").visible)

        mouseClick(findChild(page, "networkDiagnosticsBackButton"))
        compare(findChild(page, "toolsOverview").visible, true)
        compare(findChild(page, "networkDiagnosticsPage").visible, false)
    }

    function test_networkRecoveryActionOpensRecoveryPlaceholder() {
        fakeNetworkController.attentionCount = 1
        fakeNetworkController.hasAttention = true
        const page = createTemporaryObject(toolsPageComponent, this)
        mouseClick(findChild(page, "toolCard_2"))
        mouseClick(findChild(page, "networkDiagnosticsRecoveryButton"))
        compare(findChild(page, "toolPlaceholder").visible, true)
        compare(findChild(page, "toolPlaceholderTitle").text, "Восстановление сети")
    }

    function test_firstCardOpensIntegrityScreenAndReturnsWithoutResettingController() {
        const page = createTemporaryObject(toolsPageComponent, this)
        mouseClick(findChild(page, "toolCard_0"))

        compare(findChild(page, "toolsOverview").visible, false)
        verify(findChild(page, "systemIntegrityPage").visible)
        compare(fakeIntegrityController.state, "idle")

        findChild(page, "systemIntegrityPage").backRequested()
        tryCompare(findChild(page, "toolsOverview"), "visible", true)
        compare(fakeIntegrityController.state, "idle")
    }

    function test_secondCardOpensFileSystemScreenAndReturns() {
        const page = createTemporaryObject(toolsPageComponent, this)
        mouseClick(findChild(page, "toolCard_1"))

        compare(findChild(page, "toolsOverview").visible, false)
        verify(findChild(page, "fileSystemCheckPage").visible)
        compare(fakeFileSystemController.state, "idle")

        findChild(page, "fileSystemCheckPage").backRequested()
        tryCompare(findChild(page, "toolsOverview"), "visible", true)
    }
}
