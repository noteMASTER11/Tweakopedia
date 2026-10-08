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
    title: "Tweakopedia success preview"

    ListModel { id: emptyQueue }

    QtObject {
        id: previewController
        property var queue: emptyQueue
        property string applyStatus: "succeeded"
        property string applyMessage: "Пакет применён и проверен."
        property bool rebootRequired: true
        function applyQueue(name) {}
        function removeFromQueue(id) {}
        function restartComputer() {}
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 3
            queueCount: 0
        }

        QueuePage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: previewController
        }
    }
}
