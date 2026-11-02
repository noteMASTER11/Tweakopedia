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
        property string appearanceMode: "system"
        property string lightThemeId: "light_cool"
        property string darkThemeId: "dark_graphite"
        property string accentId: "blue"
        property string fontId: "segoe"
        property real uiScale: 1.0
        property bool uiScaleRestartRequired: false
        property string motionMode: "system"
        property string effectiveThemeId: appearanceMode === "dark" ? darkThemeId : lightThemeId
        property bool effectiveDark: appearanceMode === "dark"
        property bool reduceMotion: motionMode === "on"
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
        function setAppearanceMode(value) { appearanceMode = value; return true }
        function setLightThemeId(value) { lightThemeId = value; return true }
        function setDarkThemeId(value) { darkThemeId = value; return true }
        function setAccentId(value) { accentId = value; return true }
        function setFontId(value) { fontId = value; return true }
        function setUiScale(value) { uiScale = value; return true }
        function setMotionMode(value) { motionMode = value; return true }
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
        fakeController.appearanceMode = "system"
        fakeController.lightThemeId = "light_cool"
        fakeController.darkThemeId = "dark_graphite"
        fakeController.accentId = "blue"
        fakeController.fontId = "segoe"
        fakeController.uiScale = 1.0
        fakeController.motionMode = "system"
        FluentTheme.configure("light_cool", "blue", 1.0, false)
    }

    function test_changesAppearanceControls() {
        const page = createTemporaryObject(pageComponent, this)
        verify(page)

        findChild(page, "appearanceMode_dark").clicked()
        compare(fakeController.appearanceMode, "dark")

        findChild(page, "theme_dark_midnight").clicked()
        compare(fakeController.darkThemeId, "dark_midnight")

        findChild(page, "accent_violet").clicked()
        compare(fakeController.accentId, "violet")

        findChild(page, "motion_on").clicked()
        compare(fakeController.motionMode, "on")
    }

    function test_themePresetButtonsUseEqualGeometry() {
        const page = createTemporaryObject(pageComponent, this)
        verify(page)

        const cool = findChild(page, "theme_light_cool")
        const warm = findChild(page, "theme_light_warm")
        const graphite = findChild(page, "theme_dark_graphite")
        const midnight = findChild(page, "theme_dark_midnight")
        verify(cool && warm && graphite && midnight)

        compare(cool.width, warm.width)
        compare(cool.width, graphite.width)
        compare(cool.width, midnight.width)
        compare(cool.height, 136)
        compare(warm.height, cool.height)
        compare(graphite.height, cool.height)
        compare(midnight.height, cool.height)

        const preview = findChild(page, "themePreview_dark_midnight")
        verify(preview)
        verify(preview.width <= midnight.width)
        compare(preview.clip, true)
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
        toggle.toggledByUser(true)
        compare(fakeController.setCalls, 1)
        compare(fakeController.debugLoggingEnabled, true)
        tryCompare(toggle, "checked", true)

        const openLogs = findChild(page, "openLogsButton")
        compare(openLogs.text, "Открыть логи")
        openLogs.clicked()
        compare(fakeController.openCalls, 1)
    }

    function test_failedToggleKeepsPublishedState() {
        fakeController.setResult = false
        const page = createTemporaryObject(pageComponent, this)
        const toggle = findChild(page, "debugLoggingToggle")

        toggle.toggledByUser(true)

        compare(fakeController.setCalls, 1)
        compare(fakeController.debugLoggingEnabled, false)
        compare(toggle.checked, false)
    }
}
