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
                width: ListView.view.width
                title: model.title
                summary: model.summary
                currentState: model.currentState
                targetState: model.targetState
                supported: model.supported
                impact: model.impact
                restart: model.restart
                onTargetSelected: state => root.controller.selectTarget(model.id, state)
                onExplanationRequested: {
                    explanationDrawer.explanation = root.controller.openExplanation(model.id)
                    explanationDrawer.open()
                }
            }
        }
    }
}
