import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "RemainingPages"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    ListModel { id: emptyTweaks }
    ListModel { id: emptyQueue }
    ListModel { id: queuedChanges }
    ListModel { id: emptyHistory }

    QtObject {
        id: fakeController
        property var tweaks: emptyTweaks
        property var queue: emptyQueue
        property var history: emptyHistory
        property bool previewReady: false
        property string previewSummary: ""
        property var previewOperations: []
        property string applyStatus: "idle"
        property int applyProgress: 0
        property string applyMessage: ""
        property bool rebootRequired: false
        property int applyCalls: 0
        property int restartCalls: 0
        property string lastPackageName: ""
        property string lastRemovedId: ""
        function buildPreview() { return false }
        function applyQueue(name) {
            ++applyCalls
            lastPackageName = name
            applyStatus = "running"
            return true
        }
        function removeFromQueue(id) { lastRemovedId = id; return true }
        function restartComputer() { ++restartCalls; return true }
        function rollback(id) { return false }
    }

    Component { id: overviewComponent; OverviewPage { width: 900; height: 620; controller: fakeController } }
    Component { id: queueComponent; QueuePage { width: 900; height: 620; controller: fakeController } }
    Component { id: historyComponent; HistoryPage { width: 900; height: 620; controller: fakeController } }

    function init() {
        queuedChanges.clear()
        fakeController.queue = emptyQueue
        fakeController.applyStatus = "idle"
        fakeController.applyProgress = 0
        fakeController.applyMessage = ""
        fakeController.rebootRequired = false
        fakeController.applyCalls = 0
        fakeController.restartCalls = 0
        fakeController.lastPackageName = ""
        fakeController.lastRemovedId = ""
    }

    function addQueuedChange() {
        queuedChanges.append({
            id: "boot.verbose-logon-messages",
            title: "Подробные сообщения входа",
            currentState: "disabled",
            targetState: "enabled"
        })
        fakeController.queue = queuedChanges
    }

    function test_overviewEmptyStateUsesFluentSurface() {
        const page = createTemporaryObject(overviewComponent, this)
        const empty = findChild(page, "overviewEmptyState")
        compare(empty.visible, true)
        compare(findChild(page, "overviewBackground").color, FluentTheme.canvas)
    }

    function test_queueEmptyStateHidesApplyCommand() {
        const page = createTemporaryObject(queueComponent, this)
        compare(findChild(page, "queueEmptyState").visible, true)
        compare(findChild(page, "applyQueueButton").visible, false)
        compare(findChild(page, "queuePlanPreview"), null)
        compare(findChild(page, "queueBackground").color, FluentTheme.canvas)
    }

    function test_queueUsesCancelAndApplyActions() {
        addQueuedChange()
        const page = createTemporaryObject(queueComponent, this)

        const cancel = findChild(page, "cancelQueueItemButton")
        compare(cancel.text, "Отменить")
        const cancelBackground = findChild(page, "cancelQueueItemBackground")
        compare(cancelBackground.color, "#c42b1c")
        compare(cancelBackground.radius, 6)
        mouseMove(cancel, cancel.width / 2, cancel.height / 2)
        tryCompare(cancel, "hovered", true)
        compare(cancelBackground.color, "#a4262c")
        cancel.clicked()
        compare(fakeController.lastRemovedId, "boot.verbose-logon-messages")

        const apply = findChild(page, "applyQueueButton")
        compare(apply.text, "Применить")
        compare(findChild(page, "applyQueueButtonBackground").color, FluentTheme.accent)
        compare(findChild(page, "applyQueueButtonBackground").radius, 6)
        apply.clicked()

        compare(fakeController.applyCalls, 1)
        verify(fakeController.lastPackageName.indexOf("Изменения Windows — ") === 0)
        compare(apply.enabled, false)
        compare(findChild(page, "applySpinner").visible, true)
        compare(findChild(page, "applySpinner").running, true)
    }

    function test_failedApplyRestoresButtonAndShowsMessage() {
        addQueuedChange()
        fakeController.applyStatus = "failed"
        fakeController.applyMessage = "Не удалось применить изменения"
        const page = createTemporaryObject(queueComponent, this)

        compare(findChild(page, "applyQueueButton").enabled, true)
        compare(findChild(page, "applySpinner").visible, false)
        const error = findChild(page, "applyErrorMessage")
        compare(error.visible, true)
        compare(error.text, "Не удалось применить изменения")
    }

    function test_successScreenOffersRestartOnlyWhenRequired() {
        fakeController.applyStatus = "succeeded"
        const page = createTemporaryObject(queueComponent, this)

        compare(findChild(page, "applySuccessView").visible, true)
        compare(findChild(page, "applySuccessTitle").text, "Настройки применены")
        compare(findChild(page, "applySuccessCheck").color, FluentTheme.stateOn)
        compare(findChild(page, "rebootRequirementText").visible, false)
        compare(findChild(page, "restartComputerButton").visible, false)

        fakeController.rebootRequired = true
        const rebootText = findChild(page, "rebootRequirementText")
        const restart = findChild(page, "restartComputerButton")
        tryCompare(rebootText, "visible", true)
        compare(rebootText.text, "Некоторые из применённых настроек требуют перезагрузки ПК")
        tryCompare(restart, "visible", true)
        compare(restart.text, "Перезагрузить")
        restart.clicked()
        compare(fakeController.restartCalls, 1)
    }

    function test_historyEmptyState() {
        const page = createTemporaryObject(historyComponent, this)
        compare(findChild(page, "historyEmptyState").visible, true)
        compare(findChild(page, "historyBackground").color, FluentTheme.canvas)
    }
}
