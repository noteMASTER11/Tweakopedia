import QtQuick
import QtQuick.Controls.Basic
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    width: 1180
    height: 900
    visible: true
    color: FluentTheme.canvas

    QtObject {
        id: previewController
        property string state: "issues_found"
        property string stageText: ""
        property string outputText: "Проверка сетевых адаптеров завершена.\nПроверка DNS завершена.\nПроверка прокси завершена.\nПроверка файла hosts завершена.\nПроверка таблицы маршрутизации завершена.\nПроверка Windows Firewall завершена."
        property string resultTitle: "Обнаружены параметры, требующие внимания"
        property string resultDescription: "Просмотрите отмеченные блоки и подробный вывод перед изменением настроек."
        property string errorCode: ""
        property string sessionLogPath: "D:/ChatGPT/Temp/Tweakopedia/network-diagnostics.log"
        property var checks: [
            { id: "adapters", title: "Сетевые адаптеры", status: "ok",
              summary: "Активен адаптер Wi-Fi, адрес получен по DHCP.",
              details: ["Wi-Fi · 192.168.1.24", "Шлюз · 192.168.1.1"] },
            { id: "dns", title: "DNS", status: "ok",
              summary: "DNS-серверы настроены и доступны.", details: ["192.168.1.1"] },
            { id: "proxy", title: "Прокси", status: "attention",
              summary: "Для текущего пользователя задан прокси-сервер.",
              details: ["127.0.0.1:8080"] },
            { id: "hosts", title: "Файл hosts", status: "ok",
              summary: "Активных сторонних перенаправлений не найдено.", details: [] },
            { id: "routes", title: "Маршруты", status: "ok",
              summary: "Основной маршрут использует активный Wi-Fi адаптер.", details: [] },
            { id: "firewall", title: "Windows Firewall", status: "attention",
              summary: "Найдены разрешающие входящие правила сторонних программ.",
              details: ["Local TCP 8080", "Remote Assistance"],
              items: [{ key: "{8C3D87D8-DEMO}", title: "Example Web Server",
                        description: "Разрешает входящие подключения тестового сервера.",
                        profile: "Private", program: "C:/Program Files/Example/server.exe",
                        service: "Any", protocol: "TCP", local_port: "8080",
                        remote_port: "Any", local_address: "Any", remote_address: "Any" }] }
        ]
        property int okCount: 4
        property int attentionCount: 2
        property int unavailableCount: 0
        property bool hasAttention: true
        property bool busy: false
        property bool canStart: true
        property string actionState: "idle"
        property string actionMessage: ""
        property string activeRuleKey: ""
        property var removedRuleKeys: []
        function startDiagnostics() { return true }
        function copyOutput() { return true }
        function openSessionLog() { return true }
        function removeFirewallRule(key) { return true }
    }

    NetworkDiagnosticsPage {
        anchors.fill: parent
        controller: previewController
        Component.onCompleted: openCheckDetails(previewController.checks[5])
    }
}
