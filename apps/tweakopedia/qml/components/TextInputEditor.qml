import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root
    required property var definition
    property string value: ""
    signal confirmed(var value)
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        TextField {
            id: field
            objectName: "textInputField"
            Layout.fillWidth: true
            placeholderText: root.definition.label || "Значение"
            text: root.value
            maximumLength: root.definition.maxLength || 32767
            font.family: FluentTheme.fontFamily
            onAccepted: confirmButton.clicked()
        }

        Button {
            id: confirmButton
            objectName: "confirmTextInput"
            text: "✓"
            enabled: (!root.definition.required || field.text.trim().length > 0)
                && (!root.definition.minLength || field.text.trim().length >= root.definition.minLength)
            implicitWidth: 38
            implicitHeight: 34
            Accessible.name: "Подтвердить поле «" + (root.definition.label || "") + "»"
            onClicked: root.confirmed(field.text.trim())
            contentItem: Text {
                text: confirmButton.text
                color: confirmButton.enabled ? "white" : FluentTheme.disabledText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: FluentTheme.fontFamily
                font.weight: Font.DemiBold
            }
            background: Rectangle {
                radius: 5
                color: confirmButton.enabled ? FluentTheme.accent : FluentTheme.disabledSurface
            }
        }
    }
}
