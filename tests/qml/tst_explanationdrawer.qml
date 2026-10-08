import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "InfoPane"
    when: windowShown
    visible: true
    width: 900
    height: 700

    Component {
        id: paneComponent
        InfoPane {
            width: 420
            height: 680
            docked: true
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
            opened: true
        }
    }

    function test_showsAllSectionsAndCollapsibleTechnicalDetails() {
        const pane = createTemporaryObject(paneComponent, this)
        compare(findChild(pane, "purposeText").text, "Назначение полностью")
        compare(findChild(pane, "mechanismText").text, "Механизм полностью")
        compare(findChild(pane, "effectText").text, "Эффект полностью")
        compare(findChild(pane, "tradeoffsText").text, "Ограничения полностью")
        compare(findChild(pane, "recommendationText").text, "Рекомендация полностью")
        compare(findChild(pane, "technicalDetailsText").visible, false)
        findChild(pane, "technicalToggle").clicked()
        compare(findChild(pane, "technicalDetailsText").visible, true)
        verify(findChild(pane, "registryObjectText").text.includes("LongPathsEnabled"))
        verify(findChild(pane, "rollbackText").text.includes("исходного значения"))
    }
}
