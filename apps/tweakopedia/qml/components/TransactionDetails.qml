import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property string transactionId: ""
    property string packageName: ""
    property string status: ""
    property string error: ""
    property bool canRollback: false
    signal rollbackRequested(string transactionId)

    RowLayout {
        anchors.fill: parent
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: root.packageName; font.weight: Font.DemiBold }
            Label { text: root.status }
            Label { text: root.error; visible: root.error.length > 0; wrapMode: Text.WordWrap }
        }
        Button {
            objectName: "rollbackButton"
            text: "Вернуть исходное"
            enabled: root.canRollback
            onClicked: root.rollbackRequested(root.transactionId)
        }
    }
}
