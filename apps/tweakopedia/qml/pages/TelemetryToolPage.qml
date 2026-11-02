import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    objectName: "telemetryToolPage"

    property var controller
    property string selectedProfileId: "balanced"
    property var profileDefinitions: [
        {
            id: "basic",
            title: "Базовый",
            subtitle: "Убрать персонализацию",
            audience: "Для тех, кому нужны все обычные функции Windows, но не нужны персональные предложения и рекламный идентификатор.",
            effect: "Windows оставит обязательную диагностику, но перестанет добавлять имя компьютера, предлагать отправить отзыв и использовать диагностические данные для персональных советов.",
            remains: "Вход в Microsoft, Hello, Xbox, обновления, отчёты о сбоях и синхронизация между устройствами продолжат работать.",
            glyph: "\uE77B"
        },
        {
            id: "balanced",
            title: "Оптимальный",
            subtitle: "Сократить фоновый обмен",
            audience: "Рекомендуемый вариант для обычного домашнего компьютера без межустройственной истории и экспериментальных настроек.",
            effect: "Помимо базовых ограничений Windows прекратит загружать экспериментальные настройки, отправлять историю действий между устройствами и собирать образцы ввода для персонализации.",
            remains: "Вход в Microsoft, Hello, Xbox, обновления, Edge и штатные отчёты о сбоях продолжат работать.",
            glyph: "\uE73E"
        },
        {
            id: "strict",
            title: "Строгий",
            subtitle: "Минимум фоновой отправки",
            audience: "Для тех, кто готов отказаться от автоматических отчётов о сбоях и части встроенной диагностики проблем.",
            effect: "Windows и Edge получат минимально доступный уровень отправки. Отчёты о сбоях и сбор сведений о влиянии приложений будут отключены; разбор редких ошибок станет менее информативным.",
            remains: "Вход в Microsoft, Hello, Xbox и обновления не отключаются. В обычных редакциях Windows часть обязательных данных всё равно может сохраняться.",
            glyph: "\uE72E"
        }
    ]
    property var profile: ({ entries: [], totalCount: 0, configuredCount: 0,
                             changeCount: 0, unsupportedCount: 0, queuedCount: 0 })
    readonly property var entries: profile.entries || []
    signal backRequested()
    signal reviewRequested()
    signal tweakRequested(string id)

    function refreshProfile() {
        profile = controller ? controller.telemetryProfile(selectedProfileId) : {
            entries: [], totalCount: 0, configuredCount: 0,
            changeCount: 0, unsupportedCount: 0, queuedCount: 0
        }
    }

    function selectedDefinition() {
        for (let index = 0; index < profileDefinitions.length; ++index) {
            if (profileDefinitions[index].id === selectedProfileId)
                return profileDefinitions[index]
        }
        return profileDefinitions[1]
    }

    function statusText(status) {
        if (status === "configured") return "Уже настроено"
        if (status === "queued") return "В очереди"
        if (status === "unsupported") return "Не поддерживается"
        if (status === "replace") return "Заменит выбор в очереди"
        return "Будет изменено"
    }

    function statusColor(status) {
        if (status === "configured") return FluentTheme.positive
        if (status === "queued") return FluentTheme.accent
        if (status === "unsupported") return FluentTheme.textSecondary
        return FluentTheme.warning
    }

    Component.onCompleted: refreshProfile()
    onVisibleChanged: if (visible) refreshProfile()

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

    header: ToolPageHeader {
        objectName: "telemetryToolHeader"
        title: "Блокировка телеметрии"
        subtitle: "Совместимый пакет параметров Windows с предварительным просмотром и возвратом исходных значений."
        backButtonObjectName: "telemetryBackButton"
        onBackRequested: root.backRequested()
    }

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
                            root.controller.queueTelemetryProfile(root.selectedProfileId)
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

            FluentText {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                text: "Выберите, насколько сильно ограничить фоновую отправку данных"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 16
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: width >= 820 ? 3 : 1
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    id: profileSelectorRepeater
                    objectName: "telemetryProfileSelectorRepeater"
                    model: root.profileDefinitions

                    delegate: AbstractButton {
                        id: profileCard
                        required property var modelData
                        objectName: "telemetryProfileChoice_" + modelData.id
                        Layout.fillWidth: true
                        Layout.preferredHeight: 126
                        Accessible.name: modelData.title + ". " + modelData.subtitle
                        onClicked: {
                            root.selectedProfileId = modelData.id
                            root.refreshProfile()
                        }

                        contentItem: ColumnLayout {
                            spacing: 7
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 15
                                Layout.rightMargin: 15
                                Layout.topMargin: 12
                                spacing: 9
                                Rectangle {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: 16
                                    color: root.selectedProfileId === profileCard.modelData.id
                                        ? FluentTheme.selected : FluentTheme.surfaceInset
                                    Text {
                                        anchors.centerIn: parent
                                        text: profileCard.modelData.glyph
                                        color: FluentTheme.accent
                                        font.family: "Segoe MDL2 Assets"
                                        font.pixelSize: 15
                                    }
                                }
                                FluentText {
                                    Layout.fillWidth: true
                                    text: profileCard.modelData.title
                                    color: FluentTheme.textPrimary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }
                                Rectangle {
                                    visible: profileCard.modelData.id === "balanced"
                                    Layout.preferredWidth: recommendedText.implicitWidth + 14
                                    Layout.preferredHeight: 24
                                    radius: 12
                                    color: FluentTheme.selected
                                    FluentText {
                                        id: recommendedText
                                        anchors.centerIn: parent
                                        text: "Рекомендуется"
                                        color: FluentTheme.accent
                                        font.family: FluentTheme.fontFamily
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                            FluentText {
                                Layout.fillWidth: true
                                Layout.leftMargin: 15
                                Layout.rightMargin: 15
                                text: profileCard.modelData.subtitle
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            Item { Layout.fillHeight: true }
                        }
                        background: Rectangle {
                            radius: 9
                            color: root.selectedProfileId === profileCard.modelData.id
                                ? FluentTheme.infoSurface
                                : profileCard.pressed ? FluentTheme.selected
                                : profileCard.hovered ? FluentTheme.hover : FluentTheme.surface
                            border.width: root.selectedProfileId === profileCard.modelData.id
                                || profileCard.hovered || profileCard.activeFocus ? 2 : 1
                            border.color: root.selectedProfileId === profileCard.modelData.id
                                || profileCard.hovered || profileCard.activeFocus
                                ? FluentTheme.accent : FluentTheme.stroke
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: explanationLayout.implicitHeight + 28
                radius: 10
                color: FluentTheme.infoSurface
                border.width: 1
                border.color: FluentTheme.infoStroke

                ColumnLayout {
                    id: explanationLayout
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9

                    Repeater {
                        model: [
                            { glyph: "\uE8D7", title: "Кому подходит",
                              text: root.selectedDefinition().audience },
                            { glyph: "\uE7BA", title: "Что изменится",
                              text: root.selectedDefinition().effect },
                            { glyph: "\uE73E", title: "Что останется работать",
                              text: root.selectedDefinition().remains }
                        ]
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                Layout.alignment: Qt.AlignTop
                                radius: 14
                                color: FluentTheme.selected
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.glyph
                                    color: FluentTheme.accent
                                    font.family: "Segoe MDL2 Assets"
                                    font.pixelSize: 13
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                FluentText {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: FluentTheme.textPrimary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                }
                                FluentText {
                                    Layout.fillWidth: true
                                    text: modelData.text
                                    color: FluentTheme.textSecondary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 12
                                    lineHeight: 1.12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
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
                          color: FluentTheme.positive, tint: FluentTheme.positiveSurface },
                        { title: "Требуют изменения", value: root.profile.changeCount || 0,
                          color: FluentTheme.warning, tint: FluentTheme.warningSurface },
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
