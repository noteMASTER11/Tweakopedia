import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia about preview"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 5
        }

        AboutPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
