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

    function test_cardHoverAndClickOpenExplanation() {
        const row = createTemporaryObject(rowComponent, this)
        compare(findChild(row, "titleLabel").text, "Поддержка длинных путей Win32")
        compare(findChild(row, "explanationButton"), null)

        const border = findChild(row, "accentBorder")
        verify(border)
        compare(border.visible, false)

        mouseMove(row, 12, 12)
        tryCompare(border, "visible", true)

        const spy = signalSpy.createObject(row, {target: row, signalName: "explanationRequested"})
        mouseClick(row, 12, 12)
        compare(spy.count, 1)
        compare(row.activeFocus, true)

        mouseMove(this, width - 1, height - 1)
        tryCompare(border, "visible", false)
    }

    function test_keyboardOpensExplanationFromFocusedCard() {
        const row = createTemporaryObject(rowComponent, this)
        const spy = signalSpy.createObject(row, {target: row, signalName: "explanationRequested"})

        row.forceActiveFocus()
        keyClick(Qt.Key_Return)
        keyClick(Qt.Key_Space)

        compare(spy.count, 2)
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
        const explanationSpy = signalSpy.createObject(row, {
            target: row,
            signalName: "explanationRequested"
        })

        mouseClick(toggle)
        mouseClick(toggle)

        compare(spy.count, 2)
        compare(spy.signalArguments[0][0], "enabled")
        compare(spy.signalArguments[1][0], "disabled")
        compare(row.currentState, "disabled")
        compare(explanationSpy.count, 0)
    }

    function test_actionUsesQueueButtonInsteadOfStateSelector() {
        const row = createTemporaryObject(rowComponent, this, {
            action: true,
            binary: false,
            currentState: "installed",
            availableStates: [{"id": "remove", "title": "Удалить"}]
        })
        const actionButton = findChild(row, "actionQueueButton")
        verify(actionButton)
        compare(actionButton.visible, true)
        compare(findChild(row, "stateSelector").visible, false)
        compare(findChild(row, "currentStateLabel").visible, false)

        const spy = signalSpy.createObject(row, {target: row, signalName: "targetSelected"})
        mouseClick(actionButton)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "remove")
    }

    function test_parameterizedTargetUsesEditorAndKeepsCardBounds() {
        const row = createTemporaryObject(rowComponent, this, {
            binary: false,
            availableStates: [{id: "configured", title: "Настроено",
                               inputIds: ["manufacturer"]}],
            inputs: [{id: "manufacturer", label: "Производитель", type: "text",
                      required: true, maxLength: 64}]
        })
        const initialHeight = row.height
        const spy = signalSpy.createObject(row, {
            target: row, signalName: "parameterizedTargetSelected"
        })
        const explanationSpy = signalSpy.createObject(row, {
            target: row, signalName: "explanationRequested"
        })
        row.chooseTarget("configured")
        const panel = findChild(row, "tweakInputPanel")
        tryCompare(panel, "visible", true)
        verify(row.height >= initialHeight)
        verify(panel.y + panel.height + 16 <= row.height)
        const field = findChild(row, "textInputField")
        field.text = "Tweakopedia"
        field.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "configured")
        compare(spy.signalArguments[0][1].manufacturer, "Tweakopedia")
        compare(explanationSpy.count, 0)
        verify(panel.x >= 0)
        verify(panel.x + panel.width <= row.width)
    }

    function test_stateWithoutInputsQueuesImmediately() {
        const row = createTemporaryObject(rowComponent, this, {
            binary: false,
            availableStates: [
                {id: "cleared", title: "Очистить", inputIds: []},
                {id: "configured", title: "Настроить", inputIds: ["manufacturer"]}
            ],
            inputs: [{id: "manufacturer", label: "Производитель", type: "text",
                      required: true, maxLength: 64}]
        })
        const directSpy = signalSpy.createObject(row, {
            target: row, signalName: "targetSelected"
        })

        row.chooseTarget("cleared")

        compare(directSpy.count, 1)
        compare(directSpy.signalArguments[0][0], "cleared")
        compare(findChild(row, "tweakInputPanel").visible, false)
    }

    Component { id: signalSpy; SignalSpy {} }
}
