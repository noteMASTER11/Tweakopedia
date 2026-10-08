import QtQuick
import QtQuick.Layouts
import QtTest
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "FluentNavigation"
    when: windowShown
    visible: true
    width: 1200
    height: 700

    Component {
        id: navigationComponent
        FluentNavigation {
            height: 680
            availableWidth: 1100
            queueCount: 3
        }
    }

    Component {
        id: compactLayoutComponent
        Item {
            width: 1200
            height: 680

            RowLayout {
                anchors.fill: parent
                spacing: 0

                FluentNavigation {
                    objectName: "compactNavigation"
                    Layout.fillHeight: true
                    availableWidth: parent.parent.width
                }

                Item { Layout.fillWidth: true; Layout.fillHeight: true }
            }
        }
    }

    function test_breakpointAndWidths() {
        const navigation = createTemporaryObject(navigationComponent, this)
        compare(navigation.compact, false)
        compare(navigation.width, FluentTheme.navigationExpandedWidth)

        navigation.availableWidth = 1099
        compare(navigation.compact, true)
        compare(navigation.width, FluentTheme.navigationCompactWidth)

        navigation.availableWidth = 1100
        compare(navigation.compact, false)
        compare(navigation.width, FluentTheme.navigationExpandedWidth)
    }

    function test_compactWidthIsEnforcedInsideLayout() {
        const shell = createTemporaryObject(compactLayoutComponent, this)
        const navigation = findChild(shell, "compactNavigation")
        verify(navigation)
        wait(0)
        compare(navigation.compact, false)
        compare(navigation.width, FluentTheme.navigationExpandedWidth)

        shell.width = 900
        wait(0)
        compare(navigation.compact, true)
        compare(navigation.width, FluentTheme.navigationCompactWidth)
        compare(navigation.Layout.minimumWidth, FluentTheme.navigationCompactWidth)
        compare(navigation.Layout.preferredWidth, FluentTheme.navigationCompactWidth)
        compare(navigation.Layout.maximumWidth, FluentTheme.navigationCompactWidth)
    }

    function test_orderSettingsPlacementBadgeAndAccessibleNames() {
        const navigation = createTemporaryObject(navigationComponent, this, {availableWidth: 1099})
        const expected = ["Обзор", "Твики", "Справочник", "Очередь", "История", "Настройки"]
        for (let index = 0; index < expected.length; ++index) {
            const item = findChild(navigation, "navItem_" + index)
            verify(item)
            compare(item.title, expected[index])
            compare(item.Accessible.name, expected[index])
        }

        const history = findChild(navigation, "navItem_4")
        const settings = findChild(navigation, "navItem_5")
        verify(settings.y > history.y)

        const queueBadge = findChild(navigation, "navBadge_3")
        compare(queueBadge.visible, true)
        compare(queueBadge.text, "3")
    }

    function test_activationChangesCurrentDestination() {
        const navigation = createTemporaryObject(navigationComponent, this)
        const spy = signalSpy.createObject(navigation, {
            target: navigation,
            signalName: "destinationRequested"
        })
        const queue = findChild(navigation, "navItem_3")
        mouseClick(queue)
        compare(navigation.currentIndex, 3)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], 3)
    }

    Component { id: signalSpy; SignalSpy {} }
}
