import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property string title: "Ход выполнения"
    property string outputText: ""
    property string placeholderText: "Вывод появится после запуска."
    property string terminalObjectName: "liveCommandOutput"
    property string copyButtonObjectName: "liveCommandCopyButton"
    property string logButtonObjectName: "liveCommandLogButton"
    property string logButtonText: "Открыть журнал"
    property bool busy: false
    property bool logEnabled: false
    signal copyRequested()
    signal openLogRequested()

    implicitHeight: 210
    radius: 10
    color: FluentTheme.surface
    border.width: 1
    border.color: FluentTheme.stroke

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 9

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 14
                color: root.busy ? FluentTheme.selected : FluentTheme.surfaceInset

                Text {
                    anchors.centerIn: parent
                    text: root.busy ? "\uE895" : "\uE9D5"
                    color: root.busy ? FluentTheme.accent : FluentTheme.textSecondary
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 13
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                FluentText {
                    Layout.fillWidth: true
                    text: root.title
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                FluentText {
                    Layout.fillWidth: true
                    text: root.busy ? "Поток обновляется во время выполнения"
                                    : "Последний вывод команды"
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 11
                }
            }

            AbstractButton {
                id: copyButton
                objectName: root.copyButtonObjectName
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                enabled: root.outputText.length > 0
                Accessible.name: "Копировать вывод"
                ToolTip.visible: hovered
                ToolTip.text: "Копировать вывод"
                onClicked: root.copyRequested()
                contentItem: Text {
                    text: "\uE8C8"
                    color: copyButton.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: copyButton.pressed ? FluentTheme.selected
                        : copyButton.hovered ? FluentTheme.hover : "transparent"
                    border.width: 1
                    border.color: copyButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                }
            }

            AbstractButton {
                id: logButton
                objectName: root.logButtonObjectName
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                enabled: root.logEnabled
                Accessible.name: root.logButtonText
                ToolTip.visible: hovered
                ToolTip.text: root.logButtonText
                onClicked: root.openLogRequested()
                contentItem: Text {
                    text: "\uE8A5"
                    color: logButton.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: logButton.pressed ? FluentTheme.selected
                        : logButton.hovered ? FluentTheme.hover : "transparent"
                    border.width: 1
                    border.color: logButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 7
            color: FluentTheme.surfaceInset
            border.width: 1
            border.color: root.busy ? FluentTheme.accent : FluentTheme.stroke

            Flickable {
                id: outputScroll
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                contentWidth: width
                contentHeight: Math.max(height, outputTextArea.implicitHeight + 20)
                boundsBehavior: Flickable.StopAtBounds
                property bool following: true

                onContentYChanged: {
                    const maximum = Math.max(0, contentHeight - height)
                    following = maximum - contentY < 4
                }

                TextArea {
                    id: outputTextArea
                    objectName: root.terminalObjectName
                    x: 10
                    y: 7
                    width: outputScroll.width - 20
                    height: Math.max(outputScroll.height - 14, implicitHeight)
                    text: root.outputText
                    placeholderText: root.placeholderText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: FluentTheme.textPrimary
                    placeholderTextColor: FluentTheme.textSecondary
                    selectionColor: FluentTheme.accent
                    selectedTextColor: "white"
                    font.family: "Cascadia Mono, Consolas"
                    font.pixelSize: 11
                    background: null

                    onTextChanged: {
                        if (outputScroll.following) {
                            Qt.callLater(function() {
                                outputScroll.contentY = Math.max(0,
                                    outputScroll.contentHeight - outputScroll.height)
                            })
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            }
        }
    }
}
