import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root
    property var explanation: ({})
    edge: Qt.RightEdge
    width: Math.min(620, parent ? parent.width * 0.72 : 620)
    height: parent ? parent.height : 700
    modal: true

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                text: root.explanation.title || "Объяснение"
                font.pixelSize: 24
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Label { text: "Назначение"; font.weight: Font.DemiBold }
            Text {
                objectName: "purposeText"; text: root.explanation.purpose || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Механизм"; font.weight: Font.DemiBold }
            Text {
                objectName: "mechanismText"; text: root.explanation.mechanism || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Эффект"; font.weight: Font.DemiBold }
            Text {
                objectName: "effectText"; text: root.explanation.effect || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Ограничения"; font.weight: Font.DemiBold }
            Text {
                objectName: "tradeoffsText"; text: root.explanation.tradeoffs || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Рекомендация"; font.weight: Font.DemiBold }
            Text {
                objectName: "recommendationText"; text: root.explanation.recommendation || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Технические детали"; font.weight: Font.DemiBold }
            Text {
                objectName: "technicalDetailsText"; text: root.explanation.technicalDetails || ""
                textFormat: Text.MarkdownText; wrapMode: Text.WordWrap; color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Объект реестра"; font.weight: Font.DemiBold }
            Text {
                objectName: "registryObjectText"
                text: root.explanation.registryObject || ""
                font.family: "Consolas"
                wrapMode: Text.WrapAnywhere
                color: palette.text
                Layout.fillWidth: true
            }
            Label { text: "Возврат"; font.weight: Font.DemiBold }
            Text {
                objectName: "rollbackText"
                text: root.explanation.rollback || ""
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                color: palette.text
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true; implicitHeight: 24 }
        }
    }
}
