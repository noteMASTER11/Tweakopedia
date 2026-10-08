import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "TweakRow"
    when: windowShown
    visible: true
    width: 900
    height: 240

    readonly property var binaryStates: [
        {"id": "disabled", "title": "Выключено"},
        {"id": "enabled", "title": "Включено"}
    ]

    Component {
        id: rowComponent
        TweakRow {
            width: 860
            title: "Поддержка длинных путей Win32"
            summary: "Описание параметра"
            currentState: "disabled"
            currentStateTitle: "выключено"
            targetState: ""
            targetStateTitle: ""
            availableStates: binaryStates
            binary: true
            pending: false
            supported: true
        }
    }

    function test_stateTable_data() {
        return [
            {tag: "disabled-neutral", current: "disabled", currentTitle: "выключено",
             target: "", targetTitle: "", pending: false, toggle: false, enabled: true},
            {tag: "disabled-to-enabled", current: "disabled", currentTitle: "выключено",
             target: "enabled", targetTitle: "включено", pending: true, toggle: true, enabled: true},
            {tag: "enabled-neutral", current: "enabled", currentTitle: "включено",
             target: "", targetTitle: "", pending: false, toggle: true, enabled: true},
            {tag: "enabled-to-disabled", current: "enabled", currentTitle: "включено",
             target: "disabled", targetTitle: "выключено", pending: true, toggle: false, enabled: true},
            {tag: "unknown", current: "unknown", currentTitle: "не определено",
             target: "", targetTitle: "", pending: false, toggle: false, enabled: false}
        ]
    }

    function test_stateTable(data) {
        const row = createTemporaryObject(rowComponent, this, {
            currentState: data.current,
            currentStateTitle: data.currentTitle,
            targetState: data.target,
            targetStateTitle: data.targetTitle,
            pending: data.pending
        })
        verify(row)

        const toggle = findChild(row, "binaryToggle")
        compare(toggle.checked, data.toggle)
        compare(toggle.enabled, data.enabled)

        const current = findChild(row, "currentStateLabel")
        compare(current.prefix, data.pending ? "После применения:" : "Сейчас:")
        compare(current.stateTitle, data.pending ? data.targetTitle : data.currentTitle)
        compare(findChild(current, "stateValueLabel").font.weight, Font.DemiBold)

        const background = findChild(row, "rowBackground")
        compare(background.color, FluentTheme.surface)

        const gradient = findChild(row, "pendingGradient")
        verify(gradient)
        compare(gradient.visible, data.pending)
        if (data.pending) {
            fuzzyCompare(gradient.width, row.width, 0.01)
            compare(gradient.fadeEnd, 0.5)
        }

        compare(findChild(row, "targetStateLabel"), null)
        compare(findChild(row, "pendingStripe"), null)
        compare(findChild(row, "changedBadge"), null)
    }

    function test_binaryStateWordsAreLowercase() {
        const row = createTemporaryObject(rowComponent, this, {
            currentStateTitle: "Выключено",
            targetState: "enabled",
            targetStateTitle: "Включено",
            pending: true
        })
        const state = findChild(row, "currentStateLabel")
        compare(state.prefix, "После применения:")
        compare(state.stateTitle, "включено")
    }

    function test_unsupportedRowKeepsReasonAndDisablesToggle() {
        const row = createTemporaryObject(rowComponent, this, {
            supported: false,
            supportDetails: "Компонент отсутствует"
        })
        compare(findChild(row, "binaryToggle").enabled, false)
        compare(findChild(row, "supportDetailsLabel").text, "Компонент отсутствует")
        compare(findChild(row, "supportDetailsLabel").visible, true)
    }

    function test_contentAndExplanationButton() {
        const row = createTemporaryObject(rowComponent, this)
        compare(findChild(row, "titleLabel").text, "Поддержка длинных путей Win32")
        const help = findChild(row, "explanationButton")
        verify(help)
        const spy = signalSpy.createObject(row, {target: row, signalName: "explanationRequested"})
        help.clicked()
        compare(spy.count, 1)
    }

    function test_twoRapidTogglesEmitEnabledThenDisabled() {
        const row = createTemporaryObject(rowComponent, this)
        const spy = signalSpy.createObject(row, {target: row, signalName: "targetSelected"})
        row.targetSelected.connect(function(state) {
            row.targetState = state === row.currentState ? "" : state
            row.targetStateTitle = state === "enabled" ? "включено" : ""
            row.pending = row.targetState !== ""
        })
        const toggle = findChild(row, "binaryToggle")

        mouseClick(toggle)
        mouseClick(toggle)

        compare(spy.count, 2)
        compare(spy.signalArguments[0][0], "enabled")
        compare(spy.signalArguments[1][0], "disabled")
        compare(row.currentState, "disabled")
    }

    Component { id: signalSpy; SignalSpy {} }
}
