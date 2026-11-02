pragma Singleton

import QtQuick
import QtQuick.Controls

QtObject {
    id: root

    property string themeId: "light_cool"
    property string accentId: "blue"
    property real uiScale: 1.0
    property bool reduceMotion: false

    readonly property bool dark: themeId === "dark_graphite" || themeId === "dark_midnight"
    readonly property bool warm: themeId === "light_warm"
    readonly property bool midnight: themeId === "dark_midnight"

    readonly property color canvas: midnight ? "#0F172A"
        : dark ? "#1F1F1F" : warm ? "#FAF8F3" : "#F6F8FC"
    readonly property color surface: midnight ? "#162033"
        : dark ? "#2A2A2A" : warm ? "#FFFDF9" : "#FFFFFF"
    readonly property color surfaceRaised: midnight ? "#1B2940"
        : dark ? "#323232" : "#FFFFFF"
    readonly property color surfaceInset: midnight ? "#111B2D"
        : dark ? "#252525" : warm ? "#F4F0E8" : "#F1F4F9"
    readonly property color navigation: midnight ? "#111A2B"
        : dark ? "#242424" : warm ? "#F5F1E9" : "#F3F6FA"
    readonly property color textPrimary: dark ? "#F4F4F4" : warm ? "#27241F" : "#20232B"
    readonly property color textSecondary: midnight ? "#AAB7CA"
        : dark ? "#B8B8B8" : warm ? "#696257" : "#5D6878"
    readonly property color stroke: midnight ? "#30415C"
        : dark ? "#454545" : warm ? "#E4DED3" : "#DDE3EC"
    readonly property color strokeStrong: midnight ? "#49617F"
        : dark ? "#606060" : warm ? "#C8BFB0" : "#BCC6D3"

    readonly property color accent: accentColor(accentId, dark)
    readonly property color accentHover: dark ? Qt.lighter(accent, 1.12) : Qt.darker(accent, 1.10)
    readonly property color accentPressed: dark ? Qt.lighter(accent, 1.22) : Qt.darker(accent, 1.22)
    readonly property color hover: midnight ? "#21304A"
        : dark ? "#363636" : warm ? "#F1ECE3" : "#EDF2F8"
    readonly property color selected: accentTint(accentId, dark, midnight)
    readonly property color disabledSurface: midnight ? "#1B2537"
        : dark ? "#303030" : warm ? "#F3EFE8" : "#F2F4F7"
    readonly property color disabledText: midnight ? "#6F7D91"
        : dark ? "#7F7F7F" : warm ? "#9B9388" : "#8B95A3"

    readonly property color stateOn: dark ? "#6CCB75" : "#107C10"
    readonly property color stateOff: dark ? "#FF7B72" : "#C42B1C"
    readonly property color stateUnknown: dark ? "#A8B0BC" : "#687487"
    readonly property color danger: stateOff
    readonly property color dangerHover: dark ? "#FF948D" : "#A4262C"
    readonly property color dangerSurface: dark ? "#3A2325" : "#FDE7E9"
    readonly property color dangerStroke: dark ? "#7C3C42" : "#F1AEB5"
    readonly property color positive: stateOn
    readonly property color positiveSurface: dark ? "#203527" : "#E6F4EA"
    readonly property color positiveStroke: dark ? "#3E6B49" : "#A9D9B2"
    readonly property color warning: dark ? "#FFB454" : "#9D5D00"
    readonly property color warningSurface: dark ? "#3A2D1D" : "#FFF4CE"
    readonly property color warningStroke: dark ? "#76572A" : "#E8C96B"
    readonly property color infoSurface: dark ? selected : "#EAF3FF"
    readonly property color infoStroke: dark ? strokeStrong : "#B7CBE7"
    readonly property color overlay: dark ? "#99000000" : "#66000000"
    readonly property color shadow: dark ? "#B0000000" : "#30000000"

    readonly property string fontFamily: Application.font.family
    readonly property int bodyFontWeight: Font.Medium
    readonly property int headingFontWeight: Font.DemiBold
    readonly property int navigationExpandedWidth: 220
    readonly property int navigationCompactWidth: 64
    readonly property int compactBreakpoint: 1100
    readonly property int animationFast: reduceMotion ? 0 : 120
    readonly property int animationNormal: reduceMotion ? 0 : 180

    function configure(nextThemeId, nextAccentId, nextScale, nextReduceMotion) {
        themeId = nextThemeId || "light_cool"
        accentId = nextAccentId || "blue"
        uiScale = nextScale || 1.0
        reduceMotion = !!nextReduceMotion
    }

    function accentColor(id, isDark) {
        if (id === "violet") return isDark ? "#A78BFA" : "#7657D5"
        if (id === "teal") return isDark ? "#4DD4C6" : "#008C82"
        if (id === "red") return isDark ? "#FF7B72" : "#C42B1C"
        if (id === "amber") return isDark ? "#FFB454" : "#B86400"
        return isDark ? "#60A5FA" : "#0067C0"
    }

    function accentTint(id, isDark, isMidnight) {
        if (isDark) {
            if (id === "violet") return isMidnight ? "#30294A" : "#372E45"
            if (id === "teal") return isMidnight ? "#173B3C" : "#253D3A"
            if (id === "red") return isMidnight ? "#3B2630" : "#422B2D"
            if (id === "amber") return isMidnight ? "#3A3023" : "#403426"
            return isMidnight ? "#1C3554" : "#29384A"
        }
        if (id === "violet") return "#EEE9FF"
        if (id === "teal") return "#DDF5F2"
        if (id === "red") return "#FDE7E9"
        if (id === "amber") return "#FFF0D5"
        return "#DEEAF8"
    }
}
