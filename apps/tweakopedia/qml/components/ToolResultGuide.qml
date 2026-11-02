import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property string title: "Если найдены проблемы"
    property string description: ""
    property var actions: []
    property string articleId: ""
    readonly property bool compactAction: width < 660
    signal articleRequested(string id)

    implicitHeight: guideLayout.implicitHeight + 28
    radius: 10
    color: FluentTheme.infoSurface
    border.width: 1
    border.color: FluentTheme.infoStroke

    ColumnLayout {
        id: guideLayout
        anchors.fill: parent
        anchors.margins: 14
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 14
                color: FluentTheme.selected

                Text {
                    anchors.centerIn: parent
                    text: "\uE946"
                    color: FluentTheme.accent
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                }
            }

            FluentText {
                Layout.fillWidth: true
                text: root.title
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Button {
                id: articleButton
                objectName: "toolResultGuideArticleButton"
                visible: root.articleId.length > 0
                Layout.preferredWidth: root.compactAction ? 40 : 176
                Layout.preferredHeight: 40
                text: "Подробнее в Твикопедии"
                Accessible.name: text
                ToolTip.visible: root.compactAction && hovered
                ToolTip.text: text
                onClicked: root.articleRequested(root.articleId)

                contentItem: RowLayout {
                    spacing: 7
                    FluentText {
                        visible: !root.compactAction
                        text: articleButton.text
                        color: FluentTheme.accent
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "\uE72A"
                        color: FluentTheme.accent
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 10
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    }
                }
                background: Rectangle {
                    radius: 6
                    color: articleButton.pressed ? FluentTheme.selected
                        : articleButton.hovered ? FluentTheme.hover : "transparent"
                    border.width: articleButton.activeFocus ? 2 : 1
                    border.color: articleButton.activeFocus
                        ? FluentTheme.accent : FluentTheme.infoStroke
                }
            }
        }

        FluentText {
            Layout.fillWidth: true
            text: root.description
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 12
            lineHeight: 1.15
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.actions

            delegate: RowLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                spacing: 9

                Rectangle {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    radius: 10
                    color: FluentTheme.accent

                    FluentText {
                        anchors.centerIn: parent
                        text: String(index + 1)
                        color: "white"
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }

                FluentText {
                    Layout.fillWidth: true
                    text: String(modelData)
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
