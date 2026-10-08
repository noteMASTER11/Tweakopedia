import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    required property var controller
    padding: 24

    background: Rectangle { objectName: "historyBackground"; color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        Text { text: "История"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Text {
                objectName: "historyEmptyState"
                anchors.centerIn: parent
                visible: historyList.count === 0
                text: "История пуста. Здесь появятся применённые пакеты и операции возврата."
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
            }
            ListView {
                id: historyList
                anchors.fill: parent
                spacing: 8
                clip: true
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
}
