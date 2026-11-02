import QtQuick
import "../style"

Rectangle {
    id: root
    property alias text: label.text
    property bool positive: true
    implicitWidth: label.implicitWidth + 18
    implicitHeight: 26
    radius: 13
    color: positive ? FluentTheme.positiveSurface : FluentTheme.warningSurface
    FluentText {
        id: label
        anchors.centerIn: parent
        color: root.positive ? FluentTheme.stateOn : FluentTheme.warning
        font.family: FluentTheme.fontFamily
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
}
