import QtQuick
import QtQuick.Controls.Basic
import "../style"

Item {
    id: root

    property var states: []
    property string selectedState
    signal stateSelected(string stateId)

    implicitWidth: stateRow.implicitWidth
    implicitHeight: stateRow.implicitHeight

    Row {
        id: stateRow
        spacing: 4

        Repeater {
            model: root.states

            delegate: Button {
                required property var modelData

                objectName: "stateButton_" + modelData.id
                text: modelData.title
                implicitHeight: 32
                leftPadding: 12
                rightPadding: 12
                focusPolicy: Qt.StrongFocus
                Accessible.name: text
                onClicked: root.stateSelected(modelData.id)

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    font.weight: parent.modelData.id === root.selectedState ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 4
                    color: parent.modelData.id === root.selectedState
                        ? FluentTheme.selected
                        : parent.hovered ? FluentTheme.hover : FluentTheme.surface
                    border.width: 1
                    border.color: parent.modelData.id === root.selectedState
                        ? FluentTheme.accent : FluentTheme.stroke
                }
            }
        }
    }
}
