import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window

    width: 1440
    height: 900
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia history preview"

    ListModel {
        id: historyModel

        ListElement {
            transactionId: "00000000-0000-0000-0000-000000000001"
            packageName: "Рабочее окружение"
            status: "succeeded"
            updatedAt: "2026-10-08T20:30:00Z"
            error: ""
            canRollback: true
            detailsAvailable: true
            operations: [
                ListElement {
                    title: "Поддержка длинных путей Win32"
                    kind: "Реестр"
                    object: "HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled"
                    before: "Выключено · DWORD 0"
                    after: "Включено · DWORD 1"
                    restart: "Не требуется"
                },
                ListElement {
                    title: "Подробные сообщения входа"
                    kind: "Реестр"
                    object: "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\VerboseStatus"
                    before: "Выключено · значение отсутствовало"
                    after: "Включено · DWORD 1"
                    restart: "Выход из системы"
                }
            ]
        }

        ListElement {
            transactionId: "00000000-0000-0000-0000-000000000002"
            packageName: "Удаление встроенных приложений"
            status: "failed"
            updatedAt: "2026-10-08T18:15:00Z"
            error: "Не удалось удалить один из выбранных пакетов."
            canRollback: false
            detailsAvailable: true
            operations: [
                ListElement {
                    title: "Удалить Clipchamp"
                    kind: "Приложение"
                    object: "Clipchamp.Clipchamp"
                    before: "Установлено · пакетов: 1"
                    after: "Удалить"
                    restart: "Не требуется"
                }
            ]
        }

        ListElement {
            transactionId: "00000000-0000-0000-0000-000000000003"
            packageName: "Экспериментальные функции"
            status: "rolled_back"
            updatedAt: "2026-10-08T16:42:00Z"
            error: ""
            canRollback: false
            detailsAvailable: true
            operations: [
                ListElement {
                    title: "Новый мастер создания архивов"
                    kind: "Feature Store"
                    object: "Feature ID 18261182"
                    before: "По умолчанию"
                    after: "Включено · состояние 2"
                    restart: "Перезапуск Проводника"
                }
            ]
        }
    }

    QtObject {
        id: previewController
        property var history: historyModel
        function rollback(id) { return true }
    }

    function findObject(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const children = item.children || []
        for (let index = 0; index < children.length; ++index) {
            const found = findObject(children[index], name)
            if (found)
                return found
        }
        return null
    }

    Component.onCompleted: Qt.callLater(function() {
        const first = findObject(historyPage,
            "transactionDetails_00000000-0000-0000-0000-000000000001")
        if (first)
            first.expanded = true
    })

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 4
        }

        HistoryPage {
            id: historyPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: previewController
        }
    }
}
