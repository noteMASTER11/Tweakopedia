import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root
    required property var definition
    property var value: undefined
    signal valueConfirmed(string inputId, var value)
    implicitHeight: loader.item ? loader.item.implicitHeight : 0

    function acceptFile(path) {
        if (loader.item && loader.item.acceptFile) loader.item.acceptFile(path)
    }

    Loader {
        id: loader
        anchors.left: parent.left
        anchors.right: parent.right
        sourceComponent: root.definition.type === "text" ? textEditor
            : root.definition.type === "integer" ? integerEditor
            : root.definition.type === "file" ? fileEditor
            : root.definition.type === "choice" ? choiceEditor
            : booleanEditor
    }

    Component {
        id: textEditor
        TextInputEditor {
            definition: root.definition
            value: root.value === undefined ? "" : String(root.value)
            onConfirmed: value => root.valueConfirmed(root.definition.id, value)
        }
    }
    Component {
        id: integerEditor
        IntegerInputEditor {
            definition: root.definition
            value: root.value
            onConfirmed: value => root.valueConfirmed(root.definition.id, value)
        }
    }
    Component {
        id: fileEditor
        FileInputEditor {
            definition: root.definition
            value: root.value === undefined ? "" : String(root.value)
            onConfirmed: value => root.valueConfirmed(root.definition.id, value)
        }
    }
    Component {
        id: choiceEditor
        RowLayout {
            implicitHeight: selector.implicitHeight
            FluentText {
                Layout.fillWidth: true
                text: root.definition.label || "Вариант"
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }
            ComboBox {
                id: selector
                objectName: "inputChoiceSelector"
                model: root.definition.choices || []
                textRole: "label"
                valueRole: "value"
                onActivated: index => root.valueConfirmed(
                    root.definition.id, root.definition.choices[index].value)
            }
        }
    }
    Component {
        id: booleanEditor
        RowLayout {
            implicitHeight: toggle.implicitHeight
            FluentText {
                Layout.fillWidth: true
                text: root.definition.label || "Параметр"
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }
            FluentToggle {
                id: toggle
                objectName: "inputBooleanToggle"
                checked: root.value === true
                onToggledByUser: checked => root.valueConfirmed(root.definition.id, checked)
            }
        }
    }
}
