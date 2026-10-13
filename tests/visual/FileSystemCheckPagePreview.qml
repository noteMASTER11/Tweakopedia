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
        property var volumes: [
            { target: "C:", title: "C: — Windows", details: "NTFS · 125 ГБ свободно из 500 ГБ · Системный том" },
            { target: "D:", title: "D: — Данные", details: "NTFS · 704 ГБ свободно из 1 000 ГБ" }
        ]
        property int selectedVolumeIndex: 0
        property bool hasVolumes: true
        property string state: "checking"
        property string stageText: "Проверка файловой системы"
        property string outputText: "chkdsk.exe C:\n\nТип файловой системы: NTFS.\nМетка тома: Windows.\n\nЭтап 1. Проверка базовой структуры файловой системы...\n  284160 файловых записей обработано.\nПроверка файлов завершена.\n\nЭтап 2. Проверка связей имен файлов..."
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property int exitCode: -1
        property string sessionLogPath: ""
        property bool busy: true
        property bool canCheck: false
        function setSelectedVolumeIndex(index) { selectedVolumeIndex = index; return true }
        function startCheck() { return true }
        function refreshVolumes() { return true }
        function copyOutput() { return true }
        function openSessionLog() { return true }
    }

    FileSystemCheckPage {
        anchors.fill: parent
        controller: previewController
    }
}
