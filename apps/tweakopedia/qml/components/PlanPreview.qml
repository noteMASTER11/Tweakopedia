import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property bool previewReady: false
    property alias packageName: packageNameField.text
    property string summary: ""
    property var operations: []
    readonly property var firstOperation: operations.length > 0 ? operations[0] : ({})
    signal previewRequested()
    signal applyConfirmed(string packageName)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "Предварительный итог"; font.pixelSize: 20; font.weight: Font.DemiBold }
        TextField {
            id: packageNameField
            objectName: "packageNameField"
            placeholderText: "Имя пакета"
            Layout.fillWidth: true
        }
        Button { text: "Пересчитать итог"; onClicked: root.previewRequested() }
        Label { text: root.summary; visible: root.previewReady }
        GridLayout {
            visible: root.previewReady && root.operations.length > 0
            columns: 2
            Label { text: "Параметр" }
            Label { text: root.firstOperation.title || "" }
            Label { text: "Исходное состояние" }
            Label { objectName: "beforeStateText"; text: root.firstOperation.beforeState || "" }
            Label { text: "Целевое состояние" }
            Label { objectName: "targetStateText"; text: root.firstOperation.targetState || "" }
            Label { text: "Объект реестра" }
            Label {
                objectName: "registryObjectText"
                text: root.firstOperation.registryObject || ""
                wrapMode: Text.WrapAnywhere
                Layout.maximumWidth: 520
            }
            Label { text: "Перезапуск" }
            Label { objectName: "restartText"; text: root.firstOperation.restart || "" }
        }
        Button {
            objectName: "applyButton"
            text: "Применить пакет"
            enabled: root.previewReady && root.operations.length > 0 && packageNameField.text.trim().length > 0
            onClicked: root.applyConfirmed(packageNameField.text.trim())
        }
    }
}
