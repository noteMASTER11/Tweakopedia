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
    signal articleRequested(string id)

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

    header: ToolPageHeader {
        objectName: "fileSystemCheckHeader"
        title: "Проверка файловой системы"
        subtitle: "Диагностика локальных томов NTFS средствами CHKDSK без исправления ошибок."
        backButtonObjectName: "fileSystemCheckBackButton"
        onBackRequested: root.backRequested()
    }

    footer: Item {
        implicitHeight: fileSystemCheckResultGuide.implicitHeight + 20

        Rectangle {
            anchors.fill: parent
            color: FluentTheme.canvas
        }

        ToolResultGuide {
            id: fileSystemCheckResultGuide
            objectName: "fileSystemCheckResultGuide"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            title: "Если CHKDSK обнаружил ошибки"
            description: "Проверка в Tweakopedia работает только на чтение. Найденные ошибки означают несогласованность структуры NTFS либо проблемы чтения накопителя; приложение ничего не исправляет автоматически."
            actions: [
                "Сначала сохраните важные файлы с выбранного тома на другой накопитель.",
                "Для исправления логических ошибок запустите chkdsk X: /f; системный том Windows предложит проверить при перезагрузке.",
                "При сообщениях о повреждённых секторах или повторяющихся ошибках проверьте состояние накопителя и не откладывайте его замену."
            ]
            articleId: "guide.file-system-check"
            onArticleRequested: id => root.articleRequested(id)
        }
    }

    ScrollView {
        id: fileSystemCheckScroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Item {
            width: fileSystemCheckScroll.availableWidth
            implicitHeight: fileSystemCheckContent.implicitHeight + 46

            ColumnLayout {
                id: fileSystemCheckContent
                x: 28
                y: 22
                width: parent.width - 56
                spacing: 14

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
                        color: toolState === "healthy" ? FluentTheme.positive
                            : toolState === "issues_found" || toolState === "incomplete" || toolState === "failed"
                                ? FluentTheme.danger : FluentTheme.textPrimary
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

        LiveCommandOutput {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            title: "Ход выполнения CHKDSK"
            outputText: controller ? controller.outputText : ""
            placeholderText: "Здесь появится поток вывода CHKDSK."
            terminalObjectName: "fileSystemCheckTerminal"
            copyButtonObjectName: "fileSystemCheckCopyButton"
            logButtonObjectName: "fileSystemCheckOpenLogButton"
            logButtonText: "Открыть журнал CHKDSK"
            busy: controller && controller.busy
            logEnabled: controller && controller.sessionLogPath.length > 0
            onCopyRequested: if (controller) controller.copyOutput()
            onOpenLogRequested: if (controller) controller.openSessionLog()
        }

                Item { Layout.preferredHeight: 2 }
            }
        }
    }
}
