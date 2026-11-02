import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    objectName: "systemIntegrityPage"

    property var controller
    readonly property bool wideLayout: width >= 760
    readonly property string toolState: controller ? controller.state : "idle"
    signal backRequested()
    signal articleRequested(string id)

    function primaryText() {
        if (toolState === "repairable")
            return "Восстановить"
        if (controller && controller.busy)
            return "Выполняется…"
        if (toolState === "idle")
            return "Начать проверку"
        if (toolState === "failed")
            return "Повторить"
        return "Проверить ещё раз"
    }

    function runPrimaryAction() {
        if (!controller)
            return
        if (toolState === "repairable")
            controller.startRestore()
        else
            controller.startScan()
    }

    function defaultTitle() {
        if (toolState === "awaiting_elevation")
            return "Требуется подтверждение"
        if (controller && controller.busy)
            return "Проверка выполняется"
        return "Проверка ещё не запускалась"
    }

    function defaultDescription() {
        if (toolState === "awaiting_elevation")
            return "Подтвердите запуск встроенного средства Windows от имени администратора."
        if (controller && controller.busy)
            return "Не закрывайте исполнительный процесс. Можно перейти в другой раздел программы."
        return "DISM проверит хранилище компонентов текущей установки Windows и сообщит его состояние."
    }

    padding: 0
    background: Rectangle { color: FluentTheme.canvas }

    header: ToolPageHeader {
        objectName: "systemIntegrityHeader"
        title: "Проверка целостности системы"
        subtitle: "Диагностика и восстановление хранилища компонентов средствами DISM."
        backButtonObjectName: "systemIntegrityBackButton"
        onBackRequested: root.backRequested()
    }

    footer: Item {
        implicitHeight: systemIntegrityResultGuide.implicitHeight + 20

        Rectangle {
            anchors.fill: parent
            color: FluentTheme.canvas
        }

        ToolResultGuide {
            id: systemIntegrityResultGuide
            objectName: "systemIntegrityResultGuide"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            title: "Если DISM обнаружил повреждение"
            description: "Это означает, что хранилище компонентов Windows содержит отсутствующие или повреждённые данные, используемые при обслуживании системы."
            actions: [
                "Нажмите «Восстановить»: приложение выполнит DISM /RestoreHealth и затем повторно проверит состояние хранилища.",
                "После успешного восстановления повторите проверку; при неполадках системных файлов дополнительно выполните sfc /scannow.",
                "Если восстановление не завершилось, откройте журнал DISM и проверьте код ошибки; может потребоваться источник файлов той же версии Windows."
            ]
            articleId: "guide.system-integrity"
            onArticleRequested: id => root.articleRequested(id)
        }
    }

    ScrollView {
        id: systemIntegrityScroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Item {
            width: systemIntegrityScroll.availableWidth
            implicitHeight: systemIntegrityContent.implicitHeight + 46

            ColumnLayout {
                id: systemIntegrityContent
                x: 28
                y: 22
                width: parent.width - 56
                spacing: 14

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: stateGrid.implicitHeight + 36
            radius: 10
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke

            GridLayout {
                id: stateGrid
                anchors.fill: parent
                anchors.margins: 18
                columns: root.wideLayout ? 2 : 1
                columnSpacing: 22
                rowSpacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 17
                            color: root.toolState === "healthy" ? "#E8F5EA"
                                : root.toolState === "repairable" ? "#FFF4CE"
                                : (root.toolState === "failed" || root.toolState === "non_repairable")
                                    ? "#FDE7E9" : FluentTheme.selected

                            Text {
                                anchors.centerIn: parent
                                text: root.toolState === "healthy" ? "\uE73E"
                                    : root.toolState === "repairable" ? "\uE7BA"
                                    : (root.toolState === "failed" || root.toolState === "non_repairable")
                                        ? "\uEA39" : "\uE9F5"
                                color: root.toolState === "healthy" ? "#107C10"
                                    : root.toolState === "repairable" ? "#9D5D00"
                                    : (root.toolState === "failed" || root.toolState === "non_repairable")
                                        ? "#C42B1C" : FluentTheme.accent
                                font.family: "Segoe MDL2 Assets"
                                font.pixelSize: 16
                            }
                        }

                        FluentText {
                            Layout.fillWidth: true
                            text: controller && controller.resultTitle.length > 0
                                ? controller.resultTitle : root.defaultTitle()
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }
                    }

                    FluentText {
                        Layout.fillWidth: true
                        text: controller && controller.resultDescription.length > 0
                            ? controller.resultDescription : root.defaultDescription()
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        lineHeight: 1.2
                        wrapMode: Text.WordWrap
                    }

                    FluentText {
                        id: stageText
                        objectName: "systemIntegrityStageText"
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: controller ? controller.stageText : ""
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }

                    ProgressBar {
                        id: progressBar
                        objectName: "systemIntegrityProgressBar"
                        Layout.fillWidth: true
                        visible: controller && controller.busy
                        from: 0
                        to: 100
                        value: controller && controller.progressKnown ? controller.progress : 0
                        indeterminate: controller && controller.busy && !controller.progressKnown

                        background: Rectangle {
                            implicitHeight: 5
                            radius: 2.5
                            color: FluentTheme.surfaceInset
                        }
                        contentItem: Item {
                            implicitHeight: 5
                            clip: true

                            Rectangle {
                                id: progressFill
                                height: parent.height
                                width: progressBar.indeterminate
                                    ? Math.max(42, parent.width * 0.28)
                                    : parent.width * progressBar.visualPosition
                                radius: 2.5
                                color: FluentTheme.accent
                                x: progressBar.indeterminate ? -width : 0

                                NumberAnimation on x {
                                    running: progressBar.indeterminate
                                    loops: Animation.Infinite
                                    from: -progressFill.width
                                    to: progressFill.parent ? progressFill.parent.width : 0
                                    duration: 1100
                                    easing.type: Easing.InOutCubic
                                }
                            }
                        }
                    }

                    FluentText {
                        Layout.fillWidth: true
                        visible: controller && (controller.errorCode.length > 0 || controller.exitCode >= 0)
                        text: {
                            let details = []
                            if (controller.errorCode.length > 0)
                                details.push("Код: " + controller.errorCode)
                            if (controller.exitCode >= 0)
                                details.push("Код завершения: " + controller.exitCode)
                            return details.join(" · ")
                        }
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: !root.wideLayout
                    Layout.alignment: root.wideLayout ? Qt.AlignRight | Qt.AlignVCenter : Qt.AlignLeft
                    spacing: 8

                    Button {
                        id: primaryButton
                        objectName: "systemIntegrityPrimaryButton"
                        Layout.fillWidth: !root.wideLayout
                        Layout.preferredWidth: root.wideLayout ? 178 : -1
                        text: root.primaryText()
                        enabled: controller && (root.toolState === "repairable"
                            ? controller.canRestore : controller.canScan)
                        onClicked: root.runPrimaryAction()

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
                            color: !primaryButton.enabled ? FluentTheme.surfaceInset
                                : primaryButton.pressed ? FluentTheme.accentPressed
                                : primaryButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
                            border.width: primaryButton.activeFocus ? 2 : 0
                            border.color: FluentTheme.accentPressed
                        }
                    }

                    FluentText {
                        Layout.fillWidth: true
                        Layout.preferredWidth: root.wideLayout ? 210 : -1
                        text: "Проверка может занять несколько минут."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        horizontalAlignment: root.wideLayout ? Text.AlignHCenter : Text.AlignLeft
                    }
                }
            }
        }

        LiveCommandOutput {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            title: "Ход выполнения DISM"
            outputText: controller ? controller.outputText : ""
            placeholderText: "Здесь появится поток вывода DISM."
            terminalObjectName: "systemIntegrityTerminal"
            copyButtonObjectName: "systemIntegrityCopyButton"
            logButtonObjectName: "systemIntegrityOpenLogButton"
            logButtonText: "Открыть журнал DISM"
            busy: controller && controller.busy
            logEnabled: true
            onCopyRequested: if (controller) controller.copyOutput()
            onOpenLogRequested: if (controller) controller.openDismLog()
        }

                Item { Layout.preferredHeight: 2 }
            }
        }
    }
}
