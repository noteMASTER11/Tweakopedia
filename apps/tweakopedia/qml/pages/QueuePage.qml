import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    signal returnToTweaksRequested()
    property bool wideLayout: width >= 900
    readonly property bool applying: controller.applyStatus === "running"
    readonly property bool showSuccess: controller.applyStatus === "succeeded"
        && controller.queue.count === 0

    function generatedPackageName() {
        return "Изменения Windows — " + Qt.formatDateTime(new Date(), "dd.MM.yyyy HH:mm")
    }

    function showExplanation(tweakId, trigger) {
        infoPane.show(controller.openExplanation(tweakId), trigger)
    }

    padding: 24
    background: Rectangle { objectName: "queueBackground"; color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: infoPane.opened && infoPane.docked ? infoPane.width : 0
        spacing: 12
        visible: !root.showSuccess

        FluentText {
            text: "Очередь"
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        FluentText {
            text: "Проверьте выбранные изменения и примените их одним пакетом."
            color: FluentTheme.textSecondary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 13
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            FluentText {
                objectName: "queueEmptyState"
                anchors.centerIn: parent
                visible: queueList.count === 0
                text: "Очередь пуста. Выберите состояния на странице «Твики»."
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
            }

            ListView {
                id: queueList
                anchors.fill: parent
                clip: true
                spacing: 8
                model: root.controller.queue

                delegate: Rectangle {
                    id: queueItemCard
                    property var inputItems: model.inputs || []
                    objectName: "queueItemCard"
                    width: ListView.view.width
                    height: Math.max(64, queueItemLayout.implicitHeight + 28)
                    radius: 8
                    color: FluentTheme.surface
                    border.color: queueItemHover.hovered ? FluentTheme.accent : FluentTheme.stroke
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Открыть справку: " + model.title

                    function openExplanation() {
                        forceActiveFocus()
                        root.showExplanation(model.id, queueItemCard)
                    }

                    function displayInputValue(input) {
                        if (input.type === "boolean") return input.value ? "Да" : "Нет"
                        if (input.type === "file" && input.value && input.value.managedPath)
                            return input.value.managedPath
                        return String(input.value)
                    }

                    Keys.onReturnPressed: event => {
                        openExplanation()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: event => {
                        openExplanation()
                        event.accepted = true
                    }
                    Keys.onSpacePressed: event => {
                        openExplanation()
                        event.accepted = true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: queueItemCard.openExplanation()
                    }

                    HoverHandler { id: queueItemHover }

                    RowLayout {
                        id: queueItemLayout
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            FluentText {
                                Layout.fillWidth: true
                                text: model.title + "   " + model.currentState + " → " + model.targetState
                                color: FluentTheme.textPrimary
                                font.family: FluentTheme.fontFamily
                                elide: Text.ElideRight
                            }

                            Repeater {
                                model: queueItemCard.inputItems
                                delegate: FluentText {
                                    required property var modelData
                                    objectName: "queueInputSummary"
                                    Layout.fillWidth: true
                                    text: modelData.label + ": " + queueItemCard.displayInputValue(modelData)
                                    color: FluentTheme.textSecondary
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 12
                                    elide: Text.ElideMiddle
                                }
                            }
                        }

                        Button {
                            id: cancelButton
                            objectName: "cancelQueueItemButton"
                            text: "Отменить"
                            hoverEnabled: true
                            Accessible.name: text + " изменение «" + model.title + "»"
                            Layout.preferredWidth: 112
                            onClicked: root.controller.removeFromQueue(model.id)

                            contentItem: FluentText {
                                text: cancelButton.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.family: FluentTheme.fontFamily
                                font.weight: Font.DemiBold
                            }
                            background: Rectangle {
                                objectName: "cancelQueueItemBackground"
                                implicitHeight: 36
                                radius: 6
                                color: cancelButton.hovered
                                    ? FluentTheme.dangerHover : FluentTheme.danger
                            }
                        }
                    }
                }
            }
        }

        FluentText {
            objectName: "applyErrorMessage"
            Layout.fillWidth: true
            visible: root.controller.applyStatus === "failed"
                || root.controller.applyStatus === "cancelled"
            text: root.controller.applyMessage
            color: FluentTheme.danger
            wrapMode: Text.WordWrap
            font.family: FluentTheme.fontFamily
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 40 : 0
            visible: queueList.count > 0
            spacing: 10

            Item { Layout.fillWidth: true }

            BusyIndicator {
                objectName: "applySpinner"
                visible: root.applying
                running: root.applying
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                palette.dark: FluentTheme.accent
            }

            Button {
                id: applyButton
                objectName: "applyQueueButton"
                visible: queueList.count > 0
                enabled: !root.applying
                text: "Применить"
                hoverEnabled: true
                Accessible.name: text
                Layout.preferredWidth: 140
                onClicked: root.controller.applyQueue(root.generatedPackageName())

                contentItem: FluentText {
                    text: applyButton.text
                    color: applyButton.enabled ? "white" : FluentTheme.disabledText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: FluentTheme.fontFamily
                    font.weight: Font.DemiBold
                }
                background: Rectangle {
                    objectName: "applyQueueButtonBackground"
                    implicitHeight: 38
                    radius: 6
                    color: !applyButton.enabled
                        ? FluentTheme.disabledSurface
                        : applyButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
                }
            }
        }
    }

    Item {
        objectName: "applySuccessView"
        anchors.fill: parent
        visible: root.showSuccess

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 620)
            spacing: 14

            FluentText {
                objectName: "applySuccessCheck"
                Layout.alignment: Qt.AlignHCenter
                text: "✓"
                color: FluentTheme.stateOn
                font.family: FluentTheme.fontFamily
                font.pixelSize: 84
                font.weight: Font.DemiBold
            }

            FluentText {
                objectName: "applySuccessTitle"
                Layout.alignment: Qt.AlignHCenter
                text: "Настройки применены"
                color: FluentTheme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                font.family: FluentTheme.fontFamily
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            FluentText {
                objectName: "rebootRequirementText"
                Layout.fillWidth: true
                visible: root.controller.rebootRequired
                text: "Некоторые из применённых настроек требуют перезагрузки ПК"
                color: FluentTheme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 14
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Button {
                    id: returnButton
                    objectName: "returnToTweaksButton"
                    text: "Вернуться к твикам"
                    hoverEnabled: true
                    Accessible.name: text
                    Layout.preferredWidth: 190
                    onClicked: root.returnToTweaksRequested()

                    contentItem: FluentText {
                        text: returnButton.text
                        color: FluentTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: FluentTheme.fontFamily
                        font.weight: Font.DemiBold
                    }
                    background: Rectangle {
                        implicitHeight: 38
                        radius: 6
                        color: returnButton.hovered
                            ? FluentTheme.hover : FluentTheme.surface
                        border.color: returnButton.hovered
                            ? FluentTheme.accent : FluentTheme.stroke
                    }
                }

                Button {
                    id: restartButton
                    objectName: "restartComputerButton"
                    visible: root.controller.rebootRequired
                    text: "Перезагрузить"
                    hoverEnabled: true
                    Accessible.name: text
                    Layout.preferredWidth: visible ? 160 : 0
                    onClicked: root.controller.restartComputer()

                    contentItem: FluentText {
                        text: restartButton.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: FluentTheme.fontFamily
                        font.weight: Font.DemiBold
                    }
                    background: Rectangle {
                        implicitHeight: 38
                        radius: 6
                        color: restartButton.hovered
                            ? FluentTheme.accentHover : FluentTheme.accent
                    }
                }
            }
        }
    }

    InfoPane {
        id: infoPane
        objectName: "queueInfoPane"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 420
        docked: root.wideLayout
    }
}
