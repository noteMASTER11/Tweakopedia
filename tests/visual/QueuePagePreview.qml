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
    title: "Tweakopedia queue preview"

    ListModel { id: queueModel }

    Component.onCompleted: {
        queueModel.append({
            id: "boot.verbose-logon-messages",
            title: "Подробные сообщения входа и завершения работы",
            currentState: "выключено",
            targetState: "включено"
        })
        queueModel.append({
            id: "boot.lock-screen",
            title: "Экран блокировки",
            currentState: "включено",
            targetState: "выключено"
        })
        queueModel.append({
            id: "boot.login-network-icon",
            title: "Значок сети на экране входа",
            currentState: "включено",
            targetState: "выключено"
        })
    }

    QtObject {
        id: previewController
        property var queue: queueModel
        property string applyStatus: "idle"
        property string applyMessage: ""
        property bool rebootRequired: false
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
            queueCount: queueModel.count
        }

        QueuePage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: previewController
        }
    }
}
