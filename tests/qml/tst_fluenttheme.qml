import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "FluentTheme"

    function init() {
        FluentTheme.configure("light_cool", "blue", 1.0, false)
    }

    function test_semanticColors() {
        compare(FluentTheme.canvas.toString(), "#f6f8fc")
        compare(FluentTheme.surface.toString(), "#ffffff")
        compare(FluentTheme.surfaceInset.toString(), "#f1f4f9")
        compare(FluentTheme.textPrimary.toString(), "#20232b")
        compare(FluentTheme.textSecondary.toString(), "#5d6878")
        compare(FluentTheme.stroke.toString(), "#dde3ec")
        compare(FluentTheme.accent.toString(), "#0067c0")
        compare(FluentTheme.hover.toString(), "#edf2f8")
        compare(FluentTheme.selected.toString(), "#deeaf8")
        compare(FluentTheme.disabledSurface.toString(), "#f2f4f7")
        compare(FluentTheme.disabledText.toString(), "#8b95a3")
        compare(FluentTheme.stateOn.toString(), "#107c10")
        compare(FluentTheme.stateOff.toString(), "#c42b1c")
        compare(FluentTheme.stateUnknown.toString(), "#687487")
    }

    function test_typographyAndAdaptiveGeometry() {
        compare(FluentTheme.fontFamily, Application.font.family)
        compare(FluentTheme.bodyFontWeight, Font.Medium)
        compare(FluentTheme.headingFontWeight, Font.DemiBold)
        compare(FluentTheme.navigationExpandedWidth, 220)
        compare(FluentTheme.navigationCompactWidth, 64)
        compare(FluentTheme.compactBreakpoint, 1100)
    }

    function test_switchesCompleteDarkPalettes() {
        FluentTheme.configure("dark_graphite", "violet", 1.25, true)
        compare(FluentTheme.canvas.toString(), "#1f1f1f")
        compare(FluentTheme.surface.toString(), "#2a2a2a")
        compare(FluentTheme.textPrimary.toString(), "#f4f4f4")
        compare(FluentTheme.accent.toString(), "#a78bfa")
        compare(FluentTheme.uiScale, 1.25)
        compare(FluentTheme.animationNormal, 0)

        FluentTheme.configure("dark_midnight", "teal", 1.0, false)
        compare(FluentTheme.canvas.toString(), "#0f172a")
        compare(FluentTheme.surface.toString(), "#162033")
        compare(FluentTheme.accent.toString(), "#4dd4c6")
    }
}
