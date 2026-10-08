import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window

    width: 1440
    height: 900
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia encyclopedia preview"

    ListModel {
        id: treeModel
        property int articleCount: count
    }

    QtObject {
        id: articleModel
        property bool hasArticle: true
        property var article: ({
            id: "privacy.diagnostic-data",
            title: "Диагностические данные Windows и управление передачей сведений о работе системы",
            summary: "Параметр определяет объём диагностических данных, которые Windows собирает для анализа стабильности и совместимости.",
            purpose: "Позволяет выбрать, какие системные сведения операционная система может собирать и передавать.",
            breadcrumbs: ["Твикопедия", "Конфиденциальность", "Диагностика и телеметрия"],
            sections: [
                {id: "purpose", title: "Назначение", text: "Настройка помогает понять и ограничить состав диагностических данных Windows.", technical: false},
                {id: "mechanism", title: "Как это работает", text: "Windows сопоставляет значение политики с доступным уровнем диагностических данных и применяет его к системным службам.", technical: false},
                {id: "effect", title: "Что изменится", text: "После применения меняется объём служебных сведений, доступных компонентам диагностики.", technical: false},
                {id: "recommendation", title: "Рекомендация", text: "Перед выбором уровня учитывайте редакцию Windows и требования используемых компонентов.", technical: false},
                {id: "technical", title: "Технические сведения", text: "Политика хранится в системном разделе реестра. При возврате восстанавливается исходное значение.", technical: true}
            ],
            technicalObjects: [
                "HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection\\AllowTelemetry\\ExtremelyLongTechnicalObjectNameForWrapVerification"
            ],
            compatibility: {operatingSystems: ["Windows 10", "Windows 11"], minimumBuild: 19041},
            restart: {id: "service", title: "службы или ПК", required: true},
            relatedArticles: [
                {id: "privacy.telemetry-details", title: "Подробности телеметрии", summary: "Какие компоненты используют диагностические данные."},
                {id: "privacy.tailored-experiences", title: "Персонализированные возможности", summary: "Использование диагностических сведений для рекомендаций."},
                {id: "privacy.feedback-frequency", title: "Частота запросов отзывов", summary: "Управление системными запросами обратной связи."},
                {id: "privacy.activity-history", title: "Журнал действий", summary: "Хранение сведений об активности пользователя."},
                {id: "services.diagtrack", title: "Служба диагностического отслеживания", summary: "Назначение связанной системной службы."},
                {id: "privacy.advertising-id", title: "Рекламный идентификатор", summary: "Идентификатор для персонализации приложений."}
            ]
        })
    }

    QtObject {
        id: encyclopediaController
        property var tree: treeModel
        property var article: articleModel
        property string query: ""
        property bool loading: false
        property bool canGoBack: true
        property bool canGoForward: false
        function setQuery(value) { query = value }
        function clearSearch() { query = "" }
        function openArticle(id) { return true }
        function goBack() { return true }
        function goForward() { return true }
    }

    QtObject {
        id: previewController
        property var encyclopedia: encyclopediaController
    }

    Component.onCompleted: {
        treeModel.append({
            nodeType: "article",
            id: "privacy.diagnostic-data",
            title: "Диагностические данные Windows",
            summary: "Сбор и передача системных сведений",
            path: "Конфиденциальность › Диагностика и телеметрия",
            depth: 0,
            expanded: false,
            selected: true,
            matchScore: 1000
        })
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 2
        }

        TweakopediaPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: previewController
            availableWindowWidth: window.width
        }
    }
}
