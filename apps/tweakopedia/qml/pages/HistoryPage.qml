import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
            delegate: ItemDelegate {
                required property string packageName
                required property string status
                required property date updatedAt
                width: ListView.view.width
                text: packageName + " · " + status + " · " + updatedAt.toLocaleString()
            }
        }
    }
}
