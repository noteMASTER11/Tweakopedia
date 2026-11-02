pragma Singleton

import QtQuick
import QtQuick.Controls

QtObject {
    id: root

    property string themeId: "light_cool"
    property string accentId: "blue"
    property real uiScale: 1.0
    property bool reduceMotion: false

    readonly property bool dark: themeId.indexOf("dark_") === 0
    readonly property bool midnight: themeId === "dark_midnight"
    readonly property var palette: paletteFor(themeId)

    readonly property color canvas: palette.canvas
    readonly property color surface: palette.surface
    readonly property color surfaceRaised: palette.surfaceRaised
    readonly property color surfaceInset: palette.surfaceInset
    readonly property color navigation: palette.navigation
    readonly property color textPrimary: palette.textPrimary
    readonly property color textSecondary: palette.textSecondary
    readonly property color stroke: palette.stroke
    readonly property color strokeStrong: palette.strokeStrong

    readonly property color accent: accentColor(accentId, dark)
    readonly property color accentHover: dark ? Qt.lighter(accent, 1.12) : Qt.darker(accent, 1.10)
    readonly property color accentPressed: dark ? Qt.lighter(accent, 1.22) : Qt.darker(accent, 1.22)
    readonly property color hover: palette.hover
    readonly property color selected: accentTint(accentId, dark, midnight)
    readonly property color disabledSurface: palette.disabledSurface
    readonly property color disabledText: palette.disabledText

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

    function paletteFor(id) {
        if (id === "light_warm") return {
            canvas: "#FAF8F3", surface: "#FFFDF9", surfaceRaised: "#FFFFFF",
            surfaceInset: "#F4F0E8", navigation: "#F5F1E9",
            textPrimary: "#27241F", textSecondary: "#696257",
            stroke: "#E4DED3", strokeStrong: "#C8BFB0", hover: "#F1ECE3",
            disabledSurface: "#F3EFE8", disabledText: "#9B9388"
        }
        if (id === "light_mist") return {
            canvas: "#EEF3F7", surface: "#F9FBFD", surfaceRaised: "#FFFFFF",
            surfaceInset: "#E7EDF3", navigation: "#EAF0F5",
            textPrimary: "#1F2933", textSecondary: "#5E6B78",
            stroke: "#CDD7E1", strokeStrong: "#A9B7C5", hover: "#E4EBF2",
            disabledSurface: "#E8EDF2", disabledText: "#83909C"
        }
        if (id === "light_sage") return {
            canvas: "#F2F6F1", surface: "#FBFDF9", surfaceRaised: "#FFFFFF",
            surfaceInset: "#EAF0E8", navigation: "#EDF3EB",
            textPrimary: "#263028", textSecondary: "#657066",
            stroke: "#D6DED2", strokeStrong: "#B4C1AF", hover: "#E7EEE4",
            disabledSurface: "#EBF0E9", disabledText: "#879187"
        }
        if (id === "dark_midnight") return {
            canvas: "#0F172A", surface: "#162033", surfaceRaised: "#1B2940",
            surfaceInset: "#111B2D", navigation: "#111A2B",
            textPrimary: "#F4F4F4", textSecondary: "#AAB7CA",
            stroke: "#30415C", strokeStrong: "#49617F", hover: "#21304A",
            disabledSurface: "#1B2537", disabledText: "#6F7D91"
        }
        if (id === "dark_slate") return {
            canvas: "#182026", surface: "#222D34", surfaceRaised: "#29363E",
            surfaceInset: "#1C252B", navigation: "#1B252C",
            textPrimary: "#EDF4F7", textSecondary: "#AFBEC5",
            stroke: "#3D4D56", strokeStrong: "#566A75", hover: "#2E3C44",
            disabledSurface: "#28333A", disabledText: "#788891"
        }
        if (id === "dark_umber") return {
            canvas: "#211D1A", surface: "#2C2723", surfaceRaised: "#352F2A",
            surfaceInset: "#27221E", navigation: "#25211E",
            textPrimary: "#F5EFE9", textSecondary: "#C2B6AA",
            stroke: "#4A413A", strokeStrong: "#655A50", hover: "#3A332D",
            disabledSurface: "#332D28", disabledText: "#887D73"
        }
        if (id === "dark_graphite") return {
            canvas: "#1F1F1F", surface: "#2A2A2A", surfaceRaised: "#323232",
            surfaceInset: "#252525", navigation: "#242424",
            textPrimary: "#F4F4F4", textSecondary: "#B8B8B8",
            stroke: "#454545", strokeStrong: "#606060", hover: "#363636",
            disabledSurface: "#303030", disabledText: "#7F7F7F"
        }
        return {
            canvas: "#F6F8FC", surface: "#FFFFFF", surfaceRaised: "#FFFFFF",
            surfaceInset: "#F1F4F9", navigation: "#F3F6FA",
            textPrimary: "#20232B", textSecondary: "#5D6878",
            stroke: "#DDE3EC", strokeStrong: "#BCC6D3", hover: "#EDF2F8",
            disabledSurface: "#F2F4F7", disabledText: "#8B95A3"
        }
    }

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
