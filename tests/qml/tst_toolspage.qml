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

    QtObject {
        id: fakeAppController
        property int queueCalls: 0
        property string lastProfileId: ""
        property var profileData: ({
            entries: [{
                id: "privacy.diagnostic-data-level",
                title: "Уровень диагностических данных Windows",
                group: "Диагностические данные",
                currentTitle: "Обязательные и дополнительные",
                targetTitle: "Только обязательные",
                status: "change"
            }],
            totalCount: 1,
            configuredCount: 0,
            changeCount: queueCalls > 0 ? 0 : 1,
            unsupportedCount: 0,
            queuedCount: queueCalls > 0 ? 1 : 0
        })
        function telemetryProfile(profileId) {
            lastProfileId = profileId || "balanced"
            profileData.changeCount = queueCalls > 0 ? 0 : 1
            profileData.queuedCount = queueCalls > 0 ? 1 : 0
            profileData.entries[0].status = queueCalls > 0 ? "queued" : "change"
            return profileData
        }
        function queueTelemetryProfile(profileId) {
            lastProfileId = profileId || "balanced"
            ++queueCalls
            return 1
        }
    }

    Component {
        id: toolsPageComponent
        ToolsPage {
            width: 960
            height: 660
            integrityController: fakeIntegrityController
            fileSystemController: fakeFileSystemController
            networkController: fakeNetworkController
            controller: fakeAppController
        }
    }

    function test_overviewShowsApprovedToolCards() {
        const page = createTemporaryObject(toolsPageComponent, this)

        compare(findChild(page, "toolsBackground").color, FluentTheme.canvas)
        compare(findChild(page, "toolsTitle").text, "Инструменты")
        compare(findChild(page, "toolsSubtitle").text,
                "Проверка состояния Windows и встроенные средства обслуживания.")

        const cards = findChild(page, "toolCardRepeater")
        compare(cards.count, 4)

        const expected = [
            "Проверка целостности системы",
            "Проверка файловой системы",
            "Диагностика сети",
            "Блокировка телеметрии"
        ]
        for (let index = 0; index < expected.length; ++index) {
            const card = findChild(page, "toolCard_" + index)
            verify(card)
            compare(card.Accessible.name, expected[index])
        }
    }

    function test_toolDetailHeadersUseFixedPageHeaderArea() {
        const page = createTemporaryObject(toolsPageComponent, this)
        const expected = [
            ["systemIntegrityPage", "systemIntegrityHeader"],
            ["fileSystemCheckPage", "fileSystemCheckHeader"],
            ["networkDiagnosticsPage", "networkDiagnosticsHeader"],
            ["telemetryToolPage", "telemetryToolHeader"]
        ]
        for (let index = 0; index < expected.length; ++index) {
            const toolPage = findChild(page, expected[index][0])
            verify(toolPage)
            verify(toolPage.header)
            compare(toolPage.header.objectName, expected[index][1])
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

    function test_telemetryCardOpensProfileAndQueuesChanges() {
        fakeAppController.queueCalls = 0
        const page = createTemporaryObject(toolsPageComponent, this)
        mouseClick(findChild(page, "toolCard_3"))
        verify(findChild(page, "telemetryToolPage").visible)
        compare(findChild(page, "telemetryProfileSelectorRepeater").count, 3)
        compare(findChild(page, "telemetryProfileRepeater").count, 1)
        mouseClick(findChild(page, "telemetryProfileChoice_strict"))
        compare(fakeAppController.lastProfileId, "strict")
        mouseClick(findChild(page, "telemetryQueueButton"))
        compare(fakeAppController.queueCalls, 1)
        compare(fakeAppController.lastProfileId, "strict")
        verify(findChild(page, "telemetryReviewButton").visible)
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
