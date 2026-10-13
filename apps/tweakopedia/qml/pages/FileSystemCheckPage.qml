import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    objectName: "fileSystemCheckPage"

    property var controller
    readonly property string toolState: controller ? controller.state : "idle"
    readonly property bool narrowLayout: width < 720
    signal backRequested()

    function primaryText() {
        if (controller && controller.busy)
            return "Выполняется…"
        if (toolState === "idle" || toolState === "no_volumes")
            return "Начать проверку"
        return "Проверить ещё раз"
    }

    function defaultTitle() {
        if (toolState === "no_volumes")
            return "Подходящие тома не найдены"
        if (toolState === "awaiting_elevation")
            return "Требуется подтверждение"
        if (controller && controller.busy)
            return "Проверка выполняется"
        return "Проверка ещё не запускалась"
    }

    function defaultDescription() {
        if (toolState === "no_volumes")
            return "Для проверки нужен готовый локальный том NTFS с назначенной буквой."
        if (toolState === "awaiting_elevation")
            return "Подтвердите запуск встроенного средства Windows от имени администратора."
        if (controller && controller.busy)
            return "Вывод CHKDSK поступает в область ниже. Проверка не изменяет содержимое тома."
        return "CHKDSK проверит структуру выбранного тома в режиме только чтения."
    }

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 22
        anchors.bottomMargin: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            AbstractButton {
                id: backButton
                objectName: "fileSystemCheckBackButton"
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
                    text: "Проверка файловой системы"
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                FluentText {
                    Layout.fillWidth: true
                    text: "Диагностика локальных томов NTFS средствами CHKDSK без исправления ошибок."
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: volumeColumn.implicitHeight + 28
            radius: 10
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke

            ColumnLayout {
                id: volumeColumn
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    FluentText {
                        Layout.fillWidth: true
                        text: "Том для проверки"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Button {
                        id: refreshButton
                        objectName: "fileSystemRefreshVolumesButton"
                        text: "Обновить список"
                        enabled: controller && !controller.busy
                        onClicked: if (controller) controller.refreshVolumes()

                        contentItem: FluentText {
                            text: refreshButton.text
                            color: refreshButton.enabled ? FluentTheme.accent : FluentTheme.disabledText
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitWidth: 124
                            implicitHeight: 30
                            radius: 6
                            color: refreshButton.hovered ? FluentTheme.hover : "transparent"
                        }
                    }
                }

                ComboBox {
                    id: volumeSelector
                    objectName: "fileSystemVolumeSelector"
                    Layout.fillWidth: true
                    model: controller ? controller.volumes : []
                    textRole: "title"
                    currentIndex: controller ? controller.selectedVolumeIndex : -1
                    enabled: controller && controller.hasVolumes && !controller.busy
                    Accessible.name: "Том для проверки"
                    onActivated: function(index) {
                        if (controller) controller.setSelectedVolumeIndex(index)
                    }

                    contentItem: FluentText {
                        leftPadding: 12
                        rightPadding: 38
                        text: volumeSelector.displayText
                        color: volumeSelector.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        implicitHeight: 42
                        radius: 6
                        color: FluentTheme.surface
                        border.width: volumeSelector.activeFocus ? 2 : 1
                        border.color: volumeSelector.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                    }
                    indicator: Text {
                        x: volumeSelector.width - width - 13
                        y: (volumeSelector.height - height) / 2
                        text: "\uE70D"
                        color: FluentTheme.textSecondary
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 11
                    }
                    popup: Popup {
                        y: volumeSelector.height + 4
                        width: volumeSelector.width
                        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
                        padding: 4

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: volumeSelector.popup.visible ? volumeSelector.delegateModel : null
                            currentIndex: volumeSelector.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator {}
                        }
                        background: Rectangle {
                            radius: 7
                            color: FluentTheme.surface
                            border.width: 1
                            border.color: FluentTheme.stroke
                        }
                    }
                }

                FluentText {
                    Layout.fillWidth: true
                    text: controller && controller.selectedVolumeIndex >= 0
                        ? controller.volumes[controller.selectedVolumeIndex].details : ""
                    visible: text.length > 0
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: statusLayout.implicitHeight + 32
            radius: 10
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke

            GridLayout {
                id: statusLayout
                anchors.fill: parent
                anchors.margins: 16
                columns: root.narrowLayout ? 1 : 2
                columnSpacing: 18
                rowSpacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    FluentText {
                        id: resultTitle
                        objectName: "fileSystemCheckResultTitle"
                        Layout.fillWidth: true
                        text: controller && controller.resultTitle.length > 0
                            ? controller.resultTitle : root.defaultTitle()
                        color: toolState === "healthy" ? "#107C10"
                            : toolState === "issues_found" || toolState === "incomplete" || toolState === "failed"
                                ? "#C42B1C" : FluentTheme.textPrimary
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
                        objectName: "fileSystemCheckStageText"
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
                        objectName: "fileSystemCheckProgressBar"
                        Layout.fillWidth: true
                        visible: controller && controller.busy
                        indeterminate: visible
                    }
                }

                Button {
                    id: primaryButton
                    objectName: "fileSystemCheckPrimaryButton"
                    Layout.preferredWidth: 176
                    Layout.fillWidth: root.narrowLayout
                    Layout.preferredHeight: 40
                    Layout.alignment: root.narrowLayout ? Qt.AlignLeft : Qt.AlignRight | Qt.AlignVCenter
                    text: root.primaryText()
                    enabled: controller && controller.canCheck
                    onClicked: if (controller) controller.startCheck()

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

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 176
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
                        text: "Вывод CHKDSK"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Button {
                        id: copyButton
                        objectName: "fileSystemCheckCopyButton"
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
                        objectName: "fileSystemCheckOpenLogButton"
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

                        onContentYChanged: {
                            const maximum = Math.max(0, contentHeight - height)
                            following = maximum - contentY < 4
                        }

                        TextArea {
                            id: terminalText
                            objectName: "fileSystemCheckTerminal"
                            x: 10
                            y: 8
                            width: terminalScroll.width - 20
                            height: Math.max(terminalScroll.height - 16, implicitHeight)
                            text: controller ? controller.outputText : ""
                            placeholderText: "Здесь появятся команда и вывод CHKDSK."
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
    }
}
