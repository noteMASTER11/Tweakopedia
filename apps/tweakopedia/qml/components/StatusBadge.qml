import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property alias text: label.text
    property bool positive: true
    implicitWidth: label.implicitWidth + 18
    implicitHeight: 28
    radius: 14
    color: positive ? "#DDF4E7" : "#F3E5C8"
    Label {
        id: label
        anchors.centerIn: parent
        color: root.positive ? "#176B3A" : "#7A5412"
        font.pixelSize: 12
    }
}
