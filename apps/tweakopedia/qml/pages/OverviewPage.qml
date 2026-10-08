import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    required property var controller
    padding: 28

    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        Label { text: "Обзор"; font.pixelSize: 28; font.weight: Font.DemiBold }
        Label {
            text: "Локальная энциклопедия параметров Windows и очередь выбранных изменений."
            color: palette.mid
        }
        Frame {
            Layout.fillWidth: true
            visible: controller.history.interruptedCount > 0
            Label {
                anchors.fill: parent
                text: "Обнаружены незавершённые транзакции: " + controller.history.interruptedCount
                color: "#9A5B00"
            }
        }
        Frame {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                Label { text: "Твиков в каталоге"; Layout.fillWidth: true }
                Label { text: controller.tweaks.rowCount(); font.pixelSize: 24 }
            }
        }
        Frame {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                Label { text: "Изменений в очереди"; Layout.fillWidth: true }
                Label { text: controller.queue.rowCount(); font.pixelSize: 24 }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
