import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Page {
    id: root
    required property var controller
    padding: 28

    background: Rectangle {
        objectName: "overviewBackground"
        color: FluentTheme.canvas
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Text { text: "Обзор"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
        Text {
            text: "Локальная энциклопедия параметров Windows и очередь выбранных изменений."
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: warningText.implicitHeight + 24
            visible: root.controller.history.interruptedCount > 0
            color: "#FFF4CE"
            radius: 8
            border.color: "#E5C365"
            Text {
                id: warningText
                anchors.fill: parent
                anchors.margins: 12
                text: "Обнаружены незавершённые транзакции: " + root.controller.history.interruptedCount
                color: "#7A5412"
                font.family: FluentTheme.fontFamily
                wrapMode: Text.WordWrap
            }
        }

        Text {
            objectName: "overviewEmptyState"
            visible: root.controller.tweaks.rowCount() === 0
            text: "Каталог пока пуст. Добавленные определения появятся здесь автоматически."
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 8
                color: FluentTheme.surface
                border.color: FluentTheme.stroke
                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    Text { text: "Твиков в каталоге"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
                    Text { text: root.controller.tweaks.rowCount(); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 110
                radius: 8
                color: FluentTheme.surface
                border.color: FluentTheme.stroke
                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    Text { text: "Изменений в очереди"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
                    Text { text: root.controller.queue.count; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
