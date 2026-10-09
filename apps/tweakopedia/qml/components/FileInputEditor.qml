import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../style"

Item {
    id: root
    required property var definition
    property string value: ""
    signal confirmed(var value)
    implicitHeight: layout.implicitHeight

    function acceptFile(path) {
        value = path
        confirmed(path)
    }

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8
        Text {
            Layout.fillWidth: true
            text: root.value.length > 0 ? root.value : (root.definition.label || "Файл не выбран")
            color: root.value.length > 0 ? FluentTheme.textPrimary : FluentTheme.textSecondary
            elide: Text.ElideMiddle
            font.family: FluentTheme.fontFamily
            font.pixelSize: 13
        }
        Button {
            id: chooseButton
            objectName: "fileChooseButton"
            text: "Выбрать"
            implicitHeight: 34
            onClicked: dialog.open()
            contentItem: Text {
                text: chooseButton.text
                color: FluentTheme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: FluentTheme.fontFamily
                font.weight: Font.DemiBold
            }
            background: Rectangle {
                radius: 5
                color: chooseButton.hovered ? FluentTheme.hover : FluentTheme.surface
                border.color: FluentTheme.stroke
            }
        }
    }

    FileDialog {
        id: dialog
        title: root.definition.label || "Выберите файл"
        nameFilters: root.definition.extensions && root.definition.extensions.length > 0
            ? ["Допустимые файлы (" + root.definition.extensions.map(
                   extension => "*." + extension).join(" ") + ")"]
            : ["Все файлы (*)"]
        onAccepted: {
            let path = selectedFile.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            root.acceptFile(decodeURIComponent(path))
        }
    }
}
