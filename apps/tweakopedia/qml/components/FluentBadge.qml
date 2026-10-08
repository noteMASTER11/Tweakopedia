import QtQuick
import "../style"

Rectangle {
    id: root

    property string text
    property color foreground: FluentTheme.accent
    property color fill: FluentTheme.selected

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: fill

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.foreground
        font.family: FluentTheme.fontFamily
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
