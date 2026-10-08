import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Page {
    id: root
    required property string pageTitle
    property string description: "Раздел будет добавлен на следующем этапе."
    padding: 28
    background: Rectangle { color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Text {
            text: root.pageTitle
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }
        Text {
            text: root.description
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            Layout.maximumWidth: 560
        }
        Item { Layout.fillHeight: true }
    }
}
