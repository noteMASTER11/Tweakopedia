import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia Fluent preview"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 1
            queueCount: 1
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 24
                anchors.rightMargin: infoPane.visible ? infoPane.width + 24 : 24
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 12

                Text { text: "Твики"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 28; font.weight: Font.DemiBold }
                Text { text: "Выбранные состояния сначала добавляются в очередь."; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 13 }
                FluentSearchField { Layout.fillWidth: true }

                ListModel {
                    id: categories
                    ListElement { categoryId: "all"; categoryTitle: "Все категории" }
                    ListElement { categoryId: "filesystem"; categoryTitle: "Файловая система" }
                    ListElement { categoryId: "privacy"; categoryTitle: "Конфиденциальность" }
                }

                CategoryRail {
                    Layout.fillWidth: true
                    wide: false
                    currentCategory: "all"
                    model: categories
                }

                TweakRow {
                    Layout.fillWidth: true
                    title: "Поддержка длинных путей Win32"
                    summary: "Разрешает совместимым приложениям работать с путями длиннее исторического ограничения Win32."
                    currentState: "disabled"
                    currentStateTitle: "выключено"
                    availableStates: [{id: "disabled", title: "Выключено"}, {id: "enabled", title: "Включено"}]
                    binary: true
                    supported: true
                }

                TweakRow {
                    Layout.fillWidth: true
                    title: "Очень длинное название параметра для проверки переноса и устойчивости компактной строки"
                    summary: "Строка показывает фактическое и выбранное состояния отдельно."
                    currentState: "disabled"
                    currentStateTitle: "выключено"
                    targetState: "enabled"
                    targetStateTitle: "включено"
                    availableStates: [{id: "disabled", title: "Выключено"}, {id: "enabled", title: "Включено"}]
                    binary: true
                    pending: true
                    supported: true
                }

                TweakRow {
                    Layout.fillWidth: true
                    title: "Режим автоматического обновления Windows"
                    summary: "Определяет способ загрузки и установки найденных обновлений."
                    currentState: "notify_download"
                    currentStateTitle: "Уведомлять перед загрузкой"
                    availableStates: [
                        {id: "notify_download", title: "Уведомлять перед загрузкой"},
                        {id: "auto_download", title: "Загружать и уведомлять"},
                        {id: "scheduled", title: "Загружать и ставить по расписанию"}
                    ]
                    binary: false
                    supported: true
                }

                Item { Layout.fillHeight: true }
                QueueCommandBar { Layout.fillWidth: true; queueCount: 1 }
            }

            InfoPane {
                id: infoPane
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 420
                visible: window.width >= 1180
                opened: visible
                docked: true
                explanation: ({
                    title: "Поддержка длинных путей Win32",
                    purpose: "Снимает историческое ограничение длины пути для совместимых программ.",
                    mechanism: "Windows учитывает системный DWORD и manifest приложения.",
                    effect: "Совместимые программы работают с более глубокими деревьями каталогов.",
                    tradeoffs: "Старые приложения сохраняют собственные ограничения.",
                    recommendation: "Включать для разработки, архивов и длинных имён каталогов.",
                    technicalDetails: "Изменяется значение LongPathsEnabled.",
                    registryObject: "HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem\\LongPathsEnabled",
                    rollback: "Восстанавливается точное исходное значение."
                })
            }
        }
    }
}
