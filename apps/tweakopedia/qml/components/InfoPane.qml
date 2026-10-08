import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

FocusScope {
    id: root

    property var explanation: ({})
    property bool docked: false
    property bool opened: false
    property bool technicalExpanded: false
    property var returnFocusItem: null
    signal closed()

    function show(data, trigger) {
        explanation = data || ({})
        returnFocusItem = trigger || null
        technicalExpanded = false
        opened = true
        if (!docked)
            forceActiveFocus()
    }

    function closePane() {
        if (!opened)
            return
        opened = false
        closed()
        if (returnFocusItem)
            returnFocusItem.forceActiveFocus()
    }

    visible: opened
    implicitWidth: 420
    focus: opened && !docked
    z: docked ? 1 : 100
    Keys.onEscapePressed: event => {
        if (!docked) {
            closePane()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: FluentTheme.surface
        border.width: 1
        border.color: FluentTheme.stroke
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    id: titleLabel
                    objectName: "infoTitle"
                    Layout.fillWidth: true
                    text: root.explanation.title || "Пояснение"
                    color: FluentTheme.textPrimary
                    wrapMode: Text.WordWrap
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                InfoButton {
                    text: "×"
                    Accessible.name: "Закрыть пояснение"
                    onClicked: root.closePane()
                }
            }

            Text { text: "Назначение"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "purposeText"; text: root.explanation.purpose || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
            Text { text: "Механизм"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "mechanismText"; text: root.explanation.mechanism || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
            Text { text: "Эффект"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "effectText"; text: root.explanation.effect || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
            Text { text: "Ограничения"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "tradeoffsText"; text: root.explanation.tradeoffs || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
            Text { text: "Рекомендация"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "recommendationText"; text: root.explanation.recommendation || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }

            Button {
                id: technicalButton
                objectName: "technicalToggle"
                Layout.fillWidth: true
                text: (root.technicalExpanded ? "▾ " : "▸ ") + "Технические сведения"
                onClicked: root.technicalExpanded = !root.technicalExpanded
                contentItem: Text {
                    text: technicalButton.text
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.weight: Font.DemiBold
                }
                background: Rectangle { color: FluentTheme.surfaceInset; radius: 4 }
            }
            Text {
                objectName: "technicalDetailsText"
                visible: root.technicalExpanded
                text: root.explanation.technicalDetails || ""
                color: FluentTheme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.family: FluentTheme.fontFamily
            }
            Text { text: "Объект реестра"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "registryObjectText"; text: root.explanation.registryObject || ""; color: FluentTheme.textSecondary; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.family: "Consolas" }
            Text { text: "Возврат"; color: FluentTheme.textPrimary; font.weight: Font.DemiBold; font.family: FluentTheme.fontFamily }
            Text { objectName: "rollbackText"; text: root.explanation.rollback || ""; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
            Item { Layout.fillHeight: true; implicitHeight: 8 }
        }
    }
}
