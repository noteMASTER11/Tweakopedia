import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "SettingsPage"
    when: windowShown
    visible: true
    width: 900
    height: 620

    QtObject {
        id: fakeController
        property bool debugLoggingEnabled: false
        property int setCalls: 0
        property int openCalls: 0
        property bool setResult: true
        function setDebugLoggingEnabled(enabled) {
            ++setCalls
            if (setResult)
                debugLoggingEnabled = enabled
            return setResult
        }
        function openLogsDirectory() {
            ++openCalls
            return true
        }
    }

    Component {
        id: pageComponent
        SettingsPage {
            width: 900
            height: 620
            controller: fakeController
        }
    }

    function init() {
        fakeController.debugLoggingEnabled = false
        fakeController.setCalls = 0
        fakeController.openCalls = 0
        fakeController.setResult = true
    }

    function test_presentsDebugLoggingSetting() {
        const page = createTemporaryObject(pageComponent, this)
        verify(page)
        compare(findChild(page, "settingsBackground").color, FluentTheme.canvas)
        compare(findChild(page, "settingsTitle").text, "Настройки")
        compare(findChild(page, "debugLoggingTitle").text, "Включить режим отладки")

        const toggle = findChild(page, "debugLoggingToggle")
        verify(toggle)
        compare(toggle.checked, false)
        mouseClick(toggle)
        compare(fakeController.setCalls, 1)
        compare(fakeController.debugLoggingEnabled, true)
        tryCompare(toggle, "checked", true)

        const openLogs = findChild(page, "openLogsButton")
        compare(openLogs.text, "Открыть логи")
        mouseClick(openLogs)
        compare(fakeController.openCalls, 1)
    }

    function test_failedToggleKeepsPublishedState() {
        fakeController.setResult = false
        const page = createTemporaryObject(pageComponent, this)
        const toggle = findChild(page, "debugLoggingToggle")

        mouseClick(toggle)

        compare(fakeController.setCalls, 1)
        compare(fakeController.debugLoggingEnabled, false)
        compare(toggle.checked, false)
    }
}
