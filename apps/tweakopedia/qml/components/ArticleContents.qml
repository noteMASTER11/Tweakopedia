import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property var sections: []
    property string activeSectionId: ""
    signal sectionRequested(string sectionId)

    implicitWidth: 210
    implicitHeight: contentsColumn.implicitHeight + 28
    radius: 10
    color: FluentTheme.surface
    border.width: 1
    border.color: FluentTheme.stroke

    ColumnLayout {
        id: contentsColumn
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: "Содержание статьи"
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Repeater {
            model: root.sections

            delegate: AbstractButton {
                id: contentsButton
                required property var modelData
                readonly property bool current: root.activeSectionId === modelData.id
                objectName: "articleContents_" + modelData.id
                Layout.fillWidth: true
                implicitHeight: 34
                leftPadding: 10
                rightPadding: 10
                topPadding: 6
                bottomPadding: 6
                focusPolicy: Qt.StrongFocus
                Accessible.name: "Перейти к разделу: " + modelData.title
                Accessible.description: current ? "Текущий раздел" : ""
                onClicked: root.sectionRequested(modelData.id)

                contentItem: Text {
                    text: modelData.title
                    color: contentsButton.current || contentsButton.hovered
                        ? FluentTheme.accent : FluentTheme.textSecondary
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                }

                background: Rectangle {
                    radius: 5
                    color: contentsButton.current
                        ? FluentTheme.selected
                        : (contentsButton.hovered || contentsButton.activeFocus
                            ? FluentTheme.hover : "transparent")
                    border.width: contentsButton.current || contentsButton.activeFocus ? 1 : 0
                    border.color: FluentTheme.accent
                }
            }
        }
    }
}
