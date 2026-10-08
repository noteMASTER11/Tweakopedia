import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property string title: ""
    property string summary: ""
    property string currentState: ""
    property string targetState: ""
    property bool supported: true
    property string impact: "low"
    property string restart: "none"
    signal explanationRequested()
    signal targetSelected(string state)

    implicitHeight: content.implicitHeight + 24
    enabled: supported

    RowLayout {
        id: content
        anchors.fill: parent
        spacing: 14
        ColumnLayout {
            Layout.fillWidth: true
            Label {
                objectName: "titleLabel"
                text: root.title
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Label {
                text: root.summary
                color: palette.mid
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Label { text: "Текущее:" }
                Label { objectName: "currentStateLabel"; text: root.currentState; font.weight: Font.DemiBold }
                Label { text: "→" }
                Label { text: root.targetState || "не выбрано" }
            }
        }
        StatusBadge { text: root.supported ? "Поддерживается" : "Недоступно"; positive: root.supported }
        StatusBadge { text: root.impact; positive: root.impact === "low" }
        ComboBox {
            id: selector
            objectName: "targetSelector"
            model: [{title: "Выключено", value: "disabled"}, {title: "Включено", value: "enabled"}]
            textRole: "title"
            currentIndex: root.targetState === "enabled" ? 1 : root.targetState === "disabled" ? 0 : -1
            onActivated: index => root.targetSelected(model[index].value)
        }
        Button {
            objectName: "explanationButton"
            text: "?"
            font.bold: true
            Accessible.name: "Подробное объяснение"
            onClicked: root.explanationRequested()
        }
    }
}
