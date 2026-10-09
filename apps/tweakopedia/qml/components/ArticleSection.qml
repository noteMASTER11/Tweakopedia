import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    required property var section
    property var technicalObjects: []
    property bool expanded: !section.technical
    readonly property string sectionIcon: {
        switch (section.id) {
        case "purpose": return "◎"
        case "mechanism": return "↻"
        case "effect": return "↗"
        case "tradeoffs": return "!"
        case "recommendation": return "✓"
        case "technical": return "</>"
        default: return "•"
        }
    }

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
                    objectName: "articleSectionIcon_" + root.section.id
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    Layout.alignment: Qt.AlignTop
                    radius: 15
                    color: "#EAF3FF"

                    FluentText {
                        objectName: "articleSectionGlyph_" + root.section.id
                        anchors.centerIn: parent
                        text: root.sectionIcon
                        color: FluentTheme.accent
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: root.section.technical ? 11 : 20
                        font.weight: Font.DemiBold
                    }
                }

                FluentText {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    text: root.section.title
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                Text {
                    objectName: "articleSectionChevron_" + root.section.id
                    visible: root.section.technical
                    text: root.expanded ? "\uE96D" : "\uE96E"
                    color: FluentTheme.textSecondary
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 12
                }
            }

            background: Rectangle {
                radius: 6
                color: headerButton.hovered && headerButton.enabled
                    ? FluentTheme.hover : "transparent"
            }
        }

        FluentText {
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
                required property int index
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: technicalText.implicitHeight + 16
                radius: 6
                color: FluentTheme.surfaceInset

                TextEdit {
                    id: technicalText
                    objectName: "technicalObject_" + index
                    anchors.fill: parent
                    anchors.margins: 8
                    text: modelData
                    readOnly: true
                    selectByMouse: true
                    color: FluentTheme.textPrimary
                    wrapMode: TextEdit.WrapAnywhere
                    font.family: "Consolas"
                    font.pixelSize: 12
                }
            }
        }
    }
}
