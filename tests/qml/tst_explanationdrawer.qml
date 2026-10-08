import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "ExplanationDrawer"
    when: windowShown

    ApplicationWindow {
        id: host
        width: 900
        height: 700
        visible: true

        ExplanationDrawer {
            id: drawer
            explanation: ({
                title: "Длинные пути",
                purpose: "Назначение полностью",
                mechanism: "Механизм полностью",
                effect: "Эффект полностью",
                tradeoffs: "Ограничения полностью",
                recommendation: "Рекомендация полностью",
                technicalDetails: "Технические детали полностью",
                registryObject: "HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled",
                rollback: "Возврат точного исходного значения"
            })
        }
    }

    function test_opensAndShowsAllSections() {
        drawer.open()
        tryVerify(() => drawer.opened)
        compare(findChild(drawer, "purposeText").text, "Назначение полностью")
        compare(findChild(drawer, "mechanismText").text, "Механизм полностью")
        compare(findChild(drawer, "effectText").text, "Эффект полностью")
        compare(findChild(drawer, "tradeoffsText").text, "Ограничения полностью")
        compare(findChild(drawer, "recommendationText").text, "Рекомендация полностью")
        compare(findChild(drawer, "technicalDetailsText").text, "Технические детали полностью")
        verify(findChild(drawer, "registryObjectText").text.includes("LongPathsEnabled"))
        verify(findChild(drawer, "rollbackText").text.includes("исходного значения"))
        drawer.close()
    }
}
