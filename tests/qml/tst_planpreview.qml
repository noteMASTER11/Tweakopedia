import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "PlanPreview"
    when: windowShown
    width: 900
    height: 600

    Component {
        id: previewComponent
        PlanPreview {
            width: 820
            packageName: "Мой пакет"
            summary: "Будет применено операций: 1."
            operations: [{
                title: "Поддержка длинных путей Win32",
                beforeState: "disabled",
                targetState: "enabled",
                registryObject: "HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled",
                restart: "none"
            }]
        }
    }

    function test_previewRequiredBeforeApply() {
        const preview = createTemporaryObject(previewComponent, this)
        const apply = findChild(preview, "applyButton")
        verify(!apply.enabled)
        preview.previewReady = true
        verify(apply.enabled)
        compare(findChild(preview, "packageNameField").text, "Мой пакет")
        compare(findChild(preview, "beforeStateText").text, "disabled")
        compare(findChild(preview, "targetStateText").text, "enabled")
        verify(findChild(preview, "registryObjectText").text.includes("LongPathsEnabled"))
        compare(findChild(preview, "restartText").text, "none")
    }

    function test_confirmationSignal() {
        const preview = createTemporaryObject(previewComponent, this)
        preview.previewReady = true
        const spy = signalSpy.createObject(preview, {target: preview, signalName: "applyConfirmed"})
        findChild(preview, "applyButton").clicked()
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "Мой пакет")
    }

    Component { id: signalSpy; SignalSpy {} }
}
