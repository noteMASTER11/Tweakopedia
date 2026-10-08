import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root

    property bool previewReady: false
    property alias packageName: packageNameField.text
    property string summary: ""
    property var operations: []
    readonly property var firstOperation: operations.length > 0 ? operations[0] : ({})
    signal previewRequested()
    signal applyConfirmed(string packageName)

    implicitHeight: content.implicitHeight + 32
    height: implicitHeight

    Rectangle {
        objectName: "previewSurface"
        anchors.fill: parent
        radius: 8
        color: FluentTheme.surface
        border.color: FluentTheme.stroke
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text { text: "Предварительный итог"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 20; font.weight: Font.DemiBold }
        TextField {
            id: packageNameField
            objectName: "packageNameField"
            placeholderText: "Имя пакета"
            Layout.fillWidth: true
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            background: Rectangle { radius: 5; color: FluentTheme.surfaceInset; border.color: packageNameField.activeFocus ? FluentTheme.accent : FluentTheme.stroke }
        }

        RowLayout {
            Layout.fillWidth: true
            Button { text: "Пересчитать итог"; onClicked: root.previewRequested() }
            Text { Layout.fillWidth: true; text: root.summary; visible: root.previewReady; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; wrapMode: Text.WordWrap }
        }

        GridLayout {
            visible: root.previewReady && root.operations.length > 0
            columns: 2
            columnSpacing: 14
            rowSpacing: 6
            Text { text: "Параметр"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "operationTitle"; text: root.firstOperation.title || ""; color: FluentTheme.textPrimary; wrapMode: Text.WordWrap; Layout.maximumWidth: 520; font.family: FluentTheme.fontFamily }
            Text { text: "Исходное состояние"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "beforeStateText"; text: root.firstOperation.beforeState || ""; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily }
            Text { text: "Целевое состояние"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "targetStateText"; text: root.firstOperation.targetState || ""; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily }
            Text { text: "Объект реестра"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "registryObjectText"; text: root.firstOperation.registryObject || ""; color: FluentTheme.textPrimary; wrapMode: Text.WrapAnywhere; Layout.maximumWidth: 520; font.family: "Consolas" }
            Text { text: "Перезапуск"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily }
            Text { objectName: "restartText"; text: root.firstOperation.restart || ""; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily }
        }

        Button {
            id: applyButton
            objectName: "applyButton"
            text: "Применить пакет"
            enabled: root.previewReady && root.operations.length > 0 && packageNameField.text.trim().length > 0
            onClicked: root.applyConfirmed(packageNameField.text.trim())
            contentItem: Text { text: applyButton.text; color: applyButton.enabled ? "white" : FluentTheme.disabledText; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.family: FluentTheme.fontFamily; font.weight: Font.DemiBold }
            background: Rectangle { implicitWidth: 150; implicitHeight: 36; radius: 5; color: applyButton.enabled ? (applyButton.hovered ? FluentTheme.accentHover : FluentTheme.accent) : FluentTheme.disabledSurface }
        }
    }
}
