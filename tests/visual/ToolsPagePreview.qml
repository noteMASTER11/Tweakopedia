import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window

    width: 1484
    height: 999
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia tools preview"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 5
        }

        ToolsPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
