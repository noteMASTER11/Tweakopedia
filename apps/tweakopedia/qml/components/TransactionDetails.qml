import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property string transactionId: ""
    property string packageName: ""
    property string status: ""
    property string error: ""
    property bool canRollback: false
    property var updatedAt
    property var operations: []
    property bool detailsAvailable: false
    property bool expanded: false
    signal rollbackRequested(string transactionId)

    readonly property string displayPackageName: packageName.replace(
        /\s+—\s+\d{2}\.\d{2}\.\d{4}\s+\d{2}:\d{2}$/, "")

    readonly property string statusTitle: {
        switch (status) {
        case "pending": return "Ожидает выполнения"
        case "running": return "Выполняется"
        case "succeeded": return "Применено"
        case "failed": return "Ошибка"
        case "rolled_back": return "Исходное восстановлено"
        case "interrupted": return "Выполнение прервано"
        default: return status
        }
    }
    readonly property color statusColor: {
        switch (status) {
        case "succeeded": return FluentTheme.stateOn
        case "failed": return FluentTheme.stateOff
        case "interrupted": return "#9A6700"
        case "rolled_back": return FluentTheme.accent
        default: return FluentTheme.textSecondary
        }
    }
    readonly property color statusSurface: {
        switch (status) {
        case "succeeded": return "#E8F3E8"
        case "failed": return "#FDE7E9"
        case "interrupted": return "#FFF4CE"
        case "rolled_back": return "#EAF3FF"
        default: return FluentTheme.surfaceInset
        }
    }
    readonly property string statusGlyph: {
        switch (status) {
        case "succeeded": return "\uE73E"
        case "failed": return "\uE783"
        case "rolled_back": return "\uE7A7"
        case "interrupted": return "\uE814"
        default: return "\uE895"
        }
    }

    function operationCountTitle(count) {
        const mod10 = count % 10
        const mod100 = count % 100
        if (mod10 === 1 && mod100 !== 11)
            return count + " изменение"
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
            return count + " изменения"
        return count + " изменений"
    }

    function dateTitle(value) {
        if (!value)
            return ""
        const date = new Date(value)
        if (isNaN(date.getTime()))
            return ""
        return Qt.formatDateTime(date, "dd.MM.yyyy · HH:mm")
    }

    objectName: "transactionDetails_" + transactionId
    implicitHeight: content.implicitHeight + 28
    radius: 10
    color: FluentTheme.surface
    border.width: root.expanded ? 2 : 1
    border.color: root.expanded ? FluentTheme.accent : FluentTheme.stroke

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            AbstractButton {
                id: headerButton
                objectName: "transactionHeader"
                Layout.fillWidth: true
                implicitHeight: 54
                focusPolicy: Qt.StrongFocus
                Accessible.name: (root.expanded ? "Свернуть пакет: " : "Раскрыть пакет: ")
                    + root.packageName
                onClicked: root.expanded = !root.expanded

                contentItem: RowLayout {
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 18
                        color: root.statusSurface

                        Text {
                            anchors.centerIn: parent
                            text: root.statusGlyph
                            color: root.statusColor
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 15
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        FluentText {
                            objectName: "transactionPackageTitle"
                            Layout.fillWidth: true
                            text: root.displayPackageName
                            color: FluentTheme.textPrimary
                            elide: Text.ElideRight
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            spacing: 8

                            FluentText {
                                text: root.dateTitle(root.updatedAt)
                                visible: text.length > 0
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                            }

                            FluentText {
                                visible: root.operations.length > 0
                                text: "·"
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                            }

                            FluentText {
                                visible: root.operations.length > 0
                                text: root.operationCountTitle(root.operations.length)
                                color: FluentTheme.textSecondary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 12
                            }
                        }
                    }

                    Rectangle {
                        implicitWidth: statusText.implicitWidth + 18
                        implicitHeight: 26
                        radius: 13
                        color: root.statusSurface

                        FluentText {
                            id: statusText
                            objectName: "transactionStatusText"
                            anchors.centerIn: parent
                            text: root.statusTitle
                            color: root.statusColor
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        id: chevron
                        objectName: "transactionChevron"
                        Layout.preferredWidth: 24
                        text: root.expanded ? "\uE96D" : "\uE96E"
                        color: FluentTheme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 12
                    }
                }

                background: Rectangle {
                    radius: 7
                    color: headerButton.down
                        ? FluentTheme.selected
                        : (headerButton.hovered || headerButton.activeFocus
                            ? FluentTheme.hover : "transparent")
                    border.width: headerButton.activeFocus ? 1 : 0
                    border.color: FluentTheme.accent
                }
            }

            Button {
                id: rollbackButton
                objectName: "rollbackButton"
                enabled: root.canRollback
                leftPadding: 13
                rightPadding: 13
                topPadding: 9
                bottomPadding: 9
                Accessible.name: "Вернуть исходные значения пакета " + root.packageName
                onClicked: root.rollbackRequested(root.transactionId)

                contentItem: RowLayout {
                    spacing: 7

                    Text {
                        text: "\uE7A7"
                        color: rollbackButton.enabled
                            ? FluentTheme.accent : FluentTheme.disabledText
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 13
                    }

                    FluentText {
                        text: "Вернуть исходное"
                        color: rollbackButton.enabled
                            ? FluentTheme.accent : FluentTheme.disabledText
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                background: Rectangle {
                    radius: 6
                    color: rollbackButton.enabled
                        ? (rollbackButton.down
                            ? FluentTheme.selected
                            : (rollbackButton.hovered ? FluentTheme.hover : FluentTheme.surface))
                        : FluentTheme.disabledSurface
                    border.width: 1
                    border.color: rollbackButton.enabled
                        ? FluentTheme.accent : FluentTheme.stroke
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.error.length > 0
            implicitHeight: errorRow.implicitHeight + 18
            radius: 7
            color: "#FDE7E9"

            RowLayout {
                id: errorRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: "\uE783"
                    color: FluentTheme.stateOff
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                }

                FluentText {
                    objectName: "transactionError"
                    Layout.fillWidth: true
                    text: root.error
                    color: FluentTheme.stateOff
                    wrapMode: Text.WordWrap
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.expanded
            implicitHeight: 1
            color: FluentTheme.stroke
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.expanded
            spacing: 8

            FluentText {
                Layout.fillWidth: true
                visible: !root.detailsAvailable
                text: "Подробности этой транзакции отсутствуют в локальном журнале."
                color: FluentTheme.textSecondary
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }

            FluentText {
                Layout.fillWidth: true
                visible: root.detailsAvailable && root.operations.length === 0
                text: "В пакете не зарегистрировано отдельных изменений."
                color: FluentTheme.textSecondary
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }

            Repeater {
                model: root.operations

                delegate: Rectangle {
                    id: operationCard
                    required property int index
                    required property var modelData
                    objectName: "transactionOperation_" + index
                    Layout.fillWidth: true
                    visible: root.expanded
                    implicitHeight: visible ? operationContent.implicitHeight + 22 : 0
                    radius: 8
                    color: FluentTheme.surfaceInset
                    border.width: 1
                    border.color: FluentTheme.stroke

                    ColumnLayout {
                        id: operationContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 11
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluentText {
                                objectName: "transactionOperationTitle_" + operationCard.index
                                Layout.fillWidth: true
                                text: operationCard.modelData.title || "Изменение Windows"
                                color: FluentTheme.textPrimary
                                wrapMode: Text.WordWrap
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Rectangle {
                                implicitWidth: kindText.implicitWidth + 14
                                implicitHeight: 22
                                radius: 11
                                color: "#EAF3FF"

                                FluentText {
                                    id: kindText
                                    anchors.centerIn: parent
                                    text: operationCard.modelData.kind || "Операция"
                                    color: FluentTheme.accent
                                    font.family: FluentTheme.fontFamily
                                    font.pixelSize: 11
                                }
                            }
                        }

                        FluentText {
                            objectName: "transactionOperationTransition_" + operationCard.index
                            Layout.fillWidth: true
                            text: (operationCard.modelData.before || "Неизвестно")
                                + "  →  " + (operationCard.modelData.after || "Неизвестно")
                            color: FluentTheme.textPrimary
                            wrapMode: Text.WordWrap
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                        }

                        TextEdit {
                            Layout.fillWidth: true
                            text: operationCard.modelData.object || ""
                            readOnly: true
                            selectByMouse: true
                            color: FluentTheme.textSecondary
                            wrapMode: TextEdit.WrapAnywhere
                            font.family: "Consolas"
                            font.pixelSize: 11
                        }

                        FluentText {
                            Layout.fillWidth: true
                            text: "Перезапуск: " + (operationCard.modelData.restart || "Не требуется")
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
