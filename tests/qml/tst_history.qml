import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "History"
    when: windowShown
    visible: true
    width: 900
    height: 500

    function test_progressAndFinalStatus() {
        const progress = createTemporaryObject(progressComponent, this)
        progress.progress = 45
        progress.status = "running"
        compare(findChild(progress, "progressBar").value, 0.45)
        compare(findChild(progress, "statusText").text, "running")
        progress.status = "succeeded"
        compare(findChild(progress, "statusText").text, "succeeded")
        compare(findChild(progress, "statusText").color, FluentTheme.stateOn)
        progress.status = "failed"
        compare(findChild(progress, "statusText").color, FluentTheme.stateOff)
    }

    function test_rollbackOnlyForCompleteSnapshot() {
        const details = createTemporaryObject(detailsComponent, this)
        const button = findChild(details, "rollbackButton")
        verify(!button.enabled)
        details.canRollback = true
        verify(button.enabled)
        details.error = "Ошибка применения"
        compare(findChild(details, "transactionError").visible, true)
        compare(findChild(details, "transactionError").color, FluentTheme.stateOff)
    }

    Component {
        id: progressComponent
        ApplyProgress { width: 700 }
    }
    Component {
        id: detailsComponent
        TransactionDetails {
            width: 700
            packageName: "Мой пакет"
            status: "succeeded"
            transactionId: "00000000-0000-0000-0000-000000000001"
        }
    }
}
