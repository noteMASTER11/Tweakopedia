import QtQuick
import QtQuick.Controls.Basic
import "../style"

AbstractButton {
    id: root

    property string title
    property string glyph
    property bool compact: false
    property bool selected: false
    property int badgeCount: 0
    property int destinationIndex: -1

    implicitWidth: compact ? FluentTheme.navigationCompactWidth : FluentTheme.navigationExpandedWidth
    implicitHeight: 44
    focusPolicy: Qt.StrongFocus
    Accessible.name: title
    Accessible.role: Accessible.Button

    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.leftMargin: root.compact ? 0 : 14
            anchors.verticalCenter: parent.verticalCenter
            width: root.compact ? parent.width : implicitWidth
            spacing: 12

            Text {
                width: root.compact ? parent.width : 24
                text: root.glyph
                color: root.selected ? FluentTheme.accent : FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                visible: !root.compact
                text: root.title
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
                font.weight: root.selected ? Font.DemiBold : Font.Normal
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        FluentBadge {
            objectName: "navBadge_" + root.destinationIndex
            visible: root.badgeCount > 0
            text: String(root.badgeCount)
            implicitWidth: 22
            anchors.right: parent.right
            anchors.rightMargin: root.compact ? 3 : 10
            anchors.top: parent.top
            anchors.topMargin: root.compact ? 1 : 11
        }
    }

    background: Rectangle {
        anchors.leftMargin: root.compact ? 8 : 4
        anchors.rightMargin: root.compact ? 8 : 4
        radius: 6
        color: root.selected
            ? FluentTheme.selected
            : root.hovered ? FluentTheme.hover : "transparent"

        Rectangle {
            visible: root.selected
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 20
            radius: 2
            color: FluentTheme.accent
        }
    }
}
