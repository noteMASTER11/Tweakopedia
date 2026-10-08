import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "FluentControls"
    when: windowShown
    visible: true
    width: 640
    height: 320

    Component {
        id: toggleComponent
        FluentToggle {}
    }

    Component {
        id: labelComponent
        StateLabel {
            prefix: "Сейчас:"
            stateId: "disabled"
            stateTitle: "выключено"
        }
    }

    Component {
        id: infoComponent
        InfoButton {}
    }

    Component {
        id: selectorComponent
        FluentStateSelector {
            states: [
                {"id": "disabled", "title": "Выключено"},
                {"id": "enabled", "title": "Включено"},
                {"id": "automatic", "title": "Автоматически"}
            ]
            selectedState: "disabled"
        }
    }

    function test_toggleGeometryAndPointerActivation() {
        const control = createTemporaryObject(toggleComponent, this, {x: 100, y: 100})
        verify(control)
        const track = findChild(control, "toggleTrack")
        const thumb = findChild(control, "toggleThumb")
        compare(track.width, 40)
        compare(track.height, 20)
        compare(thumb.width, 16)
        compare(thumb.height, 16)
        compare(control.width, 40)
        compare(control.height, 20)
        const spy = signalSpy.createObject(control, {
            target: control,
            signalName: "toggledByUser"
        })
        mouseClick(control)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], true)
    }

    function test_toggleKeyboardAndDisabledBehavior() {
        const control = createTemporaryObject(toggleComponent, this)
        const spy = signalSpy.createObject(control, {
            target: control,
            signalName: "toggledByUser"
        })
        control.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], true)

        control.enabled = false
        mouseClick(control)
        keyClick(Qt.Key_Space)
        compare(spy.count, 1)
    }

    function test_stateLabelUsesTextWeightAndSemanticColors() {
        const label = createTemporaryObject(labelComponent, this)
        const value = findChild(label, "stateValueLabel")
        compare(value.text, "выключено")
        compare(value.font.weight, Font.DemiBold)
        compare(value.color, FluentTheme.stateOff)

        label.stateId = "enabled"
        label.stateTitle = "включено"
        compare(value.color, FluentTheme.stateOn)

        label.stateId = "unknown"
        label.stateTitle = "не определено"
        compare(value.color, FluentTheme.stateUnknown)
    }

    function test_infoButtonHasAccessibleNameAndSignal() {
        const button = createTemporaryObject(infoComponent, this)
        compare(button.text, "?")
        compare(button.width, 28)
        compare(button.height, 28)
        compare(button.Accessible.name, "Подробное описание")
        const spy = signalSpy.createObject(button, {target: button, signalName: "clicked"})
        mouseClick(button)
        compare(spy.count, 1)
    }

    function test_stateSelectorEmitsNamedState() {
        const selector = createTemporaryObject(selectorComponent, this)
        const spy = signalSpy.createObject(selector, {
            target: selector,
            signalName: "stateSelected"
        })
        const automatic = findChild(selector, "stateButton_automatic")
        verify(automatic)
        verify(automatic.width > 0)
        compare(automatic.height, 32)
        mouseClick(automatic)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "automatic")
    }

    Component { id: signalSpy; SignalSpy {} }
}
