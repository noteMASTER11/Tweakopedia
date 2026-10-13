import QtQuick
import QtQuick.Controls.Basic
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    width: 1180
    height: 760
    visible: true
    color: FluentTheme.canvas

    QtObject {
        id: previewController
        property string state: "scanning"
        property string stageText: "Проверка хранилища компонентов"
        property int progress: 47
        property bool progressKnown: true
        property string outputText: "DISM.exe /Online /Cleanup-Image /ScanHealth /English\n\nDeployment Image Servicing and Management tool\nVersion: 10.0.26200.1\n\n[==================47.0%=================          ]"
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property int exitCode: -1
        property string sessionLogPath: ""
        property string dismLogPath: "C:/Windows/Logs/DISM/dism.log"
        property bool busy: true
        property bool canScan: false
        property bool canRestore: false
        function startScan() { return true }
        function startRestore() { return true }
        function copyOutput() { return true }
        function openDismLog() { return true }
    }

    SystemIntegrityPage {
        anchors.fill: parent
        controller: previewController
    }
}
