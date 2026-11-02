import QtQuick
import QtQuick.Controls.Basic
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    width: 1280
    height: 900
    visible: true
    color: FluentTheme.canvas

    QtObject {
        id: controller
        property bool debugLoggingEnabled: true
        property string appearanceMode: "dark"
        property string lightThemeId: "light_cool"
        property string darkThemeId: "dark_graphite"
        property string accentId: "violet"
        property string fontId: "segoe"
        property real uiScale: 1.0
        property bool uiScaleRestartRequired: false
        property string motionMode: "off"
        function setDebugLoggingEnabled(value) { debugLoggingEnabled = value; return true }
        function openLogsDirectory() { return true }
        function setAppearanceMode(value) { appearanceMode = value; return true }
        function setLightThemeId(value) { lightThemeId = value; return true }
        function setDarkThemeId(value) { darkThemeId = value; return true }
        function setAccentId(value) { accentId = value; return true }
        function setFontId(value) { fontId = value; return true }
        function setUiScale(value) { uiScale = value; return true }
        function setMotionMode(value) { motionMode = value; return true }
    }

    Component.onCompleted: FluentTheme.configure("dark_graphite", "violet", 1.0, false)
    SettingsPage { anchors.fill: parent; controller: controller }
}
