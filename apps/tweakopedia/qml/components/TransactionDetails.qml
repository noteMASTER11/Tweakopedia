import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root
    property string transactionId: ""
    property string packageName: ""
    property string status: ""
    property string error: ""
    property bool canRollback: false
    signal rollbackRequested(string transactionId)

    implicitHeight: content.implicitHeight + 28
    radius: 8
    color: FluentTheme.surface
    border.color: FluentTheme.stroke

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12
        ColumnLayout {
            Layout.fillWidth: true
            Text { text: root.packageName; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.weight: Font.DemiBold }
            Text { text: root.status; color: root.status === "succeeded" || root.status === "rolled_back" ? FluentTheme.stateOn : FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "transactionError"; text: root.error; visible: root.error.length > 0; color: FluentTheme.stateOff; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
        }
        Button {
            objectName: "rollbackButton"
            text: "Вернуть исходное"
            enabled: root.canRollback
            onClicked: root.rollbackRequested(root.transactionId)
        }
    }
}
