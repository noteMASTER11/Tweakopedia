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
    ListModel { id: emptyHistory }

    QtObject {
        id: controller
        property var tweaks: emptyTweaks
        property var queue: emptyQueue
        property var history: emptyHistory
        property bool previewReady: false
        property string previewSummary: ""
        property var previewOperations: []
        property string applyStatus: "idle"
        property int applyProgress: 0
        property string applyMessage: ""
        function buildPreview() { return false }
        function applyQueue(name) { return false }
        function removeFromQueue(id) { return false }
        function rollback(id) { return false }
    }

    Component { id: overviewComponent; OverviewPage { width: 900; height: 620; controller: controller } }
    Component { id: queueComponent; QueuePage { width: 900; height: 620; controller: controller } }
    Component { id: historyComponent; HistoryPage { width: 900; height: 620; controller: controller } }

    function test_overviewEmptyStateUsesFluentSurface() {
        const page = createTemporaryObject(overviewComponent, this)
        const empty = findChild(page, "overviewEmptyState")
        compare(empty.visible, true)
        compare(findChild(page, "overviewBackground").color, FluentTheme.canvas)
    }

    function test_queueEmptyStateHidesPreviewCommands() {
        const page = createTemporaryObject(queueComponent, this)
        compare(findChild(page, "queueEmptyState").visible, true)
        compare(findChild(page, "queuePlanPreview").visible, false)
        compare(findChild(page, "queueBackground").color, FluentTheme.canvas)
    }

    function test_historyEmptyState() {
        const page = createTemporaryObject(historyComponent, this)
        compare(findChild(page, "historyEmptyState").visible, true)
        compare(findChild(page, "historyBackground").color, FluentTheme.canvas)
    }
}
