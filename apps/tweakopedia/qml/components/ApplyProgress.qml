import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property int progress: 0
    property string status: "idle"
    property string message: ""

    ColumnLayout {
        anchors.fill: parent
        Label { text: "Выполнение"; font.weight: Font.DemiBold }
        ProgressBar {
            objectName: "progressBar"
            from: 0
            to: 1
            value: Math.max(0, Math.min(100, root.progress)) / 100
            Layout.fillWidth: true
        }
        Label { objectName: "statusText"; text: root.status }
        Label { text: root.message; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
