import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    required property var controller
    padding: 24

    background: Rectangle { objectName: "queueBackground"; color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Text { text: "Очередь"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
        Text { text: "Перед применением будет показан пересчитанный итоговый план."; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 13 }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                objectName: "queueEmptyState"
                anchors.centerIn: parent
                visible: queueList.count === 0
                text: "Очередь пуста. Выберите состояния на странице «Твики»."
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
            }

            ListView {
                id: queueList
                anchors.fill: parent
                clip: true
                spacing: 8
                model: root.controller.queue
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 64
                    radius: 8
                    color: FluentTheme.surface
                    border.color: FluentTheme.stroke
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        Text { Layout.fillWidth: true; text: model.title + "   " + model.currentState + " → " + model.targetState; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; elide: Text.ElideRight }
                        Button { text: "Удалить"; onClicked: root.controller.removeFromQueue(model.id) }
                    }
                }
            }
        }

        PlanPreview {
            objectName: "queuePlanPreview"
            Layout.fillWidth: true
            visible: queueList.count > 0
            previewReady: root.controller.previewReady
            summary: root.controller.previewSummary
            operations: root.controller.previewOperations
            onPreviewRequested: root.controller.buildPreview()
            onApplyConfirmed: packageName => root.controller.applyQueue(packageName)
        }
        ApplyProgress {
            Layout.fillWidth: true
            visible: root.controller.applyStatus !== "idle"
            progress: root.controller.applyProgress
            status: root.controller.applyStatus
            message: root.controller.applyMessage
        }
    }
}
