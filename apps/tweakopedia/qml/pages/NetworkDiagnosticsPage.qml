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
    readonly property bool detailDocked: width >= 1180
    readonly property string toolState: controller ? controller.state : "idle"
    property var selectedCheck: ({})
    property var pendingRule: ({})
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

    function openCheckDetails(check) {
        selectedCheck = check
        detailPane.open()
    }

    function ruleIsRemoved(key) {
        return controller && controller.removedRuleKeys
            && controller.removedRuleKeys.indexOf(key) >= 0
    }

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

    ScrollView {
        id: pageScroll
        objectName: "networkDiagnosticsPageScroll"
        anchors.fill: parent
        anchors.rightMargin: detailPane.opened && root.detailDocked ? detailPane.width : 0
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            objectName: "networkDiagnosticsContent"
            width: pageScroll.width
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
                    columns: root.narrowLayout || pageScroll.width < 900 ? 1 : 2
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
                        Layout.minimumWidth: 0
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

            ColumnLayout {
                id: checkList
                objectName: "networkDiagnosticsCheckList"
                visible: controller && controller.checks.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 8

                FluentText {
                    Layout.fillWidth: true
                    text: "Результаты проверки"
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Repeater {
                    id: checkRepeater
                    objectName: "networkDiagnosticsCheckRepeater"
                    model: controller ? controller.checks : []

                    delegate: AbstractButton {
                        id: checkCard
                        required property var modelData
                        objectName: "networkDiagnosticsCheckCard_" + modelData.id
                        Layout.fillWidth: true
                        Layout.preferredHeight: 82
                        leftPadding: 16
                        rightPadding: 16
                        topPadding: 12
                        bottomPadding: 12
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: (modelData.title || "Проверка") + ". Открыть подробности"
                        onClicked: root.openCheckDetails(modelData)

                        contentItem: RowLayout {
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                Layout.alignment: Qt.AlignVCenter
                                radius: 18
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
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 3

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
                                        visible: !root.narrowLayout
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
                            }

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                text: "\uE76C"
                                color: checkCard.hovered || checkCard.activeFocus
                                    ? FluentTheme.accent : FluentTheme.textSecondary
                                font.family: "Segoe MDL2 Assets"
                                font.pixelSize: 13
                            }
                        }

                        background: Rectangle {
                            radius: 8
                            color: root.selectedCheck.id === checkCard.modelData.id
                                && detailPane.opened ? FluentTheme.selected
                                : checkCard.pressed ? FluentTheme.selected
                                : checkCard.hovered ? FluentTheme.hover : FluentTheme.surface
                            border.width: root.selectedCheck.id === checkCard.modelData.id
                                && detailPane.opened ? 2
                                : checkCard.hovered || checkCard.activeFocus ? 2 : 1
                            border.color: root.selectedCheck.id === checkCard.modelData.id
                                && detailPane.opened ? FluentTheme.accent
                                : checkCard.hovered || checkCard.activeFocus
                                  ? FluentTheme.accent : FluentTheme.stroke
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

    Popup {
        id: detailPane
        objectName: "networkDiagnosticsDetailPane"
        parent: root
        x: root.width - width
        y: 0
        width: root.detailDocked ? 420 : Math.min(540, root.width)
        height: root.height
        modal: !root.detailDocked
        dim: !root.detailDocked
        padding: 0
        closePolicy: Popup.CloseOnEscape
            | (root.detailDocked ? Popup.NoAutoClose : Popup.CloseOnPressOutside)
        enter: Transition {
            NumberAnimation {
                property: "x"
                from: root.detailDocked ? root.width - detailPane.width : root.width
                duration: root.detailDocked ? 0 : 170
                easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "x"
                to: root.detailDocked ? root.width - detailPane.width : root.width
                duration: root.detailDocked ? 0 : 130
                easing.type: Easing.InCubic
            }
        }
        Overlay.modal: Rectangle { color: root.detailDocked ? "transparent" : "#52000000" }

        background: Rectangle {
            color: FluentTheme.canvas
            border.width: 1
            border.color: FluentTheme.stroke
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: FluentTheme.surface
                border.width: 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 14
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: 19
                        color: root.statusTint(root.selectedCheck.status || "unavailable")
                        Text {
                            anchors.centerIn: parent
                            text: root.statusGlyph(root.selectedCheck.status || "unavailable")
                            color: root.statusColor(root.selectedCheck.status || "unavailable")
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 16
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        FluentText {
                            id: detailTitle
                            objectName: "networkDiagnosticsDetailTitle"
                            Layout.fillWidth: true
                            text: root.selectedCheck.title || "Подробности проверки"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        FluentText {
                            text: root.statusTitle(root.selectedCheck.status || "unavailable")
                            color: root.statusColor(root.selectedCheck.status || "unavailable")
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }

                    AbstractButton {
                        id: detailCloseButton
                        objectName: "networkDiagnosticsDetailCloseButton"
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        Accessible.name: "Закрыть подробности"
                        onClicked: detailPane.close()
                        contentItem: Text {
                            text: "\uE8BB"
                            color: FluentTheme.textPrimary
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: detailCloseButton.pressed ? FluentTheme.selected
                                : detailCloseButton.hovered ? FluentTheme.hover : "transparent"
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: FluentTheme.stroke
            }

            ScrollView {
                id: detailScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: detailScroll.availableWidth
                    spacing: 14

                    Item { Layout.preferredHeight: 4 }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        implicitHeight: summaryText.implicitHeight + 28
                        radius: 8
                        color: FluentTheme.surface
                        border.width: 1
                        border.color: FluentTheme.stroke
                        FluentText {
                            id: summaryText
                            anchors.fill: parent
                            anchors.margins: 14
                            text: root.selectedCheck.summary || "Подробное описание отсутствует."
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        spacing: 8
                        visible: !!(root.selectedCheck.details
                            && root.selectedCheck.details.length > 0)

                        FluentText {
                            text: "Собранные данные"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            id: detailList
                            objectName: "networkDiagnosticsDetailList"
                            model: root.selectedCheck.details || []
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: detailLine.implicitHeight + 22
                                radius: 7
                                color: FluentTheme.surface
                                border.width: 1
                                border.color: FluentTheme.stroke
                                FluentText {
                                    id: detailLine
                                    anchors.fill: parent
                                    anchors.margins: 11
                                    text: String(modelData)
                                    color: FluentTheme.textSecondary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 12
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        spacing: 8
                        visible: root.selectedCheck.id === "firewall"
                            && root.selectedCheck.items
                            && root.selectedCheck.items.length > 0

                        FluentText {
                            text: "Найденные правила"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            id: firewallRuleRepeater
                            objectName: "firewallRuleRepeater"
                            model: root.selectedCheck.items || []
                            delegate: Rectangle {
                                id: ruleCard
                                required property var modelData
                                required property int index
                                property var rule: modelData
                                property bool removed: root.ruleIsRemoved(rule.key)
                                Layout.fillWidth: true
                                implicitHeight: ruleLayout.implicitHeight + 28
                                radius: 9
                                color: FluentTheme.surface
                                border.width: 1
                                border.color: removed ? "#A7D7AA" : FluentTheme.stroke

                                ColumnLayout {
                                    id: ruleLayout
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    FluentText {
                                        Layout.fillWidth: true
                                        text: ruleCard.rule.title || ruleCard.rule.key
                                        color: FluentTheme.textPrimary
                                        font.family: FluentTheme.fontFamily
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.WordWrap
                                    }
                                    FluentText {
                                        Layout.fillWidth: true
                                        text: ruleCard.rule.description || "Описание правила отсутствует."
                                        color: FluentTheme.textSecondary
                                        font.family: FluentTheme.fontFamily
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    Repeater {
                                        model: [
                                            { label: "Внутреннее имя", value: ruleCard.rule.key },
                                            { label: "Профиль", value: ruleCard.rule.profile },
                                            { label: "Программа", value: ruleCard.rule.program },
                                            { label: "Служба", value: ruleCard.rule.service },
                                            { label: "Протокол", value: ruleCard.rule.protocol },
                                            { label: "Локальный порт", value: ruleCard.rule.local_port },
                                            { label: "Удалённый порт", value: ruleCard.rule.remote_port },
                                            { label: "Локальный адрес", value: ruleCard.rule.local_address },
                                            { label: "Удалённый адрес", value: ruleCard.rule.remote_address }
                                        ]
                                        delegate: RowLayout {
                                            required property var modelData
                                            visible: modelData.value !== undefined
                                                && String(modelData.value).length > 0
                                            Layout.fillWidth: true
                                            spacing: 8
                                            FluentText {
                                                Layout.preferredWidth: 112
                                                text: modelData.label
                                                color: FluentTheme.textSecondary
                                                font.family: FluentTheme.fontFamily
                                                font.pixelSize: 11
                                            }
                                            FluentText {
                                                Layout.fillWidth: true
                                                text: modelData.value || ""
                                                color: FluentTheme.textPrimary
                                                font.family: FluentTheme.fontFamily
                                                font.pixelSize: 11
                                                wrapMode: Text.WrapAnywhere
                                            }
                                        }
                                    }

                                    Button {
                                        id: removeRuleButton
                                        objectName: "firewallRuleRemoveButton"
                                        Layout.alignment: Qt.AlignRight
                                        Layout.preferredHeight: 36
                                        enabled: controller && !controller.busy && !ruleCard.removed
                                        text: ruleCard.removed ? "Удалено"
                                            : controller && controller.busy
                                              && controller.activeRuleKey === ruleCard.rule.key
                                                ? "Удаляется…" : "Удалить правило"
                                        onClicked: {
                                            root.pendingRule = ruleCard.rule
                                            confirmationPopup.open()
                                        }
                                        contentItem: FluentText {
                                            text: removeRuleButton.text
                                            color: removeRuleButton.enabled ? "white" : FluentTheme.disabledText
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            implicitWidth: 142
                                            radius: 6
                                            color: removeRuleButton.enabled
                                                ? (removeRuleButton.pressed ? "#8E1A10"
                                                   : removeRuleButton.hovered ? "#B42318" : "#C42B1C")
                                                : FluentTheme.disabledSurface
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        visible: controller && controller.actionState !== "idle"
                        implicitHeight: actionText.implicitHeight + 26
                        radius: 8
                        color: controller && controller.actionState === "succeeded"
                            ? "#E8F5EA" : controller && controller.actionState === "failed"
                              ? "#FDE7E9" : FluentTheme.surfaceInset
                        border.width: 1
                        border.color: controller && controller.actionState === "succeeded"
                            ? "#A7D7AA" : controller && controller.actionState === "failed"
                              ? "#E8A5AA" : FluentTheme.stroke
                        FluentText {
                            id: actionText
                            anchors.fill: parent
                            anchors.margins: 13
                            text: controller && controller.actionMessage
                                ? controller.actionMessage : ""
                            color: controller && controller.actionState === "failed"
                                ? "#C42B1C" : FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    FluentText {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        visible: controller && controller.actionState === "succeeded"
                        text: "Повторите диагностику, чтобы обновить итоговый список."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.preferredHeight: 8 }
                }
            }
        }
    }

    Popup {
        id: confirmationPopup
        objectName: "firewallRuleConfirmation"
        parent: root
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: Math.min(430, root.width - 32)
        height: confirmationLayout.implicitHeight + 36
        modal: true
        dim: true
        padding: 18
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "#66000000" }
        background: Rectangle {
            radius: 10
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke
        }

        contentItem: ColumnLayout {
            id: confirmationLayout
            spacing: 12
            FluentText {
                Layout.fillWidth: true
                text: "Удалить правило Windows Firewall?"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            FluentText {
                id: confirmationText
                objectName: "firewallRuleConfirmationText"
                Layout.fillWidth: true
                text: "Будет удалено правило «" + (root.pendingRule.title || "Без названия")
                    + "».\nВнутреннее имя: " + (root.pendingRule.key || "")
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
            }
            FluentText {
                Layout.fillWidth: true
                text: "Перед удалением текущая политика будет экспортирована в резервный файл."
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    id: cancelRemovalButton
                    text: "Отмена"
                    onClicked: confirmationPopup.close()
                    background: Rectangle {
                        implicitWidth: 92
                        implicitHeight: 36
                        radius: 6
                        color: cancelRemovalButton.pressed ? FluentTheme.selected
                            : cancelRemovalButton.hovered ? FluentTheme.hover : FluentTheme.surface
                        border.width: 1
                        border.color: FluentTheme.stroke
                    }
                }
                Button {
                    id: confirmRemovalButton
                    objectName: "firewallRuleConfirmButton"
                    text: "Удалить"
                    onClicked: {
                        if (controller && root.pendingRule.key)
                            controller.removeFirewallRule(root.pendingRule.key)
                        confirmationPopup.close()
                    }
                    contentItem: FluentText {
                        text: confirmRemovalButton.text
                        color: "white"
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitWidth: 98
                        implicitHeight: 36
                        radius: 6
                        color: confirmRemovalButton.pressed ? "#8E1A10"
                            : confirmRemovalButton.hovered ? "#B42318" : "#C42B1C"
                    }
                }
            }
        }
    }
}
