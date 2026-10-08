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
        Label { text: "Очередь"; font.pixelSize: 28; font.weight: Font.DemiBold }
        Label { text: "Перед применением будет показан пересчитанный итоговый план."; color: palette.mid }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.controller.queue
            delegate: ItemDelegate {
                required property string id
                required property string title
                required property string currentState
                required property string targetState
                width: ListView.view.width
                text: title + "   " + currentState + " → " + targetState
                onClicked: root.controller.removeFromQueue(id)
            }
        }
    }
}
