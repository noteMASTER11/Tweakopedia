import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "ToolsPage"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    Component {
        id: toolsPageComponent
        ToolsPage { width: 960; height: 660 }
    }

    function test_overviewShowsApprovedToolCards() {
        const page = createTemporaryObject(toolsPageComponent, this)

        compare(findChild(page, "toolsBackground").color, FluentTheme.canvas)
        compare(findChild(page, "toolsTitle").text, "Инструменты")
        compare(findChild(page, "toolsSubtitle").text,
                "Проверка состояния Windows и встроенные средства обслуживания.")

        const cards = findChild(page, "toolCardRepeater")
        compare(cards.count, 5)

        const expected = [
            "Проверка целостности системы",
            "Проверка файловой системы",
            "Диагностика сети",
            "Восстановление сети",
            "Блокировка телеметрии"
        ]
        for (let index = 0; index < expected.length; ++index) {
            const card = findChild(page, "toolCard_" + index)
            verify(card)
            compare(card.Accessible.name, expected[index])
        }
    }

    function test_cardOpensDedicatedPlaceholderAndReturns() {
        const page = createTemporaryObject(toolsPageComponent, this)
        const card = findChild(page, "toolCard_2")
        mouseClick(card)

        compare(findChild(page, "toolsOverview").visible, false)
        compare(findChild(page, "toolPlaceholder").visible, true)
        compare(findChild(page, "toolPlaceholderTitle").text, "Диагностика сети")
        compare(findChild(page, "toolPlaceholderStatus").text, "В разработке")

        mouseClick(findChild(page, "toolPlaceholderBackButton"))
        compare(findChild(page, "toolsOverview").visible, true)
        compare(findChild(page, "toolPlaceholder").visible, false)
    }
}
