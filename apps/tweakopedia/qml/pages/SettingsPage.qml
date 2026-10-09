import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    padding: 0

    background: Rectangle {
        objectName: "settingsBackground"
        color: FluentTheme.canvas
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        anchors.bottomMargin: 24
        spacing: 8

        Text {
            objectName: "settingsTitle"
            Layout.fillWidth: true
            text: "Настройки"
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: "Параметры приложения и средства диагностики."
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
        }

        Item { Layout.preferredHeight: 12 }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 106
            radius: 10
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 18

                Text {
                    text: "\uE9D9"
                    color: FluentTheme.accent
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 22
                    Layout.alignment: Qt.AlignVCenter
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        objectName: "debugLoggingTitle"
                        Layout.fillWidth: true
                        text: "Включить режим отладки"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Записывает подробные сообщения уровня DEBUG и выше в отдельный файл текущего сеанса."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }

                Button {
                    id: openLogsButton
                    objectName: "openLogsButton"
                    Layout.preferredWidth: 126
                    Layout.preferredHeight: 34
                    text: "Открыть логи"
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    onClicked: root.controller.openLogsDirectory()

                    contentItem: Text {
                        text: openLogsButton.text
                        color: FluentTheme.textPrimary
                        font: openLogsButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: openLogsButton.pressed
                            ? FluentTheme.selected
                            : openLogsButton.hovered ? FluentTheme.hover : FluentTheme.surface
                        border.width: 1
                        border.color: openLogsButton.activeFocus
                            ? FluentTheme.accent : FluentTheme.stroke
                    }
                }

                FluentToggle {
                    id: debugLoggingToggle
                    objectName: "debugLoggingToggle"
                    checked: root.controller.debugLoggingEnabled
                    Accessible.name: "Включить режим отладки"
                    onToggledByUser: function(enabled) {
                        root.controller.setDebugLoggingEnabled(enabled)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: "Файлы сохраняются в локальном каталоге данных Tweakopedia. Имя содержит дату и время запуска сеанса."
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }
    }
}
