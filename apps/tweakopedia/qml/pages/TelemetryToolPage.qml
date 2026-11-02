import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    objectName: "telemetryToolPage"

    property var controller
    property var profile: ({ entries: [], totalCount: 0, configuredCount: 0,
                             changeCount: 0, unsupportedCount: 0, queuedCount: 0 })
    readonly property var entries: profile.entries || []
    signal backRequested()
    signal reviewRequested()
    signal tweakRequested(string id)

    function refreshProfile() {
        profile = controller ? controller.telemetryProfile() : {
            entries: [], totalCount: 0, configuredCount: 0,
            changeCount: 0, unsupportedCount: 0, queuedCount: 0
        }
    }

    function statusText(status) {
        if (status === "configured") return "Уже настроено"
        if (status === "queued") return "В очереди"
        if (status === "unsupported") return "Не поддерживается"
        if (status === "replace") return "Заменит выбор в очереди"
        return "Будет изменено"
    }

    function statusColor(status) {
        if (status === "configured") return "#107C10"
        if (status === "queued") return FluentTheme.accent
        if (status === "unsupported") return FluentTheme.textSecondary
        return "#CA5010"
    }

    Component.onCompleted: refreshProfile()
    onVisibleChanged: if (visible) refreshProfile()

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

    footer: Rectangle {
        implicitHeight: 76
        color: FluentTheme.canvas

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.topMargin: 10
            anchors.bottomMargin: 14
            radius: 9
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 12

                FluentText {
                    Layout.fillWidth: true
                    text: root.profile.changeCount > 0
                        ? "Изменений для добавления: " + root.profile.changeCount
                        : root.profile.queuedCount > 0
                          ? "Параметры профиля уже добавлены в очередь"
                          : "Поддерживаемые параметры уже настроены"
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Button {
                    id: reviewButton
                    objectName: "telemetryReviewButton"
                    visible: root.profile.queuedCount > 0
                    Layout.preferredWidth: 146
                    Layout.preferredHeight: 38
                    text: "Открыть очередь"
                    onClicked: root.reviewRequested()
                }

                Button {
                    id: queueButton
                    objectName: "telemetryQueueButton"
                    enabled: root.profile.changeCount > 0
                    Layout.preferredWidth: 196
                    Layout.preferredHeight: 38
                    text: "Добавить в очередь"
                    onClicked: {
                        if (root.controller)
                            root.controller.queueTelemetryProfile()
                        root.refreshProfile()
                    }
                    contentItem: FluentText {
                        text: queueButton.text
                        color: queueButton.enabled ? "white" : FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: !queueButton.enabled ? FluentTheme.surfaceInset
                            : queueButton.pressed ? FluentTheme.accentPressed
                            : queueButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
                    }
                }
            }
        }
    }

    ScrollView {
        id: pageScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: pageScroll.availableWidth
            spacing: 14

            Item { Layout.preferredHeight: 8 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 12

                AbstractButton {
                    id: backButton
                    objectName: "telemetryBackButton"
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 38
                    Accessible.name: "Вернуться к инструментам"
                    onClicked: root.backRequested()
                    contentItem: Text {
                        text: "\uE72B"
                        color: FluentTheme.textPrimary
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: backButton.pressed ? FluentTheme.selected
                            : backButton.hovered ? FluentTheme.hover : FluentTheme.surface
                        border.width: backButton.activeFocus ? 2 : 1
                        border.color: backButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    FluentText {
                        Layout.fillWidth: true
                        text: "Блокировка телеметрии"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 27
                        font.weight: Font.DemiBold
                    }
                    FluentText {
                        Layout.fillWidth: true
                        text: "Совместимый пакет параметров Windows с предварительным просмотром и возвратом исходных значений."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: introLayout.implicitHeight + 28
                radius: 10
                color: "#F4F8FE"
                border.width: 1
                border.color: "#C9DCF5"

                ColumnLayout {
                    id: introLayout
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 7
                    FluentText {
                        Layout.fillWidth: true
                        text: "Сбалансированный профиль"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    FluentText {
                        Layout.fillWidth: true
                        text: "Ограничивает диагностические данные, облачную конфигурацию, эксперименты, персонализацию и Activity Feed. Не отключает WER, обновления, вход в Microsoft, Windows Hello, Xbox и связанные с ними компоненты."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        lineHeight: 1.15
                        wrapMode: Text.WordWrap
                    }
                    FluentText {
                        Layout.fillWidth: true
                        text: "Сетевые адреса и файл hosts не изменяются. Каждый пункт остаётся отдельным твиком и попадёт в общую очередь."
                        color: FluentTheme.accent
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: width >= 720 ? 3 : 1
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    model: [
                        { title: "Готово или в очереди",
                          value: (root.profile.configuredCount || 0) + (root.profile.queuedCount || 0),
                          color: "#107C10", tint: "#E8F5EA" },
                        { title: "Требуют изменения", value: root.profile.changeCount || 0,
                          color: "#CA5010", tint: "#FFF0E5" },
                        { title: "Не поддерживаются", value: root.profile.unsupportedCount || 0,
                          color: FluentTheme.textSecondary, tint: FluentTheme.surfaceInset }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        objectName: "telemetrySummary_" + index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        radius: 9
                        color: modelData.tint
                        border.width: 1
                        border.color: FluentTheme.stroke
                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 15
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            FluentText {
                                text: String(modelData.value)
                                color: modelData.color
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                            }
                            FluentText {
                                text: modelData.title
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            FluentText {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                text: "Состав пакета"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Repeater {
                id: profileRepeater
                objectName: "telemetryProfileRepeater"
                model: root.entries

                delegate: AbstractButton {
                    id: entryCard
                    required property var modelData
                    objectName: "telemetryEntry_" + modelData.id
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    Layout.preferredHeight: 104
                    Accessible.name: modelData.title + ". Открыть твик"
                    onClicked: root.tweakRequested(modelData.id)

                    contentItem: RowLayout {
                        spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 15
                            spacing: 3
                            FluentText {
                                Layout.fillWidth: true
                                text: entryCard.modelData.title
                                color: FluentTheme.textPrimary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            FluentText {
                                Layout.fillWidth: true
                                text: entryCard.modelData.summary || ""
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 11
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }
                            FluentText {
                                Layout.fillWidth: true
                                text: entryCard.modelData.group + " · Сейчас: "
                                    + (entryCard.modelData.currentTitle || "не определено")
                                    + " · Цель: " + entryCard.modelData.targetTitle
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                        FluentText {
                            text: root.statusText(entryCard.modelData.status)
                            color: root.statusColor(entryCard.modelData.status)
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.rightMargin: 14
                            text: "\uE76C"
                            color: entryCard.hovered ? FluentTheme.accent : FluentTheme.textSecondary
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 12
                        }
                    }
                    background: Rectangle {
                        radius: 8
                        color: entryCard.pressed ? FluentTheme.selected
                            : entryCard.hovered ? FluentTheme.hover : FluentTheme.surface
                        border.width: entryCard.hovered || entryCard.activeFocus ? 2 : 1
                        border.color: entryCard.hovered || entryCard.activeFocus
                            ? FluentTheme.accent : FluentTheme.stroke
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }
}
