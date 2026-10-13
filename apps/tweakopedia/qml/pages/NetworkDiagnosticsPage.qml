import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    objectName: "networkDiagnosticsPage"

    property var controller
    readonly property bool narrowLayout: width < 760
    readonly property string toolState: controller ? controller.state : "idle"
    signal backRequested()
    signal recoveryRequested()

    function primaryText() {
        if (controller && controller.busy)
            return "Выполняется…"
        if (toolState === "idle")
            return "Начать диагностику"
        if (toolState === "failed")
            return "Повторить"
        return "Проверить ещё раз"
    }

    function defaultTitle() {
        if (toolState === "awaiting_elevation")
            return "Требуется подтверждение"
        if (controller && controller.busy)
            return "Диагностика выполняется"
        return "Диагностика ещё не запускалась"
    }

    function defaultDescription() {
        if (toolState === "awaiting_elevation")
            return "Подтвердите запуск встроенных средств Windows от имени администратора."
        if (controller && controller.busy)
            return "Программа последовательно собирает сведения об адаптерах, DNS, прокси, hosts, маршрутах и Windows Firewall."
        return "Проверка только считывает сетевую конфигурацию и не изменяет параметры Windows."
    }

    function statusTitle(status) {
        if (status === "ok") return "Замечаний нет"
        if (status === "attention") return "Требует внимания"
        return "Данные недоступны"
    }

    function statusColor(status) {
        if (status === "ok") return "#107C10"
        if (status === "attention") return "#CA5010"
        return FluentTheme.textSecondary
    }

    function statusTint(status) {
        if (status === "ok") return "#E8F5EA"
        if (status === "attention") return "#FFF0E5"
        return FluentTheme.surfaceInset
    }

    function statusGlyph(status) {
        if (status === "ok") return "\uE73E"
        if (status === "attention") return "\uE7BA"
        return "\uE946"
    }

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

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
                    objectName: "networkDiagnosticsBackButton"
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 38
                    focusPolicy: Qt.StrongFocus
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
                        border.width: 1
                        border.color: backButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    FluentText {
                        Layout.fillWidth: true
                        text: "Диагностика сети"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                    FluentText {
                        Layout.fillWidth: true
                        text: "Анализ текущей сетевой конфигурации встроенными средствами Windows."
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
                implicitHeight: stateLayout.implicitHeight + 36
                radius: 10
                color: FluentTheme.surface
                border.width: 1
                border.color: FluentTheme.stroke

                GridLayout {
                    id: stateLayout
                    anchors.fill: parent
                    anchors.margins: 18
                    columns: root.narrowLayout ? 1 : 2
                    columnSpacing: 18
                    rowSpacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        FluentText {
                            Layout.fillWidth: true
                            text: controller && controller.resultTitle.length > 0
                                ? controller.resultTitle : root.defaultTitle()
                            color: toolState === "healthy" ? "#107C10"
                                : toolState === "issues_found" ? "#CA5010"
                                : toolState === "failed" ? "#C42B1C"
                                : FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        FluentText {
                            Layout.fillWidth: true
                            text: controller && controller.resultDescription.length > 0
                                ? controller.resultDescription : root.defaultDescription()
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        FluentText {
                            id: stageText
                            objectName: "networkDiagnosticsStageText"
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: controller ? controller.stageText : ""
                            color: FluentTheme.accent
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        ProgressBar {
                            id: progressBar
                            objectName: "networkDiagnosticsProgressBar"
                            Layout.fillWidth: true
                            visible: controller && controller.busy
                            indeterminate: visible
                        }
                    }

                    Button {
                        id: primaryButton
                        objectName: "networkDiagnosticsPrimaryButton"
                        Layout.preferredWidth: 176
                        Layout.fillWidth: root.narrowLayout
                        Layout.preferredHeight: 40
                        Layout.alignment: root.narrowLayout ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter
                        text: root.primaryText()
                        enabled: controller && controller.canStart
                        onClicked: if (controller) controller.startDiagnostics()

                        contentItem: FluentText {
                            text: primaryButton.text
                            color: primaryButton.enabled ? "white" : FluentTheme.disabledText
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: primaryButton.enabled
                                ? (primaryButton.pressed ? FluentTheme.accentPressed
                                   : primaryButton.hovered ? FluentTheme.accentHover : FluentTheme.accent)
                                : FluentTheme.disabledSurface
                        }
                    }
                }
            }

            GridLayout {
                visible: controller && controller.checks.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: root.narrowLayout ? 1 : 3
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: [
                        { title: "Без замечаний", value: controller ? controller.okCount : 0,
                          color: "#107C10", tint: "#E8F5EA" },
                        { title: "Требуют внимания", value: controller ? controller.attentionCount : 0,
                          color: "#CA5010", tint: "#FFF0E5" },
                        { title: "Недоступны", value: controller ? controller.unavailableCount : 0,
                          color: FluentTheme.textSecondary, tint: FluentTheme.surfaceInset }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 78
                        radius: 9
                        color: modelData.tint
                        border.width: 1
                        border.color: FluentTheme.stroke

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2

                            FluentText {
                                objectName: index === 0 ? "networkDiagnosticsOkCount"
                                    : index === 1 ? "networkDiagnosticsAttentionCount"
                                    : "networkDiagnosticsUnavailableCount"
                                text: String(modelData.value)
                                color: modelData.color
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 22
                                font.weight: Font.DemiBold
                            }
                            FluentText {
                                text: modelData.title
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            GridLayout {
                visible: controller && controller.checks.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: root.narrowLayout ? 1 : 2
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    id: checkRepeater
                    objectName: "networkDiagnosticsCheckRepeater"
                    model: controller ? controller.checks : []

                    delegate: Rectangle {
                        id: checkCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 154
                        radius: 10
                        color: FluentTheme.surface
                        border.width: 1
                        border.color: FluentTheme.stroke

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 38
                                Layout.preferredHeight: 38
                                Layout.alignment: Qt.AlignTop
                                radius: 19
                                color: root.statusTint(checkCard.modelData.status)

                                Text {
                                    anchors.centerIn: parent
                                    text: root.statusGlyph(checkCard.modelData.status)
                                    color: root.statusColor(checkCard.modelData.status)
                                    font.family: "Segoe MDL2 Assets"
                                    font.pixelSize: 16
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FluentText {
                                        Layout.fillWidth: true
                                        text: checkCard.modelData.title || "Проверка"
                                        color: FluentTheme.textPrimary
                                        font.family: FluentTheme.fontFamily
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    FluentText {
                                        text: root.statusTitle(checkCard.modelData.status)
                                        color: root.statusColor(checkCard.modelData.status)
                                        font.family: FluentTheme.fontFamily
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }
                                }

                                FluentText {
                                    Layout.fillWidth: true
                                    text: checkCard.modelData.summary || ""
                                    color: FluentTheme.textSecondary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                FluentText {
                                    Layout.fillWidth: true
                                    visible: checkCard.modelData.details
                                        && checkCard.modelData.details.length > 0
                                    text: visible ? checkCard.modelData.details.slice(0, 2).join(" · ") : ""
                                    color: FluentTheme.textSecondary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: controller && controller.hasAttention
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: recoveryLayout.implicitHeight + 28
                radius: 10
                color: "#FFF8F3"
                border.width: 1
                border.color: "#F0C7A8"

                RowLayout {
                    id: recoveryLayout
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        FluentText {
                            Layout.fillWidth: true
                            text: "Нужны дальнейшие действия?"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: "Перейдите к отдельному инструменту и выберите операции после изучения результатов."
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    Button {
                        id: recoveryButton
                        objectName: "networkDiagnosticsRecoveryButton"
                        Layout.preferredWidth: 196
                        Layout.preferredHeight: 38
                        text: "Перейти к восстановлению"
                        onClicked: root.recoveryRequested()
                        contentItem: FluentText {
                            text: recoveryButton.text
                            color: "white"
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: recoveryButton.pressed ? FluentTheme.accentPressed
                                : recoveryButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: 286
                radius: 10
                color: FluentTheme.surface
                border.width: 1
                border.color: FluentTheme.stroke

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluentText {
                            Layout.fillWidth: true
                            text: "Вывод диагностики"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Button {
                            id: copyButton
                            objectName: "networkDiagnosticsCopyButton"
                            text: "Копировать"
                            enabled: controller && controller.outputText.length > 0
                            onClicked: if (controller) controller.copyOutput()
                            contentItem: FluentText {
                                text: copyButton.text
                                color: copyButton.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 104
                                implicitHeight: 34
                                radius: 6
                                color: copyButton.pressed ? FluentTheme.selected
                                    : copyButton.hovered ? FluentTheme.hover : FluentTheme.surface
                                border.width: 1
                                border.color: copyButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                            }
                        }

                        Button {
                            id: openLogButton
                            objectName: "networkDiagnosticsOpenLogButton"
                            text: "Открыть журнал"
                            enabled: controller && controller.sessionLogPath.length > 0
                            onClicked: if (controller) controller.openSessionLog()
                            contentItem: FluentText {
                                text: openLogButton.text
                                color: openLogButton.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 124
                                implicitHeight: 34
                                radius: 6
                                color: openLogButton.pressed ? FluentTheme.selected
                                    : openLogButton.hovered ? FluentTheme.hover : FluentTheme.surface
                                border.width: 1
                                border.color: openLogButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "#101218"
                        border.width: 1
                        border.color: "#2B3040"

                        Flickable {
                            id: terminalScroll
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            contentWidth: width
                            contentHeight: Math.max(height, terminalText.implicitHeight + 24)
                            boundsBehavior: Flickable.StopAtBounds
                            property bool following: true

                            TextArea {
                                id: terminalText
                                objectName: "networkDiagnosticsTerminal"
                                x: 10
                                y: 8
                                width: terminalScroll.width - 20
                                height: Math.max(terminalScroll.height - 16, implicitHeight)
                                text: controller ? controller.outputText : ""
                                placeholderText: "Здесь появится ход и подробный вывод диагностики."
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere
                                color: "#E6EDF3"
                                placeholderTextColor: "#798294"
                                selectionColor: FluentTheme.accent
                                selectedTextColor: "white"
                                font.family: "Cascadia Mono, Consolas"
                                font.pixelSize: 12
                                background: null

                                onTextChanged: {
                                    if (terminalScroll.following) {
                                        Qt.callLater(function() {
                                            terminalScroll.contentY = Math.max(0,
                                                terminalScroll.contentHeight - terminalScroll.height)
                                        })
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar {}
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 10 }
        }
    }
}
