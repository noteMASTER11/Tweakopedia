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
    title: "Tweakopedia OEM preview"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            Layout.fillHeight: true
            availableWidth: window.width
            currentIndex: 1
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: content.implicitHeight + 48
            clip: true

            ColumnLayout {
                id: content
                width: parent.width - 48
                x: 24
                y: 24
                spacing: 12

                Text {
                    text: "OEM-информация"
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: "Каждое поле изменяется независимо и сначала добавляется в очередь."
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                TweakRow {
                    id: manufacturer
                    Layout.fillWidth: true
                    title: "Производитель компьютера"
                    summary: "Задаёт имя производителя, отображаемое в сведениях о системе и поддержке."
                    currentState: "cleared"
                    currentStateTitle: "Очищено"
                    availableStates: [
                        {id: "cleared", title: "Очистить", inputIds: []},
                        {id: "configured", title: "Задать значение", inputIds: ["manufacturer"]}
                    ]
                    inputs: [{id: "manufacturer", label: "Производитель", type: "text",
                              required: true, minLength: 1, maxLength: 128}]
                    Component.onCompleted: chooseTarget("configured")
                }

                TweakRow {
                    id: supportUrl
                    Layout.fillWidth: true
                    title: "Сайт поддержки OEM"
                    summary: "Задаёт веб-адрес службы поддержки производителя."
                    currentState: "custom"
                    currentStateTitle: "Пользовательское значение"
                    supportDetails: "Текущее значение не совпадает с именованным состоянием."
                    availableStates: [
                        {id: "cleared", title: "Очистить", inputIds: []},
                        {id: "configured", title: "Задать значение", inputIds: ["support_url"]}
                    ]
                    inputs: [{id: "support_url", label: "Адрес поддержки", type: "text",
                              required: true, minLength: 1, maxLength: 2048}]
                    Component.onCompleted: chooseTarget("configured")
                }

                TweakRow {
                    id: logo
                    Layout.fillWidth: true
                    title: "Логотип производителя"
                    summary: "Устанавливает BMP-логотип и связывает его с OEM-сведениями Windows."
                    currentState: "cleared"
                    currentStateTitle: "Удалён"
                    availableStates: [
                        {id: "cleared", title: "Удалить логотип", inputIds: []},
                        {id: "configured", title: "Установить логотип", inputIds: ["logo"]}
                    ]
                    inputs: [{id: "logo", label: "Логотип BMP", type: "file",
                              required: true, extensions: ["bmp"], maxSize: 1048576}]
                    Component.onCompleted: chooseTarget("configured")
                }
            }
        }
    }
}
