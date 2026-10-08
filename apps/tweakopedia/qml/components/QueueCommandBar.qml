import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property int queueCount: 0
    signal reviewRequested()

    function countText() {
        const lastTwo = queueCount % 100
        const last = queueCount % 10
        if (lastTwo >= 11 && lastTwo <= 14)
            return queueCount + " изменений"
        if (last === 1)
            return queueCount + " изменение"
        if (last >= 2 && last <= 4)
            return queueCount + " изменения"
        return queueCount + " изменений"
    }

    visible: queueCount > 0
    implicitHeight: 64
    color: FluentTheme.surface
    border.width: 1
    border.color: FluentTheme.stroke

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 12

        Text {
            objectName: "queueCountLabel"
            Layout.fillWidth: true
            text: root.countText()
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Button {
            id: reviewButton
            objectName: "reviewQueueButton"
            text: "Просмотреть и применить"
            Accessible.name: text
            onClicked: root.reviewRequested()

            contentItem: Text {
                text: reviewButton.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            background: Rectangle {
                implicitWidth: 190
                implicitHeight: 36
                radius: 5
                color: reviewButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
            }
        }
    }
}
