import QtQuick
import QtQuick.Controls.Basic
import "../style"

Button {
    id: root

    text: "?"
    implicitWidth: 28
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus
    Accessible.name: "Подробное описание"
    ToolTip.visible: hovered
    ToolTip.text: Accessible.name

    contentItem: Text {
        text: root.text
        color: root.enabled ? FluentTheme.accent : FluentTheme.disabledText
        font.family: FluentTheme.fontFamily
        font.pixelSize: 14
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 4
        color: root.down ? FluentTheme.selected : root.hovered ? FluentTheme.hover : "transparent"
        border.width: 1
        border.color: FluentTheme.stroke
    }
}
