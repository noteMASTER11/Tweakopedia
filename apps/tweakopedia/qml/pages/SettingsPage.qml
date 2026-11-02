import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    padding: 0

    background: Rectangle {
        objectName: "settingsBackground"
        color: FluentTheme.canvas
    }

    component SettingCard: Rectangle {
        Layout.fillWidth: true
        radius: 10
        color: FluentTheme.surface
        border.width: 1
        border.color: FluentTheme.stroke
    }

    component FluentCombo: ComboBox {
        id: combo
        implicitHeight: 38
        leftPadding: 12
        rightPadding: 34
        font.family: FluentTheme.fontFamily
        font.pixelSize: 13

        contentItem: FluentText {
            text: combo.displayText
            color: FluentTheme.textPrimary
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Text {
            x: combo.width - width - 12
            anchors.verticalCenter: parent.verticalCenter
            text: "\uE70D"
            color: FluentTheme.textSecondary
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 11
        }
        background: Rectangle {
            radius: 6
            color: combo.pressed ? FluentTheme.selected : FluentTheme.surfaceInset
            border.width: 1
            border.color: combo.activeFocus ? FluentTheme.accent : FluentTheme.stroke
        }
        popup: Popup {
            y: combo.height + 4
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
            padding: 4
            background: Rectangle {
                radius: 8
                color: FluentTheme.surfaceRaised
                border.width: 1
                border.color: FluentTheme.stroke
            }
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
            }
        }
        delegate: ItemDelegate {
            required property var modelData
            required property int index
            width: combo.width - 8
            height: 36
            contentItem: FluentText {
                text: modelData
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 5
                color: index === combo.highlightedIndex ? FluentTheme.selected : "transparent"
            }
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.max(0, scroll.availableWidth - 56)
            x: 28
            spacing: 14

            Item { Layout.preferredHeight: 10 }

            FluentText {
                objectName: "settingsTitle"
                Layout.fillWidth: true
                text: "Настройки"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            FluentText {
                Layout.fillWidth: true
                text: "Оформление, масштаб интерфейса и средства диагностики. Кроме масштаба, изменения применяются сразу."
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            FluentText {
                Layout.topMargin: 8
                text: "Внешний вид"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            SettingCard {
                implicitHeight: 92

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        FluentText {
                            text: "Режим"
                            color: FluentTheme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                        FluentText {
                            text: "Следовать Windows или постоянно использовать выбранную схему."
                            color: FluentTheme.textSecondary
                            font.pixelSize: 12
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Repeater {
                            model: [
                                { id: "system", label: "Системный", icon: "\uE713" },
                                { id: "light", label: "Светлый", icon: "\uE706" },
                                { id: "dark", label: "Тёмный", icon: "\uE708" }
                            ]
                            delegate: Button {
                                required property var modelData
                                objectName: "appearanceMode_" + modelData.id
                                Layout.preferredWidth: 104
                                Layout.preferredHeight: 44
                                onClicked: root.controller.setAppearanceMode(modelData.id)
                                contentItem: RowLayout {
                                    spacing: 7
                                    Text {
                                        text: modelData.icon
                                        color: root.controller.appearanceMode === modelData.id
                                            ? FluentTheme.accent : FluentTheme.textSecondary
                                        font.family: "Segoe MDL2 Assets"
                                        font.pixelSize: 15
                                    }
                                    FluentText {
                                        text: modelData.label
                                        color: FluentTheme.textPrimary
                                        font.pixelSize: 12
                                    }
                                }
                                background: Rectangle {
                                    radius: 7
                                    color: root.controller.appearanceMode === modelData.id
                                        ? FluentTheme.selected : parent.hovered ? FluentTheme.hover : FluentTheme.surfaceInset
                                    border.width: root.controller.appearanceMode === modelData.id ? 2 : 1
                                    border.color: root.controller.appearanceMode === modelData.id
                                        ? FluentTheme.accent : FluentTheme.stroke
                                }
                            }
                        }
                    }
                }
            }

            Row {
                id: themePresetRow
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: [
                        { id: "light_cool", title: "Холодный свет", kind: "light", canvas: "#F6F8FC", surface: "#FFFFFF", text: "#20232B" },
                        { id: "light_warm", title: "Тёплая бумага", kind: "light", canvas: "#FAF8F3", surface: "#FFFDF9", text: "#27241F" },
                        { id: "dark_graphite", title: "Графит", kind: "dark", canvas: "#1F1F1F", surface: "#2A2A2A", text: "#F4F4F4" },
                        { id: "dark_midnight", title: "Полночь", kind: "dark", canvas: "#0F172A", surface: "#162033", text: "#E8EEF8" }
                    ]
                    delegate: Button {
                        id: themeButton
                        required property var modelData
                        readonly property bool selected: root.controller.effectiveThemeId === modelData.id
                        objectName: "theme_" + modelData.id
                        width: Math.max(0, (themePresetRow.width - themePresetRow.spacing * 3) / 4)
                        height: 136
                        onClicked: {
                            if (modelData.kind === "light")
                                root.controller.setLightThemeId(modelData.id)
                            else
                                root.controller.setDarkThemeId(modelData.id)
                            if (root.controller.appearanceMode !== "system")
                                root.controller.setAppearanceMode(modelData.kind)
                        }
                        contentItem: ColumnLayout {
                            spacing: 8
                            clip: true
                            Rectangle {
                                objectName: "themePreview_" + modelData.id
                                Layout.fillWidth: true
                                Layout.preferredHeight: 78
                                radius: 6
                                clip: true
                                color: modelData.canvas
                                border.width: 1
                                border.color: modelData.kind === "dark" ? "#46566D" : "#D8DEE8"
                                Rectangle {
                                    x: 8; y: 8; width: 34; height: parent.height - 16
                                    radius: 4; color: modelData.surface
                                }
                                Column {
                                    x: 52; y: 14; spacing: 7
                                    Rectangle { width: Math.max(0, Math.min(72, parent.parent.width - 64)); height: 7; radius: 3; color: modelData.text }
                                    Rectangle { width: Math.max(0, Math.min(106, parent.parent.width - 64)); height: 5; radius: 2; color: modelData.text; opacity: 0.45 }
                                    Rectangle { width: Math.max(0, Math.min(92, parent.parent.width - 64)); height: 5; radius: 2; color: FluentTheme.accent }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                FluentText {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: FluentTheme.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: themeButton.selected
                                    text: "\uE73E"
                                    color: FluentTheme.accent
                                    font.family: "Segoe MDL2 Assets"
                                    font.pixelSize: 14
                                }
                            }
                        }
                        background: Rectangle {
                            radius: 10
                            color: themeButton.hovered ? FluentTheme.hover : FluentTheme.surface
                            border.width: themeButton.selected ? 2 : 1
                            border.color: themeButton.selected ? FluentTheme.accent : FluentTheme.stroke
                        }
                    }
                }
            }

            SettingCard {
                implicitHeight: 80
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    FluentText {
                        Layout.fillWidth: true
                        text: "Акцентный цвет"
                        color: FluentTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    RowLayout {
                        spacing: 12
                        Repeater {
                            model: [
                                { id: "blue", color: "#0067C0", label: "Синий" },
                                { id: "violet", color: "#7657D5", label: "Фиолетовый" },
                                { id: "teal", color: "#008C82", label: "Бирюзовый" },
                                { id: "red", color: "#C42B1C", label: "Красный" },
                                { id: "amber", color: "#B86400", label: "Янтарный" }
                            ]
                            delegate: Button {
                                id: accentButton
                                required property var modelData
                                objectName: "accent_" + modelData.id
                                Accessible.name: modelData.label
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                onClicked: root.controller.setAccentId(modelData.id)
                                contentItem: Text {
                                    text: root.controller.accentId === modelData.id ? "\uE73E" : ""
                                    color: "white"
                                    font.family: "Segoe MDL2 Assets"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: width / 2
                                    color: modelData.color
                                    border.width: root.controller.accentId === modelData.id ? 3 : 1
                                    border.color: root.controller.accentId === modelData.id
                                        ? FluentTheme.textPrimary : FluentTheme.stroke
                                }
                            }
                        }
                    }
                }
            }

            SettingCard {
                implicitHeight: 174
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            FluentText { text: "Шрифт"; color: FluentTheme.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                            FluentText { text: "Segoe UI оптимизирован для рендеринга Qt; системный использует настройку Windows."; color: FluentTheme.textSecondary; font.pixelSize: 12 }
                        }
                        FluentCombo {
                            id: fontCombo
                            objectName: "fontCombo"
                            Layout.preferredWidth: 190
                            model: ["Segoe UI (рекомендуется)", "Системный"]
                            currentIndex: root.controller.fontId === "system" ? 1 : 0
                            onActivated: root.controller.setFontId(currentIndex === 1 ? "system" : "segoe")
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: FluentTheme.stroke }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            FluentText { text: "Масштаб интерфейса"; color: FluentTheme.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                            FluentText {
                                text: root.controller.uiScaleRestartRequired
                                    ? "Новый масштаб будет применён после перезапуска Tweakopedia."
                                    : "Изменяет размер интерфейса независимо от масштаба Windows."
                                color: root.controller.uiScaleRestartRequired
                                    ? FluentTheme.warning : FluentTheme.textSecondary
                                font.pixelSize: 12
                            }
                        }
                        FluentCombo {
                            id: scaleCombo
                            objectName: "scaleCombo"
                            Layout.preferredWidth: 120
                            model: ["90%", "100%", "110%", "125%"]
                            currentIndex: root.controller.uiScale < 0.95 ? 0
                                : root.controller.uiScale < 1.05 ? 1
                                : root.controller.uiScale < 1.2 ? 2 : 3
                            onActivated: root.controller.setUiScale([0.9, 1.0, 1.1, 1.25][currentIndex])
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: FluentTheme.stroke }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            FluentText { text: "Уменьшение движения"; color: FluentTheme.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                            FluentText { text: "Отключает декоративные переходы и следует настройке Windows в системном режиме."; color: FluentTheme.textSecondary; font.pixelSize: 12 }
                        }
                        RowLayout {
                            spacing: 4
                            Repeater {
                                model: [{ id: "system", label: "Системный" }, { id: "on", label: "Вкл." }, { id: "off", label: "Выкл." }]
                                delegate: Button {
                                    required property var modelData
                                    objectName: "motion_" + modelData.id
                                    Layout.preferredWidth: modelData.id === "system" ? 88 : 54
                                    Layout.preferredHeight: 34
                                    onClicked: root.controller.setMotionMode(modelData.id)
                                    contentItem: FluentText {
                                        text: modelData.label
                                        color: FluentTheme.textPrimary
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: root.controller.motionMode === modelData.id
                                            ? FluentTheme.selected : parent.hovered ? FluentTheme.hover : FluentTheme.surfaceInset
                                        border.width: root.controller.motionMode === modelData.id ? 2 : 1
                                        border.color: root.controller.motionMode === modelData.id
                                            ? FluentTheme.accent : FluentTheme.stroke
                                    }
                                }
                            }
                        }
                    }
                }
            }

            FluentText {
                Layout.topMargin: 8
                text: "Диагностика"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            SettingCard {
                implicitHeight: 106

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 16
                    anchors.bottomMargin: 16
                    spacing: 18

                    Text {
                        text: "\uE9D9"
                        color: FluentTheme.accent
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 22
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        FluentText {
                            objectName: "debugLoggingTitle"
                            text: "Включить режим отладки"
                            color: FluentTheme.textPrimary
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: "Записывает подробные сообщения уровня DEBUG и выше в отдельный файл текущего сеанса."
                            color: FluentTheme.textSecondary
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                    Button {
                        id: openLogsButton
                        objectName: "openLogsButton"
                        Layout.preferredWidth: 126
                        Layout.preferredHeight: 34
                        text: "Открыть логи"
                        onClicked: root.controller.openLogsDirectory()
                        contentItem: FluentText {
                            text: openLogsButton.text
                            color: FluentTheme.textPrimary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: openLogsButton.pressed ? FluentTheme.selected
                                : openLogsButton.hovered ? FluentTheme.hover : FluentTheme.surface
                            border.width: 1
                            border.color: openLogsButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                        }
                    }
                    FluentToggle {
                        id: debugLoggingToggle
                        objectName: "debugLoggingToggle"
                        checked: root.controller.debugLoggingEnabled
                        Accessible.name: "Включить режим отладки"
                        onToggledByUser: function(enabled) {
                            root.controller.setDebugLoggingEnabled(enabled)
                        }
                    }
                }
            }

            FluentText {
                Layout.fillWidth: true
                text: "Файлы сохраняются в локальном каталоге данных Tweakopedia. Имя содержит дату и время запуска сеанса."
                color: FluentTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
