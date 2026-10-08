pragma Singleton

import QtQuick

QtObject {
    readonly property color canvas: "#F7F8FC"
    readonly property color surface: "#FFFFFF"
    readonly property color surfaceInset: "#F4F6FA"
    readonly property color textPrimary: "#20232B"
    readonly property color textSecondary: "#687487"
    readonly property color stroke: "#E1E5ED"
    readonly property color accent: "#0067C0"
    readonly property color accentHover: "#005BAA"
    readonly property color hover: "#F2F5FA"
    readonly property color selected: "#DEE9FB"
    readonly property color disabledSurface: "#F5F6F8"
    readonly property color disabledText: "#9299A6"
    readonly property color stateOn: "#107C10"
    readonly property color stateOff: "#C42B1C"
    readonly property color stateUnknown: "#687487"

    readonly property string fontFamily: "Segoe UI"
    readonly property int navigationExpandedWidth: 220
    readonly property int navigationCompactWidth: 64
    readonly property int compactBreakpoint: 1100
}
