import QtQuick
import "../style"

Rectangle {
    id: root
    property alias text: label.text
    property bool positive: true
    implicitWidth: label.implicitWidth + 18
    implicitHeight: 26
    radius: 13
    color: positive ? "#DFF6DD" : "#FFF4CE"
    FluentText {
        id: label
        anchors.centerIn: parent
        color: root.positive ? FluentTheme.stateOn : "#7A5412"
        font.family: FluentTheme.fontFamily
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
}
