import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Page {
    id: root
    required property var controller
    padding: 24

    ExplanationDrawer { id: explanationDrawer }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        Label { text: "Твики"; font.pixelSize: 28; font.weight: Font.DemiBold }
        Label {
            text: "Выбор состояния добавляет изменение в очередь. Фактическая система пока не меняется."
            color: palette.mid
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: root.controller.tweaks
            delegate: TweakRow {
                required property string id
                required property string title
                required property string summary
                required property string currentState
                required property string targetState
                required property bool supported
                required property string impact
                required property string restart
                width: ListView.view.width
                onTargetSelected: state => root.controller.selectTarget(id, state)
                onExplanationRequested: {
                    explanationDrawer.explanation = root.controller.openExplanation(id)
                    explanationDrawer.open()
                }
            }
        }
    }
}
