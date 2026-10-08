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

    function test_transactionCardUsesFluentExpanderAndShowsOperations() {
        const details = createTemporaryObject(detailsComponent, this)
        const header = findChild(details, "transactionHeader")
        const chevron = findChild(details, "transactionChevron")
        const operation = findChild(details, "transactionOperation_0")

        verify(header)
        verify(chevron)
        compare(chevron.font.family, "Segoe MDL2 Assets")
        compare(findChild(details, "transactionStatusText").text, "Применено")
        compare(findChild(details, "transactionPackageTitle").text, "Мой пакет")
        compare(details.expanded, false)
        compare(operation.visible, false)

        mouseClick(header, 24, header.height / 2)

        compare(details.expanded, true)
        compare(operation.visible, true)
        compare(findChild(details, "transactionOperationTitle_0").text,
                "Поддержка длинных путей Win32")
        compare(findChild(details, "transactionOperationTransition_0").text,
                "Выключено · DWORD 0  →  Включено · DWORD 1")
    }

    Component {
        id: progressComponent
        ApplyProgress { width: 700 }
    }
    Component {
        id: detailsComponent
        TransactionDetails {
            width: 700
            packageName: "Мой пакет — 08.10.2026 23:01"
            status: "succeeded"
            transactionId: "00000000-0000-0000-0000-000000000001"
            updatedAt: new Date("2026-10-08T20:30:00Z")
            detailsAvailable: true
            operations: [{
                title: "Поддержка длинных путей Win32",
                kind: "Реестр",
                object: "HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled",
                before: "Выключено · DWORD 0",
                after: "Включено · DWORD 1",
                restart: "Не требуется"
            }]
        }
    }
}
