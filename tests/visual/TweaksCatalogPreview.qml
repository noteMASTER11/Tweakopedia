import QtQuick
import QtQuick.Controls.Basic
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window
    property bool hiddenStripState: false
    width: 1484
    height: 999
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia — группы твиков"

    ListModel {
        id: tweaks
        property string categoryId: window.hiddenStripState ? "" : "filesystem"
        Component.onCompleted: {
            append({
                id: "filesystem.win32-long-paths",
                title: "Поддержка длинных путей Win32",
                summary: "Разрешает совместимым приложениям работать с путями длиннее исторического ограничения Win32.",
                currentState: "enabled", currentStateTitle: "включено",
                targetState: "", targetStateTitle: "",
                availableStates: [{id: "disabled", title: "Выключено"}, {id: "enabled", title: "Включено"}],
                binary: true, action: false, pending: false, supported: true,
                supportDetails: "", impact: "low", restart: "none", inputs: []
            })
            append({
                id: "filesystem.ntfs-last-access",
                title: "Обновление времени последнего доступа NTFS",
                summary: "Определяет, записывает ли Windows время последнего обращения к файлам и каталогам.",
                currentState: "system", currentStateTitle: "управляется системой",
                targetState: "disabled", targetStateTitle: "выключено",
                availableStates: [{id: "system", title: "Управляется системой"}, {id: "disabled", title: "Выключено"}],
                binary: false, action: false, pending: true, supported: true,
                supportDetails: "", impact: "medium", restart: "reboot", inputs: []
            })
            append({
                id: "filesystem.trim",
                title: "Уведомления TRIM для накопителей",
                summary: "Управляет передачей командам накопителя сведений об освобождённых блоках.",
                currentState: "enabled", currentStateTitle: "включено",
                targetState: "", targetStateTitle: "",
                availableStates: [{id: "disabled", title: "Выключено"}, {id: "enabled", title: "Включено"}],
                binary: true, action: false, pending: false, supported: true,
                supportDetails: "", impact: "medium", restart: "none", inputs: []
            })
        }
    }

    ListModel {
        id: categories
        Component.onCompleted: {
            append({id: "", title: "Все категории"})
            append({id: "behavior", title: "Поведение Windows"})
            append({id: "boot", title: "Загрузка и вход"})
            append({id: "accounts", title: "Учётные записи и системные полномочия"})
            append({id: "desktop", title: "Рабочий стол и панель задач"})
            append({id: "privacy", title: "Конфиденциальность и облачные функции"})
            append({id: "filesystem", title: "Накопители, NTFS и файловая система"})
            append({id: "network", title: "Сеть и общий доступ"})
        }
    }

    ListModel {
        id: groups
        property string selectedId: ""
        Component.onCompleted: {
            append({id: "", title: "Все", enabled: true})
            append({id: "paths", title: "Пути и имена файлов", enabled: true})
            append({id: "ntfs", title: "NTFS и журналирование", enabled: true})
            append({id: "storage", title: "Накопители и обслуживание", enabled: true})
            append({id: "compression", title: "Сжатие и экономия места", enabled: true})
            append({id: "legacy", title: "Совместимость старых приложений", enabled: true})
        }
    }

    QtObject {
        id: queue
        property int count: 1
    }

    QtObject {
        id: controller
        property var filteredTweaks: tweaks
        property var categories: categories
        property var tweakGroups: groups
        property var queue: queue
        property string appRemovalScanStatus: "succeeded"
        property string appRemovalScanError: ""
        function setTweakSearch(query) {}
        function setTweakCategory(categoryId) { tweaks.categoryId = categoryId }
        function setTweakSubcategory(groupId) { groups.selectedId = groupId }
        function stepTweakSubcategory(delta) {
            let selected = 0
            for (let index = 0; index < groups.count; ++index) {
                if (groups.get(index).id === groups.selectedId) selected = index
            }
            selected = Math.max(0, Math.min(groups.count - 1, selected + delta))
            groups.selectedId = groups.get(selected).id
        }
        function selectTarget(id, state) {}
        function selectParameterizedTarget(id, state, inputs) {}
        function openExplanation(id) { return ({}) }
        function revealTweak(id) { return -1 }
        function scanInstalledApps() {}
    }

    TweaksPage {
        anchors.fill: parent
        controller: controller
    }
}
