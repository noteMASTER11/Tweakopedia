import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "FluentTheme"

    function test_semanticColors() {
        compare(FluentTheme.canvas.toString(), "#f7f8fc")
        compare(FluentTheme.surface.toString(), "#ffffff")
        compare(FluentTheme.surfaceInset.toString(), "#f4f6fa")
        compare(FluentTheme.textPrimary.toString(), "#20232b")
        compare(FluentTheme.textSecondary.toString(), "#687487")
        compare(FluentTheme.stroke.toString(), "#e1e5ed")
        compare(FluentTheme.accent.toString(), "#0067c0")
        compare(FluentTheme.accentHover.toString(), "#005baa")
        compare(FluentTheme.hover.toString(), "#f2f5fa")
        compare(FluentTheme.selected.toString(), "#dee9fb")
        compare(FluentTheme.disabledSurface.toString(), "#f5f6f8")
        compare(FluentTheme.disabledText.toString(), "#9299a6")
        compare(FluentTheme.stateOn.toString(), "#107c10")
        compare(FluentTheme.stateOff.toString(), "#c42b1c")
        compare(FluentTheme.stateUnknown.toString(), "#687487")
    }

    function test_typographyAndAdaptiveGeometry() {
        compare(FluentTheme.fontFamily, "Segoe UI")
        compare(FluentTheme.navigationExpandedWidth, 220)
        compare(FluentTheme.navigationCompactWidth, 64)
        compare(FluentTheme.compactBreakpoint, 1100)
    }
}
