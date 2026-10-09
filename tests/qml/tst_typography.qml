import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "Typography"
    when: windowShown
    visible: true
    width: 640
    height: 200

    Component {
        id: sampleComponent
        FluentText {
            text: "Нормализация Fluent UI · Windows 11 · 0123456789"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
    }

    function test_fluentTextUsesApplicationFontAndNativeMetrics() {
        const sample = createTemporaryObject(sampleComponent, this)
        verify(sample)
        compare(sample.font.family, Application.font.family)
        compare(sample.renderType, Text.NativeRendering)
        compare(sample.font.hintingPreference, Font.PreferVerticalHinting)
        compare(sample.font.preferTypoLineMetrics, true)
        compare(sample.text, "Нормализация Fluent UI · Windows 11 · 0123456789")
    }
}
