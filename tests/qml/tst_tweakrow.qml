import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "TweakRow"
    when: windowShown

    width: 900
    height: 180

    Component {
        id: rowComponent
        TweakRow {
            width: 860
            title: "Поддержка длинных путей Win32"
            summary: "Описание параметра"
            currentState: "disabled"
            targetState: ""
            supported: true
        }
    }

    function test_contentAndExplanationButton() {
        const row = createTemporaryObject(rowComponent, this)
        verify(row)
        compare(findChild(row, "titleLabel").text, "Поддержка длинных путей Win32")
        compare(findChild(row, "currentStateLabel").text, "disabled")
        const help = findChild(row, "explanationButton")
        verify(help)
        compare(help.text, "?")
        const spy = signalSpy.createObject(row, {target: row, signalName: "explanationRequested"})
        help.clicked()
        compare(spy.count, 1)
    }

    function test_targetSelectionOnlyEmitsQueueSignal() {
        const row = createTemporaryObject(rowComponent, this)
        const spy = signalSpy.createObject(row, {target: row, signalName: "targetSelected"})
        const selector = findChild(row, "targetSelector")
        selector.activated(1)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "enabled")
        compare(row.currentState, "disabled")
    }

    Component { id: signalSpy; SignalSpy {} }
}
