import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property string title
    property string subtitle
    property string backButtonObjectName
    signal backRequested()

    implicitHeight: Math.max(72, headerRow.implicitHeight + 20)
    color: FluentTheme.canvas

    RowLayout {
        id: headerRow
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 12

        AbstractButton {
            id: backButton
            objectName: root.backButtonObjectName
            Layout.preferredWidth: 42
            Layout.preferredHeight: 38
            focusPolicy: Qt.StrongFocus
            Accessible.name: "Вернуться к инструментам"
            onClicked: root.backRequested()

            contentItem: Text {
                text: "\uE72B"
                color: FluentTheme.textPrimary
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 6
                color: backButton.pressed ? FluentTheme.selected
                    : backButton.hovered ? FluentTheme.hover : FluentTheme.surface
                border.width: backButton.activeFocus ? 2 : 1
                border.color: backButton.activeFocus
                    ? FluentTheme.accent : FluentTheme.stroke
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            FluentText {
                Layout.fillWidth: true
                text: root.title
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 27
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            FluentText {
                Layout.fillWidth: true
                text: root.subtitle
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: FluentTheme.stroke
    }
}
