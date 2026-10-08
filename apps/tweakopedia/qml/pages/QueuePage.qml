import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Page {
    id: root
    required property var controller
    padding: 24

    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        Label { text: "Очередь"; font.pixelSize: 28; font.weight: Font.DemiBold }
        Label { text: "Перед применением будет показан пересчитанный итоговый план."; color: palette.mid }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.controller.queue
            delegate: ItemDelegate {
                width: ListView.view.width
                text: model.title + "   " + model.currentState + " → " + model.targetState
                onClicked: root.controller.removeFromQueue(model.id)
            }
        }
        PlanPreview {
            Layout.fillWidth: true
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
