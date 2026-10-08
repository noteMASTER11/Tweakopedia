import QtQuick
import QtQuick.Controls.Basic
import "../style"

AbstractButton {
    id: root

    signal toggledByUser(bool checked)

    implicitWidth: 40
    implicitHeight: 20
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: checked ? "Включено" : "Выключено"
    Accessible.checked: checked

    onClicked: toggledByUser(!checked)

    background: Rectangle {
        id: track
        objectName: "toggleTrack"
        width: 40
        height: 20
        radius: height / 2
        color: !root.enabled
            ? FluentTheme.disabledSurface
            : root.checked ? FluentTheme.accent : FluentTheme.surface
        border.width: 1
        border.color: !root.enabled
            ? FluentTheme.stroke
            : root.checked ? FluentTheme.accent : FluentTheme.textSecondary

        Rectangle {
            id: thumb
            objectName: "toggleThumb"
            width: 16
            height: 16
            radius: 8
            y: 2
            x: root.checked ? track.width - width - 2 : 2
            color: !root.enabled
                ? FluentTheme.disabledText
                : root.checked ? FluentTheme.surface : FluentTheme.textSecondary

            Behavior on x { NumberAnimation { duration: 100 } }
        }
    }
}
