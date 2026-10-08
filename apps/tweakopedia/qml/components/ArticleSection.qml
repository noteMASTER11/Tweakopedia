import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    required property var section
    property var technicalObjects: []
    property bool expanded: !section.technical

    objectName: "articleSection_" + section.id
    implicitHeight: content.implicitHeight + 32
    radius: 10
    color: FluentTheme.surface
    border.width: 1
    border.color: FluentTheme.stroke

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 10

        AbstractButton {
            id: headerButton
            Layout.fillWidth: true
            implicitHeight: 30
            enabled: root.section.technical
            focusPolicy: root.section.technical ? Qt.StrongFocus : Qt.NoFocus
            Accessible.name: root.section.title
            Accessible.role: root.section.technical ? Accessible.Button : Accessible.Heading
            onClicked: root.expanded = !root.expanded

            contentItem: RowLayout {
                spacing: 10

                Rectangle {
                    width: 30
                    height: 30
                    radius: 15
                    color: "#EAF3FF"

                    Text {
                        anchors.centerIn: parent
                        text: root.section.technical ? "</>" : "•"
                        color: FluentTheme.accent
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: root.section.technical ? 11 : 20
                        font.weight: Font.DemiBold
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.section.title
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                Text {
                    visible: root.section.technical
                    text: root.expanded ? "⌃" : "⌄"
                    color: FluentTheme.textSecondary
                    font.pixelSize: 16
                }
            }

            background: Rectangle {
                radius: 6
                color: headerButton.hovered && headerButton.enabled
                    ? FluentTheme.hover : "transparent"
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.expanded
            text: root.section.text || ""
            color: FluentTheme.textSecondary
            wrapMode: Text.WordWrap
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
            lineHeight: 1.25
        }

        Repeater {
            model: root.expanded && root.section.technical ? root.technicalObjects : []

            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: technicalText.implicitHeight + 16
                radius: 6
                color: FluentTheme.surfaceInset

                Text {
                    id: technicalText
                    anchors.fill: parent
                    anchors.margins: 8
                    text: modelData
                    color: FluentTheme.textPrimary
                    wrapMode: Text.WrapAnywhere
                    font.family: "Consolas"
                    font.pixelSize: 12
                }
            }
        }
    }
}
