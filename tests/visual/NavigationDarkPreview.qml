import QtQuick
import QtQuick.Controls.Basic
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    width: 420
    height: 760
    visible: true
    color: FluentTheme.canvas

    Component.onCompleted: FluentTheme.configure("dark_graphite", "blue", 1.0, false)

    FluentNavigation {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        availableWidth: 1180
    }
}
