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

    Component {
        id: bodyComponent
        FluentText { text: "Основной текст" }
    }

    function test_fluentTextUsesApplicationFontAndNativeMetrics() {
        const sample = createTemporaryObject(sampleComponent, this)
        verify(sample)
        compare(sample.font.family, Application.font.family)
        compare(sample.font.weight, Font.DemiBold)
        compare(sample.renderType, Text.NativeRendering)
        compare(sample.renderTypeQuality, Text.HighRenderTypeQuality)
        compare(sample.font.hintingPreference, Font.PreferVerticalHinting)
        compare(sample.font.preferTypoLineMetrics, true)
        compare(sample.font.kerning, true)
        compare(sample.antialiasing, true)
        compare(sample.text, "Нормализация Fluent UI · Windows 11 · 0123456789")
    }

    function test_bodyTextDefaultsToMediumWeight() {
        const body = createTemporaryObject(bodyComponent, this)
        verify(body)
        compare(body.font.weight, Font.Medium)
    }
}
