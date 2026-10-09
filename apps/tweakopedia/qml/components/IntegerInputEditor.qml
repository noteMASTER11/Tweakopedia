import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root
    required property var definition
    property var value: undefined
    property string errorText: ""
    signal confirmed(var value)
    implicitHeight: layout.implicitHeight

    function confirm() {
        const number = Number(field.text)
        const validInteger = field.text.trim().length > 0 && Number.isInteger(number)
        const inRange = validInteger
            && (definition.minimum === undefined || number >= definition.minimum)
            && (definition.maximum === undefined || number <= definition.maximum)
        if (!inRange) {
            errorText = "Введите целое число в допустимом диапазоне."
            return
        }
        errorText = ""
        confirmed(number)
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 4
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: field
                objectName: "integerInputField"
                Layout.fillWidth: true
                placeholderText: root.definition.label || "Число"
                text: root.value === undefined ? "" : String(root.value)
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator {
                    bottom: root.definition.minimum === undefined ? -2147483648 : root.definition.minimum
                    top: root.definition.maximum === undefined ? 2147483647 : root.definition.maximum
                }
                font.family: FluentTheme.fontFamily
                onAccepted: root.confirm()
            }
            Button {
                id: confirmButton
                objectName: "confirmIntegerInput"
                text: "✓"
                implicitWidth: 38
                implicitHeight: 34
                onClicked: root.confirm()
                contentItem: Text {
                    text: confirmButton.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: FluentTheme.fontFamily
                    font.weight: Font.DemiBold
                }
                background: Rectangle { radius: 5; color: FluentTheme.accent }
            }
        }
        Text {
            objectName: "integerInputError"
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            color: FluentTheme.danger
            font.family: FluentTheme.fontFamily
            font.pixelSize: 12
        }
    }
}
