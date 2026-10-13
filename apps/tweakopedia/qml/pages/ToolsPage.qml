import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    property int selectedToolIndex: -1
    property var integrityController
    property var fileSystemController
    property var networkController
    readonly property bool wideLayout: width >= 820
    padding: 0

    background: Rectangle {
        objectName: "toolsBackground"
        color: FluentTheme.canvas
    }

    ListModel {
        id: toolsModel

        ListElement {
            title: "Проверка целостности системы"
            description: "Проверяет хранилище компонентов Windows средствами DISM."
            badge: "DISM"
            glyph: "\uE950"
            accent: "#0067C0"
            tint: "#EAF3FF"
        }
        ListElement {
            title: "Проверка файловой системы"
            description: "Проверяет выбранные накопители средствами CHKDSK без исправления ошибок."
            badge: "CHKDSK · Только проверка"
            glyph: "\uEDA2"
            accent: "#107C10"
            tint: "#E8F5EA"
        }
        ListElement {
            title: "Диагностика сети"
            description: "Анализирует адаптеры, DNS, прокси, маршруты и правила брандмауэра."
            badge: "Без изменений"
            glyph: "\uE968"
            accent: "#5C2D91"
            tint: "#F1ECFB"
        }
        ListElement {
            title: "Восстановление сети"
            description: "Подготавливает выбранные операции очистки и сброса сетевых параметров."
            badge: "С подтверждением"
            glyph: "\uE777"
            accent: "#CA5010"
            tint: "#FFF0E5"
        }
        ListElement {
            title: "Блокировка телеметрии"
            description: "Настраивает сетевую блокировку и связанные параметры Windows."
            badge: "Профили совместимости"
            glyph: "\uEA18"
            accent: "#C42B1C"
            tint: "#FDEBED"
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.selectedToolIndex < 0 ? 0
            : root.selectedToolIndex === 0 ? 1
            : root.selectedToolIndex === 1 ? 2
            : root.selectedToolIndex === 2 ? 3 : 4

        Item {
            id: overview
            objectName: "toolsOverview"

            ScrollView {
                id: overviewScroll
                anchors.fill: parent
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: overviewScroll.availableWidth
                    spacing: 8

                    Item { Layout.preferredHeight: 16 }

                    FluentText {
                        objectName: "toolsTitle"
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        text: "Инструменты"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }

                    FluentText {
                        objectName: "toolsSubtitle"
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        text: "Проверка состояния Windows и встроенные средства обслуживания."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                    }

                    Item { Layout.preferredHeight: 12 }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        columns: root.wideLayout ? 2 : 1
                        columnSpacing: 14
                        rowSpacing: 14

                        Repeater {
                            id: cardRepeater
                            objectName: "toolCardRepeater"
                            model: toolsModel

                            delegate: AbstractButton {
                                id: card

                                required property int index
                                required property string title
                                required property string description
                                required property string badge
                                required property string glyph
                                required property string accent
                                required property string tint

                                objectName: "toolCard_" + index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 164
                                focusPolicy: Qt.StrongFocus
                                Accessible.role: Accessible.Button
                                Accessible.name: title
                                onClicked: root.selectedToolIndex = index

                                contentItem: RowLayout {
                                    spacing: 18

                                    Rectangle {
                                        Layout.preferredWidth: 72
                                        Layout.preferredHeight: 72
                                        Layout.leftMargin: 22
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 36
                                        color: card.tint

                                        Text {
                                            anchors.centerIn: parent
                                            text: card.glyph
                                            color: card.accent
                                            font.family: "Segoe MDL2 Assets"
                                            font.pixelSize: 30
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 7

                                        FluentText {
                                            Layout.fillWidth: true
                                            text: card.title
                                            color: FluentTheme.textPrimary
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 17
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.WordWrap
                                        }

                                        FluentText {
                                            Layout.fillWidth: true
                                            text: card.description
                                            color: FluentTheme.textSecondary
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 13
                                            lineHeight: 1.2
                                            wrapMode: Text.WordWrap
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: badgeText.implicitWidth + 22
                                            Layout.preferredHeight: 28
                                            radius: 6
                                            color: FluentTheme.surfaceInset

                                            FluentText {
                                                id: badgeText
                                                anchors.centerIn: parent
                                                text: card.badge
                                                color: FluentTheme.textSecondary
                                                font.family: FluentTheme.fontFamily
                                                font.pixelSize: 12
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.preferredWidth: 22
                                        Layout.rightMargin: 20
                                        Layout.alignment: Qt.AlignVCenter
                                        text: "\uE76C"
                                        color: card.hovered || card.activeFocus
                                            ? FluentTheme.accent : FluentTheme.textSecondary
                                        font.family: "Segoe MDL2 Assets"
                                        font.pixelSize: 17
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                background: Rectangle {
                                    radius: 10
                                    color: card.pressed
                                        ? FluentTheme.selected
                                        : card.hovered ? FluentTheme.hover : FluentTheme.surface
                                    border.width: card.hovered || card.activeFocus ? 2 : 1
                                    border.color: card.hovered || card.activeFocus
                                        ? FluentTheme.accent : FluentTheme.stroke
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 24 }
                }
            }
        }

        SystemIntegrityPage {
            controller: root.integrityController
            onBackRequested: root.selectedToolIndex = -1
        }

        FileSystemCheckPage {
            controller: root.fileSystemController
            onBackRequested: root.selectedToolIndex = -1
        }

        NetworkDiagnosticsPage {
            controller: root.networkController
            onBackRequested: root.selectedToolIndex = -1
            onRecoveryRequested: root.selectedToolIndex = 3
        }

        Item {
            id: placeholder
            objectName: "toolPlaceholder"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 12

                AbstractButton {
                    id: backButton
                    objectName: "toolPlaceholderBackButton"
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 38
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: "Вернуться к инструментам"
                    onClicked: root.selectedToolIndex = -1

                    contentItem: Row {
                        anchors.centerIn: parent
                        spacing: 9

                        Text {
                            text: "\uE72B"
                            color: FluentTheme.textPrimary
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 15
                        }
                        FluentText {
                            text: "Назад"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                        }
                    }

                    background: Rectangle {
                        radius: 6
                        color: backButton.pressed
                            ? FluentTheme.selected
                            : backButton.hovered ? FluentTheme.hover : FluentTheme.surface
                        border.width: 1
                        border.color: backButton.activeFocus ? FluentTheme.accent : FluentTheme.stroke
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 92
                    Layout.alignment: Qt.AlignHCenter
                    radius: 46
                    color: root.selectedToolIndex >= 0
                        ? toolsModel.get(root.selectedToolIndex).tint : FluentTheme.selected

                    Text {
                        anchors.centerIn: parent
                        text: root.selectedToolIndex >= 0
                            ? toolsModel.get(root.selectedToolIndex).glyph : ""
                        color: root.selectedToolIndex >= 0
                            ? toolsModel.get(root.selectedToolIndex).accent : FluentTheme.accent
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 38
                    }
                }

                FluentText {
                    objectName: "toolPlaceholderTitle"
                    Layout.fillWidth: true
                    text: root.selectedToolIndex >= 0
                        ? toolsModel.get(root.selectedToolIndex).title : ""
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                FluentText {
                    objectName: "toolPlaceholderStatus"
                    Layout.fillWidth: true
                    text: "В разработке"
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
