import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "TweakGroupStrip"
    when: windowShown
    visible: true
    width: 640
    height: 320

    ListModel { id: groups }

    Component {
        id: stripComponent
        TweakGroupStrip {
            width: 440
            height: 40
            model: groups
            currentGroup: ""
        }
    }

    function populate() {
        groups.clear()
        groups.append({id: "", title: "Все", count: 7, enabled: true, selected: true})
        groups.append({id: "windows", title: "Окна и запуск программ", count: 3,
                       enabled: true, selected: false})
        groups.append({id: "input", title: "Ввод", count: 2,
                       enabled: true, selected: false})
        groups.append({id: "media", title: "Носители", count: 2,
                       enabled: true, selected: false})
    }

    function init() { populate() }

    function test_singleRowLabelsSelectionAndCircularArrows() {
        const strip = createTemporaryObject(stripComponent, this)
        verify(strip)
        const list = findChild(strip, "groupHorizontalList")
        tryCompare(list, "count", 4)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        compare(strip.height, strip.implicitHeight)
        compare(list.itemAtIndex(0).text, "Все")
        compare(list.itemAtIndex(1).text, "Окна и запуск программ")

        const selected = list.itemAtIndex(0)
        compare(selected.background.color, FluentTheme.selected)
        const previous = findChild(strip, "groupPreviousButton")
        const next = findChild(strip, "groupNextButton")
        compare(previous.width, previous.height)
        compare(previous.background.radius, previous.width / 2)
        compare(next.width, next.height)
        compare(next.background.radius, next.width / 2)
        compare(previous.enabled, false)
        compare(next.enabled, true)
    }

    function test_pillsAndArrowsEmitSelectionIntent() {
        const strip = createTemporaryObject(stripComponent, this)
        const selectedSpy = signalSpy.createObject(strip, {
            target: strip, signalName: "groupSelected"
        })
        const stepSpy = signalSpy.createObject(strip, {
            target: strip, signalName: "stepRequested"
        })
        const list = findChild(strip, "groupHorizontalList")
        tryVerify(function() { return list.itemAtIndex(2) !== null })

        mouseClick(list.itemAtIndex(2))
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], "input")
        mouseClick(findChild(strip, "groupNextButton"))
        compare(stepSpy.count, 1)
        compare(stepSpy.signalArguments[0][0], 1)

        strip.currentGroup = "media"
        tryCompare(findChild(strip, "groupNextButton"), "enabled", false)
        mouseClick(findChild(strip, "groupPreviousButton"))
        compare(stepSpy.count, 2)
        compare(stepSpy.signalArguments[1][0], -1)
    }

    function test_selectedTabIsBroughtIntoViewAfterSelectionAndReset() {
        const strip = createTemporaryObject(stripComponent, this, {width: 230})
        const list = findChild(strip, "groupHorizontalList")
        strip.currentGroup = "media"
        tryCompare(list, "currentIndex", 3)
        tryVerify(function() { return list.contentX > 0 })

        groups.clear()
        groups.append({id: "", title: "Все", count: 2, enabled: true, selected: false})
        groups.append({id: "long", title: "Очень длинное название группы параметров Windows",
                       count: 2, enabled: true, selected: true})
        strip.currentGroup = "long"
        tryCompare(list, "currentIndex", 1)
        const tab = list.itemAtIndex(1)
        verify(tab)
        const label = findChild(tab, "groupTabLabel_long")
        verify(label.truncated)
        compare(tab.fullTitleTooltip, "Очень длинное название группы параметров Windows")
    }

    function test_wheelScrollIsConsumedByHorizontalStrip() {
        const host = createTemporaryObject(wheelHostComponent, this)
        const strip = findChild(host, "wheelStrip")
        const list = findChild(strip, "groupHorizontalList")
        const vertical = host
        const oldVertical = vertical.contentY
        mouseWheel(strip, 110, 20, 0, -120, Qt.NoButton, Qt.NoModifier)
        tryVerify(function() { return list.contentX > 0 })
        compare(vertical.contentY, oldVertical)
    }

    function test_horizontalTouchpadGestureScrollsTheStrip() {
        const host = createTemporaryObject(wheelHostComponent, this)
        const strip = findChild(host, "wheelStrip")
        const list = findChild(strip, "groupHorizontalList")
        tryVerify(function() { return list.contentWidth > list.width })
        strip.handleWheel(-120, 0, 0, 0)
        tryVerify(function() { return list.contentX > 0 })
    }

    Component {
        id: wheelHostComponent
        Flickable {
            objectName: "verticalSentinel"
            width: 260
            height: 120
            contentHeight: 600
            TweakGroupStrip {
                objectName: "wheelStrip"
                width: parent.width
                height: 40
                model: groups
                currentGroup: ""
            }
        }
    }

    Component { id: signalSpy; SignalSpy {} }
}
