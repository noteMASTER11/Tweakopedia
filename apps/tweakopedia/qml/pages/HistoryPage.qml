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
        Label { text: "История"; font.pixelSize: 28; font.weight: Font.DemiBold }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.controller.history
            delegate: TransactionDetails {
                width: ListView.view.width
                transactionId: model.transactionId
                packageName: model.packageName
                status: model.status
                error: model.error
                canRollback: model.canRollback
                onRollbackRequested: id => root.controller.rollback(id)
            }
        }
    }
}
