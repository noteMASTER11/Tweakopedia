import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root
    property int progress: 0
    property string status: "idle"
    property string message: ""

    implicitHeight: content.implicitHeight + 28
    radius: 8
    color: FluentTheme.surface
    border.color: FluentTheme.stroke

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8
        Text { text: "Выполнение"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.weight: Font.DemiBold }
        ProgressBar {
            objectName: "progressBar"
            from: 0
            to: 1
            value: Math.max(0, Math.min(100, root.progress)) / 100
            Layout.fillWidth: true
        }
        Text {
            objectName: "statusText"
            text: root.status
            color: root.status === "succeeded" || root.status === "rolled_back"
                ? FluentTheme.stateOn
                : root.status === "failed" || root.status === "cancelled"
                    ? FluentTheme.stateOff : FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.weight: Font.DemiBold
        }
        Text { text: root.message; color: FluentTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.family: FluentTheme.fontFamily }
    }
}
